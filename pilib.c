/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Library of functions to handle low-level details of access
 *   to SPI hardware and Power Insight carriers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <errno.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "pilib.h"
#include "piglobal.h"

luaL_Reg pi_funcs[] = {
         {"open",        pi_open},
         {"spi_mode",    pi_spi_mode},
         {"spi_maxspeed", pi_spi_maxspeed},
         {"spi_message", pi_spi_message},
         {"write",       pi_write},
         {"read",        pi_read},
         {"ads1256_init", pi_ads1256_init},
         {"ads1256_wait4DRDY", pi_ads1256_wait4DRDY},
         {"ads1256_getraw", pi_ads1256_getraw},
         {"ads1256_setmux", pi_ads1256_setmux},
         {"ads1256_rxbuf2raw", pi_ads1256_rxbuf2raw},
         {"ads8344_init", pi_ads8344_init},
         {"sc620_init",  pi_sc620_init},
         {"setbank",     pi_setbank},
         {"getraw_temp", pi_getraw_temp},
         {"getraw_volt", pi_getraw_volt},
         {"getraw_amp",  pi_getraw_amp},
         {"volt2temp_K", pi_volt2temp_K},
         {"temp2volt_K", pi_temp2volt_K},
         {"rt2temp_PTS", pi_rt2temp_PTS},
         {"temp2rt_PTS", pi_temp2rt_PTS},
         {"setled_temp", pi_setled_temp},
         {"setled_main", pi_setled_main},
         {"gettime",     pi_gettime},
         {NULL, NULL},
         };

int pi_ads8344_init(lua_State * L) { return 0 ; }
int pi_sc620_init(lua_State * L) { return 0 ; }

int pi_getraw_temp(lua_State * L) { return 0 ; }
int pi_getraw_volt(lua_State * L) { return 0 ; }
int pi_getraw_amp(lua_State * L) { return 0 ; }

int pi_setled_temp(lua_State * L) { return 0 ; }
int pi_setled_main(lua_State * L) { return 0 ; }


/* Load the pi library into a lua_State */
int pi_register( lua_State *L )
{
   int  ret ;

   /* We assume an empty stack, ie, no arguments */
   if( lua_gettop( L ) != 0 ) {
      fputs( "WARNING: pi_register called with non-empty stack. Dropped\n", stderr );
      lua_settop( L, 0 );
   }

   /* FIXME: For Lua 5.2, use luaL_setfuncs but do the
    *   same thing, ie: create global reference to package
    *   "pi" with these functions
    */
   luaL_register( L, "pi", pi_funcs );
   /* NOTE: table pi is left on the stack */

   /* Add some "constants" we'll need */
   /* #defines for ioctl() */
   lua_pushlightuserdata( L, (void *)SPI_CPHA );
   lua_setfield( L, -2, "SPI_CPHA" );
   lua_pushlightuserdata( L, (void *)SPI_CPOL );
   lua_setfield( L, -2, "SPI_CPOL" );
   lua_pushlightuserdata( L, (void *)SPI_MODE_0 );
   lua_setfield( L, -2, "SPI_MODE_0" );
   lua_pushlightuserdata( L, (void *)SPI_MODE_1 );
   lua_setfield( L, -2, "SPI_MODE_1" );
   lua_pushlightuserdata( L, (void *)SPI_MODE_2 );
   lua_setfield( L, -2, "SPI_MODE_2" );
   lua_pushlightuserdata( L, (void *)SPI_MODE_3 );
   lua_setfield( L, -2, "SPI_MODE_3" );
   /* There are other "mode" bits but we don't need them
    *    right now, so don't create them
    *    SPI_CS_HIGH, SPI_LSB_FIRST, SPI_3WIRE
    *    SPI_LOOP, SPI_NO_CS, SPI_READY
    * Also, we have discrete functions for the different
    *    ioctl calls. (spi_mode, spi_message, etc.)
    */

   /* From global environment */
   lua_pushstring( L, ARGV0 );
   lua_setfield( L, -2, "ARGV0" );
   lua_pushinteger( L, verbose );
   lua_setfield( L, -2, "verbose" );
   /* FIXME: Include debug when Lua gets usable bitwise operators */

   /* Other constants?
   lua_pushlightuserdata( L, (void *)XXX );
   lua_setfield( L, -2, "XXX" );
    */

   /* FIXME: BE CAREFUL. This probably belongs in "init_final.lua"
    *    which must run immediately after pi_register()
    *    Only put things here which are required to finalize
    *    pi_register and more easily done in Lua than in C
    */
   /* dostring "init" for initialization from lua */
   ret = luaL_loadstring(L,
/**** BEGIN LUA CODE ****/
"pi.version = 'v0.1'\n"
/**** END LUA CODE ****/
      );
   if( ret != 0 || (ret = lua_pcall(L, 0, 0, 0)) ) {
      luaPI_doerror( L, ret, "Load/run 'pi_register' finalize script" );
      exit( 1 );
   }

   /* No return arguments on stack, if called as a Lua_CFunction */
   return 0 ;
}

/* Helper for printing more verbose error texts when "do"'ing Lua code */
void luaPI_doerror( lua_State * L, int ret, const char * attempt )
{
   const char * errstr ;
   if( ret != 0 ) {
      if( ret == LUA_ERRSYNTAX ) {
         fprintf( stderr, "%s: %s: Syntax error\n",
                     ARGV0, attempt );
      } else if( ret == LUA_ERRFILE ) {
         fprintf( stderr, "%s: %s: Unable to open/read file\n",
                     ARGV0, attempt );
      } else if( ret == LUA_ERRMEM ) {
         fprintf( stderr, "%s: %s: Memory allocation error\n",
                     ARGV0, attempt );
      } else if( ret == LUA_ERRRUN ) {
         fprintf( stderr, "%s: %s: Runtime error\n",
                     ARGV0, attempt );
      } else if( ret == LUA_ERRERR ) {
         fprintf( stderr, "%s: %s: Double fault\n",
                     ARGV0, attempt );
      } else {
         fprintf( stderr, "%s: %s: Unexpected error (%d)\n",
                     ARGV0, attempt, ret );
      }
      if( errstr = luaL_checkstring( L, -1 ) ) {
         fprintf( stderr, "%s:%s\n", ARGV0, errstr );
      }
      exit( 1 );
   }
}

/* pi_gettime( [start] ) -- Get absolute or delta time
 * @start -- starting seconds to subtract from current time (default 0)
 * -----
 * @delta -- Current time delta to start
 *
 * NOTE: It turns out that a double has *just* enough bits to hold a
 *      struct timeval with micro-second resolution without loss of
 *      precision.  32 bits for seconds and 20 bits for microseconds
 *      and 53 effective bits in a double mantissa
 */
int pi_gettime(lua_State * L)
{
   lua_Number  seconds ;
   struct timeval  now ;

   seconds = luaL_optnumber( L, 1, 0.0 );

   gettimeofday( &now, NULL );

   lua_pushnumber( L, now.tv_sec - seconds + now.tv_usec/1000000.0 );
   return 1 ;
}

/* ex: set sw=3 sta et : */
