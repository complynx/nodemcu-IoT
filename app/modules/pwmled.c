// Module for interfacing with PWM

//#include "lua.h"
#include "lualib.h"
#include "lauxlib.h"
#include "platform.h"
#include "auxmods.h"
#include "lrotable.h"

#include "c_types.h"

typedef struct {
	unsigned int pwmidr;
	unsigned int pwmidg;
	unsigned int pwmidb;
	u32 frequency;
	u32 maxduty;
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
	if ( led.frequency > 0 ) led.frequency = platform_pwm_setup( led.pwmidr,led.frequency, 1023 );
	if ( led.frequency > 0 ) led.frequency = platform_pwm_setup( led.pwmidg,led.frequency, 0 );
	if ( led.frequency > 0 ) led.frequency = platform_pwm_setup( led.pwmidb,led.frequency, 0 );
	if(led.frequency==0)
		return luaL_error( L, "too many pwms." );
	lua_pushinteger( L, led.frequency );
	platform_pwm_start( led.pwmidr );
	platform_pwm_start( led.pwmidg );
	platform_pwm_start( led.pwmidb );
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

// Lua: realduty = setduty( id, duty )
static int lpwmled_setcolor( lua_State* L )
{
  s32 duty;  // signed to error-check for negative values
  duty = luaL_checkinteger( L, 1);
  if ( duty > NORMAL_PWM_DEPTH )
    return luaL_error( L, "wrong arg range" );
  duty = platform_pwm_set_duty( led.pwmidr, (u32)duty );
  lua_pushinteger( L, duty );
  return 1;
}

// Lua: duty = getduty( id )
static int lpwmled_getcolor( lua_State* L )
{
  u32 duty;
  duty = platform_pwm_get_duty( led.pwmidr );
  lua_pushinteger( L, duty );
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
  { LSTRKEY( "getcolor" ), LFUNCVAL( lpwmled_getcolor ) },
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
