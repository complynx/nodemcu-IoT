// Module for interfacing with PWM

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

extern int ets_uart_printf(char* fmt,...);

#include "c_types.h"

typedef struct {
	unsigned int pwmidr;
	unsigned int pwmidg;
	unsigned int pwmidb;
	u16 frequency;
	u16 maxduty_r;
	u16 maxduty_g;
	u16 maxduty_b;
} LED;

static int checkid(lua_State* L,unsigned *ret,char var){
	*ret = luaL_checkinteger( L, var );
	if(*ret==0)
		return luaL_error( L, "no pwm for D0" );
	MOD_CHECK_ID( pwm, *ret );
	return 1;
}

static LED led;


// Lua: realfrequency = setup( idR,idG,idB,frequency)
static int lpwmled_setup( lua_State* L )
{
	s32 freq;	  // signed, to error check for negative values
	int err;

	if((err=checkid(L,&(led.pwmidr),1))!=1) return err;
	if((err=checkid(L,&(led.pwmidg),2))!=1) return err;
	if((err=checkid(L,&(led.pwmidb),3))!=1) return err;
	freq = luaL_checkinteger( L, 4 );
	if ( freq <= 0 )
		return luaL_error( L, "wrong arg range" );
	led.frequency=freq;
	if ( led.frequency > 0 ) led.frequency = platform_pwm_setup( led.pwmidr,led.frequency, 0 );
	if ( led.frequency > 0 ) led.frequency = platform_pwm_setup( led.pwmidg,led.frequency, 0 );
	if ( led.frequency > 0 ) led.frequency = platform_pwm_setup( led.pwmidb,led.frequency, 0 );
	if(led.frequency==0)
		return luaL_error( L, "too many pwms." );
	lua_pushinteger( L, led.frequency );
	platform_pwm_start( led.pwmidr );
	platform_pwm_start( led.pwmidg );
	platform_pwm_start( led.pwmidb );
	led.maxduty_r=1023;
	led.maxduty_g=1023;
	led.maxduty_b=1023;
	return 1;
}

// Lua: close()
static int lpwmled_close( lua_State* L )
{
	platform_pwm_close(  led.pwmidr );
	platform_pwm_close(  led.pwmidg );
	platform_pwm_close(  led.pwmidb );
	return 0;
}

// Lua: realclock = setclock( clock )
static int lpwmled_setclock( lua_State* L )
{
  unsigned id;
  s32 clk;	// signed to error-check for negative values
  
  clk = luaL_checkinteger( L, 1 );
  if ( clk <= 0 )
    return luaL_error( L, "wrong arg range" );
  led.frequency=clk;
  led.frequency = platform_pwm_set_clock( led.pwmidr,led.frequency );
  //due to ESP construction no need in next two
  //led.frequency = platform_pwm_set_clock( led.pwmidg,led.frequency );
  //led.frequency = platform_pwm_set_clock( led.pwmidb,led.frequency );
  lua_pushinteger( L, led.frequency );
  return 1;
}

// Lua: clock = getclock()
static int lpwmled_getclock( lua_State* L )
{
  unsigned id;
  u32 clk;
  clk = platform_pwm_get_clock( led.pwmidr );
  lua_pushinteger( L, clk );
  return 1;
}
#define MIN(a,b) (((a)>(b))?(b):(a))
#define MAX(a,b) (((a)>(b))?(a):(b))
#define CLIP(a,m,M) (((a)>(m))?(MIN((a),(M))):(m))

static int lpwmled_setcolor_abs_i(u32 r,u32 g,u32 b)
{
  platform_pwm_set_duty( led.pwmidr,CLIP(r,0,NORMAL_PWM_DEPTH) );
  platform_pwm_set_duty( led.pwmidg,CLIP(g,0,NORMAL_PWM_DEPTH) );
  platform_pwm_set_duty( led.pwmidb,CLIP(b,0,NORMAL_PWM_DEPTH) );
  return 1;
}

static u8 rawColorToChannel(u32 rc,u16 rcmax){
	u32 c=rc*255;
	c/=rcmax;
	return MIN(c,255);
}
static u32 ChannelToRawColor(u8 c,u16 rcmax){
	u32 rc=(u32)c*rcmax;
	rc/=255;
	return MIN(rc,NORMAL_PWM_DEPTH);
}

// Lua: realduty = setcolor_absolute(R,G,B) r&g&b an be 0-1023
static int lpwmled_setcolor_abs( lua_State* L )
{
  s32 r,g,b;  // signed to error-check for negative values
  r = luaL_checkinteger( L, 1);
  g = luaL_checkinteger( L, 2);
  b = luaL_checkinteger( L, 3);
  return lpwmled_setcolor_abs_i(r,g,b);
}

static int lpwmled_setcolor_i(u8 r,u8 g,u8 b)
{
	return lpwmled_setcolor_abs_i(
			ChannelToRawColor(r,led.maxduty_r),
			ChannelToRawColor(g,led.maxduty_g),
			ChannelToRawColor(b,led.maxduty_b)
	);
}

static int lpwmled_setcolor_ic(u32 color)
{
	u8 r,g,b;
	r=(color&0xff0000)>>16;
	g=(color&0xff00)>>8;
	b=(color&0xff);
	return lpwmled_setcolor_i(r,g,b);
}

// Lua: realduty = setcolor(color) color=0xRRGGBB
// Lua: realduty = setcolor(R,G,B) r&g&b an be 0-255
static int lpwmled_setcolor( lua_State* L )
{
	int n = lua_gettop(L);    /* number of arguments */
	s32 color;  // signed to error-check for negative values
	u8 r,g,b;
	if(n==1){
		color = luaL_checkinteger( L, 1);
		r=(color&0xff0000)>>16;
		g=(color&0xff00)>>8;
		b=(color&0xff);
	}else{
		r=luaL_checkinteger( L, 1)&0xff;
		g=luaL_checkinteger( L, 2)&0xff;
		b=luaL_checkinteger( L, 3)&0xff;
	}
	return lpwmled_setcolor_i(r,g,b);
}

static u8 lpwmled_getred_i(){return rawColorToChannel(platform_pwm_get_duty( led.pwmidr ),led.maxduty_r);}
static u8 lpwmled_getgreen_i(){return rawColorToChannel(platform_pwm_get_duty( led.pwmidg ),led.maxduty_g);}
static u8 lpwmled_getblue_i(){return rawColorToChannel(platform_pwm_get_duty( led.pwmidb ),led.maxduty_b);}

static u32 lpwmled_getcolor_i(){
	u32 duty,c;
	c=lpwmled_getred_i();
	duty = (c<<16);
	c=lpwmled_getgreen_i();
	duty |= (c<<8);
	c=lpwmled_getblue_i();
	duty |= c;
	return duty;
}

// Lua: =tune(r,g,b) 0..1023
static int lpwmled_tune( lua_State* L ){
	u32 color=lpwmled_getcolor_i();
	s32 c=luaL_checkinteger( L, 1);
	led.maxduty_r=CLIP(c,0,NORMAL_PWM_DEPTH);
	c=luaL_checkinteger( L, 2);
	led.maxduty_g=CLIP(c,0,NORMAL_PWM_DEPTH);
	c=luaL_checkinteger( L, 3);
	led.maxduty_b=CLIP(c,0,NORMAL_PWM_DEPTH);
	lpwmled_setcolor_ic(color);
	return 1;
}

// Lua: color = getcolor([mode=0])
/**
 * modes:
 * 0) int 0xRRGGBB
 * 1-3) RR,GG,BB
 * 4-6) RRRR,GGGG,BBBB raw
 * 7-9) RRRR,GGGG,BBBB tune
 */
static int lpwmled_getcolor( lua_State* L )
{
	int n = lua_gettop(L);    /* number of arguments */
	if(n==0){
		lua_pushinteger( L, lpwmled_getcolor_i() );
	}else{
		n=luaL_checkinteger( L, 1);
	}
	switch(n){
	case 1:
		lua_pushinteger( L, lpwmled_getred_i());
		break;
	case 2:
		lua_pushinteger( L, lpwmled_getgreen_i());
		break;
	case 3:
		lua_pushinteger( L, lpwmled_getblue_i());
		break;
	case 4:
		lua_pushinteger( L, platform_pwm_get_duty( led.pwmidr ));
		break;
	case 5:
		lua_pushinteger( L, platform_pwm_get_duty( led.pwmidg ));
		break;
	case 6:
		lua_pushinteger( L, platform_pwm_get_duty( led.pwmidb ));
		break;
	case 7:
		lua_pushinteger( L, led.maxduty_r);
		break;
	case 8:
		lua_pushinteger( L, led.maxduty_g);
		break;
	case 9:
		lua_pushinteger( L, led.maxduty_b);
		break;
	default:
		lua_pushinteger( L, lpwmled_getcolor_i());
	}
	return 1;
}

// Module function map
#define MIN_OPT_LEVEL 2
#include "lrodefs.h"
const LUA_REG_TYPE pwmled_map[] =
{
  { LSTRKEY( "setup" ), LFUNCVAL( lpwmled_setup ) },
  { LSTRKEY( "close" ), LFUNCVAL( lpwmled_close ) },
  { LSTRKEY( "setclock" ), LFUNCVAL( lpwmled_setclock ) },
  { LSTRKEY( "getclock" ), LFUNCVAL( lpwmled_getclock ) },
  { LSTRKEY( "setcolor" ), LFUNCVAL( lpwmled_setcolor ) },
  { LSTRKEY( "setcolor_abs" ), LFUNCVAL( lpwmled_setcolor_abs ) },
  { LSTRKEY( "getcolor" ), LFUNCVAL( lpwmled_getcolor ) },
  { LSTRKEY( "tune" ), LFUNCVAL( lpwmled_tune ) },
#if LUA_OPTIMIZE_MEMORY > 0

#endif
  { LNILKEY, LNILVAL }
};

LUALIB_API int luaopen_pwmled( lua_State *L )
{
#if LUA_OPTIMIZE_MEMORY > 0
  return 0;
#else // #if LUA_OPTIMIZE_MEMORY > 0
  luaL_register( L, AUXLIB_PWMLED, pwmled_map );
  return 1;
#endif // #if LUA_OPTIMIZE_MEMORY > 0  
}
