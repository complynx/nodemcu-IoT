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


ZeroCrossCalculator zeroCrossCalculator={
		/*times*/{0,0,0,0,0,0},
		/*detector_shift*/0,
		/*half period*/0,
		/*current_time*/0,
		/*last_overflow*/times_size,
		/*tick_tock*/0,
		/*pin_num*/0,
		/*pin internals*/0,0
};
int printdebug=0;
void ZCD_tick(){
	u32 T=system_get_time();
	u32 dt1,dt2,dtm;
	size_t i;
	if(T<zeroCrossCalculator.times[zeroCrossCalculator.current_time]){
		for(i=0;i<times_size;++i){
			zeroCrossCalculator.times[i]=0;
		}
		zeroCrossCalculator.current_time=times_size;
		zeroCrossCalculator.last_overflow=times_size;
	}
	INCI(zeroCrossCalculator.current_time,times_size);
	zeroCrossCalculator.times[zeroCrossCalculator.current_time]=T;

	if(zeroCrossCalculator.last_overflow>0){ --zeroCrossCalculator.last_overflow;
	}else{
		i=zeroCrossCalculator.current_time;
		dt1=zeroCrossCalculator.times[i];
		dt1-=zeroCrossCalculator.times[DECI(i,times_size)];
		dt1+=zeroCrossCalculator.times[DECI(i,times_size)];
		dt1-=zeroCrossCalculator.times[DECI(i,times_size)];
		dt1>>=1;
		i=zeroCrossCalculator.current_time;
		dt2=zeroCrossCalculator.times[DECI(i,times_size)];
		dt2-=zeroCrossCalculator.times[DECI(i,times_size)];
		dt2+=zeroCrossCalculator.times[DECI(i,times_size)];
		dt2-=zeroCrossCalculator.times[DECI(i,times_size)];
		dt2>>=1;
		i=zeroCrossCalculator.current_time;
		if(dt1>dt2){
			zeroCrossCalculator.tick_tock=1;
			SWAP(dt1,dt2);
		}else zeroCrossCalculator.tick_tock=0;
		dtm=dt2;
		dtm-=dt1;
		dtm>>=1;
		zeroCrossCalculator.detector_shift=dtm;
		dtm=dt2;
		dtm+=dt1;
		dtm>>=1;
		zeroCrossCalculator.halfperiod=dtm;
	}
	if(printdebug){
		printdebug=0;
		ets_uart_printf("DEBUG: Dt: %lu %lu \n",dt1,dt2);
	}
}

static void clearZCC(){
	zeroCrossCalculator.current_time=0;
	zeroCrossCalculator.last_overflow=times_size;
	zeroCrossCalculator.halfperiod=0;
	zeroCrossCalculator.detector_shift=0;
}

// Lua: realfrequency = setup( id_ZCD,id_lamp)
static int zcdimmer_lua_setup( lua_State* L )
{
	u32 id_ZCD=luaL_checkinteger( L, 1 );
	u8 pn,pf;
	u32 pm;
//	u32 id_lamp=luaL_checkinteger( L, 2 );
	MOD_CHECK_ID( gpio, id_ZCD );
//	MOD_CHECK_ID( gpio, id_lamp );
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
	ets_uart_printf("Tock: %d\n",zeroCrossCalculator.tick_tock);
	printdebug=1;
	return 1;
}


// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE zcdimmer_map[] =
{
  { LSTRKEY( "setup" ), LFUNCVAL( zcdimmer_lua_setup ) },
  { LSTRKEY( "close" ), LFUNCVAL( zcdimmer_lua_close ) },
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
