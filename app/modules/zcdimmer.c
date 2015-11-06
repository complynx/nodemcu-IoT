// Module for interfacing with PWM

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"
#include "gpio.h"

extern int ets_uart_printf(char* fmt,...);

#include "c_types.h"
#include "zcdetector.h"


#define MIN(a,b) (((a)>(b))?(b):(a))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define CLIP(a,m,M) (((a)>(m))?(MIN((a),(M))):(m))
#define DECI(i,size) ((i)=(((i)==0)?((size)-1):((i)-1)))
#define INCI(i,size) ((i)=(((i)>=(size)-1)?0:((i)+1)))
#define SWAP(a,b) ((a)^=(b),(b)^=(a),(a)^=(b))

LOCAL u8 dimmer_absolute[]={
		0,14,19,24,29,32,36,39,43,44,
		48,49,53,54,58,60,61,63,65,66,
		68,71,73,75,77,78,80,82,82,83,
		85,87,88,90,92,94,95,97,97,99,
		100,102,104,105,105,107,109,111,112,114,
		114,116,117,119,121,122,124,124,126,128,
		129,131,133,134,134,136,138,139,141,143,
		145,146,148,150,151,153,155,156,158,160,
		162,163,165,168,170,172,173,177,179,182,
		184,187,190,194,197,202,207,214,223,255
};

LOCAL os_timer_t zcd_timer;
LOCAL u8 dimval=0,dimval_abs=0;
LOCAL u32 lamppin=0;
LOCAL uint32_t lamppin_bit;
LOCAL u32 dt=5;//usec to open a triac;

ZeroCrossCalculator zeroCrossCalculator={
		/*times*/{0,0,0,0,0,0,0,0,0},
		/*last_state*/2,
		/*detector_shift*/0,
		/*half period*/0,
		/*current_time*/0,
		/*last_overflow*/times_size,
		/*pin_num*/0,
		/*pin internals*/0,0,
#ifdef ZCDETECTOR_DEBUG
		/*deltas*/{0,0,0,0,0,0,0,0,0},
#endif
		/*delta inc*/0
};
#ifdef ZCDETECTOR_DEBUG
int printdebug=0,swap=0;
#endif
static void setdimval(u8 d);

u32 ZCD_getNearestDimTime(){
	u32 t0=zeroCrossCalculator.times[zeroCrossCalculator.current_time],
			T2=zeroCrossCalculator.halfperiod,ct,
			Dt=zeroCrossCalculator.detector_shift;
	size_t i;
	if(zeroCrossCalculator.last_state){
		//t0+=Dt;//see the detector tick for more info
	}else{
		//t0-=Dt;
		//This is not a good solution due to the instability of the falling edge.
		i=zeroCrossCalculator.current_time;
		t0=zeroCrossCalculator.times[DECI(i,times_size)];
		t0+=T2;
	}
	ct=system_get_time();
	if(t0>ct) t0-=T2;
	Dt=256;
	Dt-=dimval_abs;
	Dt*=T2;
	Dt>>=8;
	Dt+=t0;
	if(Dt<ct) Dt+=T2;
	return Dt-ct;//time to nearest opening
}

void ZCD_planFire();
void ZCD_delayedFire(u32 Dt){
	if(Dt<1000){//Now we need to wait us.
		os_delay_us(Dt);
	}//else -- Dt already contains next time position. Fire now.
	gpio_output_set(lamppin_bit, 0, lamppin_bit, 0);
	os_delay_us(dt);
	gpio_output_set(0,lamppin_bit, lamppin_bit, 0);
	ZCD_planFire();
}

void ZCD_planFire(){
    if(dimval){
    	os_timer_disarm(&zcd_timer);
		u32 Dt=ZCD_getNearestDimTime();
		if(Dt>1000){//use OS timer, which uses ms rather than us.
			Dt/=1000;
			os_timer_arm(&zcd_timer, Dt, 0);
		}else{
			ZCD_delayedFire(Dt);
		}
    }
}

void ZCD_tick(){
	u32 T=system_get_time();
	register u32 dt1,dt2,dtm=GPIO_INPUT_GET(zeroCrossCalculator.pin_internal_gpio);
	register size_t j;
	size_t i;
	if(dtm==zeroCrossCalculator.last_state) return;//false positive
	if(T<zeroCrossCalculator.times[zeroCrossCalculator.current_time]){
		for(i=0;i<times_size;++i){
			zeroCrossCalculator.times[i]=0;
		}
		zeroCrossCalculator.current_time=times_size;
		zeroCrossCalculator.last_overflow=times_size;
		zeroCrossCalculator.delta_inc=0;
	}
	INCI(zeroCrossCalculator.current_time,times_size);
	zeroCrossCalculator.times[zeroCrossCalculator.current_time]=T;
	zeroCrossCalculator.last_state=dtm;

	if(zeroCrossCalculator.last_overflow>0){ --zeroCrossCalculator.last_overflow;
	}else{
		i=zeroCrossCalculator.current_time;
		dt1=zeroCrossCalculator.times[i];
		dt1-=zeroCrossCalculator.times[DECI(i,times_size)];
#ifdef ZCDETECTOR_DEBUG
		INCI(zeroCrossCalculator.delta_c,times_size);
		zeroCrossCalculator.deltas[zeroCrossCalculator.delta_c]=dt1;
#endif
		dt1+=zeroCrossCalculator.delta_inc;
		if(dt1<2000){
				zeroCrossCalculator.delta_inc=dt1;
				DECI(zeroCrossCalculator.current_time,times_size);
		}else{
			if(dt1>15000){
				zeroCrossCalculator.delta_inc=0;
			}
			for(dt1=0,i=zeroCrossCalculator.current_time,j=1ul<<(times_size_b-1);j;--j){
				dt1+=zeroCrossCalculator.times[i];
				dt1-=zeroCrossCalculator.times[DECI(i,times_size)];
				DECI(i,times_size);
			}
			dt1+=zeroCrossCalculator.delta_inc;
			zeroCrossCalculator.delta_inc=0;
			dt1>>=times_size_b-1;
			for(dt2=0,i=zeroCrossCalculator.current_time,j=1ul<<(times_size_b-1);j;--j){
				dt2+=zeroCrossCalculator.times[DECI(i,times_size)];
				dt2-=zeroCrossCalculator.times[DECI(i,times_size)];
			}
			dt2>>=times_size_b-1;
			i=zeroCrossCalculator.current_time;
			if(dt1>dt2){
				//even if we know for sure which of these is bigger through "last_state" param,
				//it is safer to test and swap these two on their own.
				//last_state=0 here
				SWAP(dt1,dt2);
			}
			dtm=dt2;
			dtm-=dt1;
//			dtm>>=1;//my ZCD falls to slow, but regains voltage fast at the very edge.
			zeroCrossCalculator.detector_shift=dtm;
			dtm=dt2;
			dtm+=dt1;
			dtm>>=1;
			zeroCrossCalculator.halfperiod=dtm;
		}
	}
	ZCD_planFire();
#ifdef ZCDETECTOR_DEBUG
	if(printdebug){
		printdebug=0;
		for(i=0;i<times_size;++i){
			ets_uart_printf("%lx ",zeroCrossCalculator.times[i]);
		}
		ets_uart_printf("\n");
		for(i=0;i<times_size;++i){
			ets_uart_printf("%lu ",zeroCrossCalculator.deltas[i]);
		}
		ets_uart_printf("\nDEBUG: Dt: %lu %lu, lo: %d, s: %d\n",dt1,dt2,zeroCrossCalculator.last_overflow,zeroCrossCalculator.last_state);
		ets_uart_printf("DEBUG: T: %lx, ct: %d \n",T,zeroCrossCalculator.current_time);
		ets_uart_printf("DEBUG: T/2: %lu, shift: %lu \n",zeroCrossCalculator.halfperiod,zeroCrossCalculator.detector_shift);
	}
#endif
}

static void clearZCC(){
	zeroCrossCalculator.current_time=0;
	zeroCrossCalculator.last_overflow=times_size;
	zeroCrossCalculator.halfperiod=0;
	zeroCrossCalculator.detector_shift=0;
}

static void zcdimmer_timer_cb(){
    os_timer_disarm(&zcd_timer);
	u32 Dt=ZCD_getNearestDimTime();
	ZCD_delayedFire(Dt);
}

// Lua: realfrequency = setup( id_ZCD,id_lamp)
static int zcdimmer_lua_setup( lua_State* L )
{
	u32 id_ZCD=luaL_checkinteger( L, 1 );
	u8 pn,pf;
	u32 pm;
	lamppin=luaL_checkinteger( L, 2 );
	MOD_CHECK_ID( gpio, id_ZCD );
	MOD_CHECK_ID( gpio, lamppin );

	zeroCrossCalculator.pin_num=id_ZCD;
	pn=pin_num[id_ZCD];
	zeroCrossCalculator.pin_internal_bit=BIT(pn);
	zeroCrossCalculator.pin_internal_gpio=GPIO_ID_PIN(pn);
	pm=pin_mux[id_ZCD];
	pf=pin_func[id_ZCD];
	clearZCC();

    PIN_PULLDWN_DIS(pm);
    PIN_PULLUP_EN(pm);
    ETS_GPIO_INTR_DISABLE();
    PIN_FUNC_SELECT(pm,pf);
    GPIO_DIS_OUTPUT(pn);
    gpio_register_set(zeroCrossCalculator.pin_internal_gpio,
    						GPIO_PIN_INT_TYPE_SET(GPIO_PIN_INTR_DISABLE)
                            | GPIO_PIN_PAD_DRIVER_SET(GPIO_PAD_DRIVER_DISABLE)
                            | GPIO_PIN_SOURCE_SET(GPIO_AS_PIN_SOURCE));
    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, zeroCrossCalculator.pin_internal_bit);
    gpio_pin_intr_state_set(zeroCrossCalculator.pin_internal_gpio, 3);//GPIO_PIN_INTR_ANYEDGE=3, but I dunno why this define does not appear here
    ETS_GPIO_INTR_ENABLE();


    os_timer_disarm(&zcd_timer);
    os_timer_setfn(&zcd_timer, (os_timer_func_t *)zcdimmer_timer_cb,  (void *)NULL);
    PIN_FUNC_SELECT(pin_mux[lamppin],pin_func[lamppin]);
    lamppin_bit=1<<GPIO_ID_PIN(pin_num[lamppin]);
    setdimval(dimval);

	return 1;
}

// Lua: close()
static int zcdimmer_lua_close( lua_State* L )
{
	zeroCrossCalculator.pin_num=0;
	return 0;
}

static int zcdimmer_lua_debuginfo( lua_State* L )
{
	ets_uart_printf("Shift: %lu, T/2: %lu\n",zeroCrossCalculator.detector_shift,zeroCrossCalculator.halfperiod);
	ets_uart_printf("Last state: %d\n",zeroCrossCalculator.last_state);
#ifdef ZCDETECTOR_DEBUG
	printdebug=1;
#endif
	return 1;
}

static void setdimval(u8 d){
	dimval=d;
	dimval_abs=dimmer_absolute[dimval];
#ifdef ZCDETECTOR_DEBUG
	ets_uart_printf("dimval %d\n",dimval);
#endif

}

static int zcdimmer_lua_setintensity( lua_State* L )
{
	u32 dim=luaL_checkinteger( L, 1 );
	if(dim>99)	dim=99;
	setdimval(dim);
	return 1;
}
static int zcdimmer_lua_getintensity( lua_State* L )
{
	lua_pushinteger( L, dimval);
	return 1;
}



// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE zcdimmer_map[] =
{
  { LSTRKEY( "setup" ), LFUNCVAL( zcdimmer_lua_setup ) },
  { LSTRKEY( "close" ), LFUNCVAL( zcdimmer_lua_close ) },
  { LSTRKEY( "setintensity" ), LFUNCVAL( zcdimmer_lua_setintensity ) },
  { LSTRKEY( "getintensity" ), LFUNCVAL( zcdimmer_lua_getintensity ) },
  { LSTRKEY( "debuginfo" ), LFUNCVAL( zcdimmer_lua_debuginfo ) },
#if LUA_OPTIMIZE_MEMORY > 0

#endif
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_zcdimmer( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register( L, AUXLIB_ZCDIMMER, zcdimmer_map );
  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0  
}
