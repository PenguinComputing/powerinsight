/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Loader to allow opening pilib in lua interpreter */

#include <stdio.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "pilib.h"
#include "piglobal.h"

/* Global variables */
char *  ARGV0 = "pilib" ;
char *  libexecdir = PILIBDIR_DEFAULT ;
char *  configfile = PICONFIGFILE_DEFAULT ;
unsigned int  debug = PIDEBUG_DEFAULT ;
int  verbose = PIVERBOSE_DEFAULT ;

int clidebug( lua_State *L )
{
   if( lua_isnumber( L, 1 ) ) {
      debug = lua_tointeger( L, 1 );
   } else {
      lua_pushnumber( L, debug );
   }

   /* return new value, or current value */
   return 1 ;
}

int cliverbose( lua_State *L )
{
   if( lua_isnumber( L, 1 ) ) {
      verbose = lua_tointeger( L, 1 );
   } else {
      lua_pushnumber( L, verbose );
   }

   /* return new value, or current value */
   return 1 ;
}

/* Lua library loader/open function */
PIEXPORT(luaopen_pilib)
int luaopen_pilib( lua_State *L )
{
   int  ret ;

   /* This calls luaL_register */
   pi_register( L );
   fputs( "luaopen_pilib: loaded ...\n", stderr );

   ret = luaL_loadfile( L, "init_final.lua" );
   if( ret != 0 || (ret = lua_pcall( L, 0, 0, 0 )) ) {
      luaPI_doerror( L, ret, "Load/run init_final.lua" );
   } else {
      fputs( "... and ran init_final.lua\n", stderr );
   }

   lua_getfield( L, LUA_GLOBALSINDEX, "pi" );
   lua_pushcfunction( L, clidebug );
   lua_setfield( L, -2, "debug" );
   lua_pushcfunction( L, cliverbose );
   lua_setfield( L, -2, "verbose" );
   fputs( "... and added pi.debug() and pi.verbose() helpers\n", stderr );

   fputs( "... Suggest you:\n    dofile(\"your.conf\")\n    dofile(\"post_conf.lua\")\n", stderr );
   fflush( stderr );

   return 1 ;
}

/* ex: set sw=3 sta et : */
