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

luaL_Reg pi_funcs[] = {
         {"open",        pi_open},
         {"ioctl",       pi_ioctl},
         {"write",       pi_write},
         {"init_ads1256", pi_init_ads1256},
         {"init_ads8344", pi_init_ads8344},
         {"init_sc620",  pi_init_sc620},
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
         {NULL, NULL},
         };

int pi_open(lua_State * L) { return 0 ; };
int pi_ioctl(lua_State * L) { printf( "THIS is pi_ioctl\n" ); return 0 ; };
int pi_write(lua_State * L) { return 0 ; };
int pi_init_ads1256(lua_State * L) { return 0 ; };
int pi_init_ads8344(lua_State * L) { return 0 ; };
int pi_init_sc620(lua_State * L) { return 0 ; };
int pi_setbank(lua_State * L) { return 0 ; };
int pi_getraw_temp(lua_State * L) { return 0 ; };
int pi_getraw_volt(lua_State * L) { return 0 ; };
int pi_getraw_amp(lua_State * L) { return 0 ; };
int pi_volt2temp_K(lua_State * L) { return 0 ; };
int pi_temp2volt_K(lua_State * L) { return 0 ; };
int pi_rt2temp_PTS(lua_State * L) { return 0 ; };
int pi_temp2rt_PTS(lua_State * L) { return 0 ; };
int pi_setled_temp(lua_State * L) { return 0 ; };
int pi_setled_main(lua_State * L) { return 0 ; };

int pi_register( lua_State *L )
{
   int  ret ;

   /* FIXME: Update for Lua 5.2, to use luaL_setfuncs but
    *   do the same thing, ie: create global package "pi"
    *   with these functions
    */
   luaL_register( L, "pi", pi_funcs );

   /* Add some "constants" we'll need */
   /* #defines for ioctl() */

   /* FIXME: BE CAREFUL. This probably belongs in "init_final.lua"
    *    which typicall runs immediately after pi_register()
    *    Only put things here which are required to finalize
    *    pi_register and more easily done in Lua than in C
    */
   /* dostring "init" for initialization from lua */
   ret = luaL_loadstring(L, 
/**** BEGIN LUA CODE ****/
"pi.version = 0.1\n"
/**** END LUA CODE ****/
      );
   if( ret != 0 || (ret = lua_pcall(L, 0, 0, 0)) ) {
      luaPI_doerror( L, ret, "Load/run 'pi_register' finalize script" );
      exit( 1 );
   }

   /* No return arguments on stack, if called as a Lua_CFunction */
   return 0 ;
}

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

/* ex: set sw=3 sta et : */
