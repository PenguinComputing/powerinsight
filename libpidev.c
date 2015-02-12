/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* pidev library for PowerAPI */

#include <stdio.h>
#include <time.h>
#include <string.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "pilib.h"
#include "piglobal.h"
#include "pidev.h"

/* Global variables */
char *  ARGV0 = "libpidev.so.0" ;
char *  libexecdir = PILIBDIR_DEFAULT ;
char *  configfile = PICONFIGFILE_DEFAULT ;
unsigned int  debug = PIDEBUG_DEFAULT ;
int  verbose = PIVERBOSE_DEFAULT ;

/* For pathnames */
static char  buffer[1024];

/* The Lua instance */
static lua_State *  L = NULL ;

/* Time of last update */
static time_t  update_time ;

/* Functions to allow get/set of debug and verbose */
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

/* Open/init the library */
void pidev_open( )
{
   int  ret ;

   /* Create a Lua instance */
   L = luaL_newstate( );
   if( L == NULL ) {
      fprintf( stderr, "%s: Memory allocation error creating Lua instance.\n", ARGV0 );
      return ;
   }

   /* Load initial state */
   luaL_openlibs( L );  /* Standard libraries */

   /* Register Power Insight library */
   pi_register( L );

   /* Finish initialization with Lua code */
   strcpy( buffer, libexecdir );
   strcat( buffer, "/init_final.lc" );
   ret = luaL_loadfile( L, buffer );
   if( ret != 0 || (ret = lua_pcall( L, 0, 0, 0 )) ) {
      strcpy( buffer, "Load/run " );
      strcat( buffer, libexecdir );
      strcat( buffer, "/init_final.lc" );
      luaPI_doerror( L, ret, buffer );
   }

   /* Add additional functions */
   lua_getfield( L, LUA_GLOBALSINDEX, "pi" );
   lua_pushcfunction( L, clidebug );
   lua_setfield( L, -2, "debug" );
   lua_pushcfunction( L, cliverbose );
   lua_setfield( L, -2, "verbose" );
   lua_pop( L, 1 );

   /* Now read the config file */
   ret = luaL_loadfile( L, configfile );
   if( ret != 0 || (ret = lua_pcall( L, 0, LUA_MULTRET, 0 )) ) {
      strcpy( buffer, "Processing config file " );
      strcat( buffer, configfile );
      luaPI_doerror( L, ret, buffer );
   }
   if( (debug & DBG_LUA) && lua_gettop( L ) ) {
      fprintf( stderr, "Config file returned %d values. Ignored\n", lua_gettop( L ) );
   }
   lua_pop( L, lua_gettop( L ));

   /* Post-configfile initialization with Lua code */
   strcpy( buffer, libexecdir );
   strcat( buffer, "/post_conf.lc" );
   ret = luaL_loadfile( L, buffer );
   if( ret != 0 || (ret = lua_pcall( L, 0, 0, 0 )) ) {
      strcpy( buffer, "Load/run " );
      strcat( buffer, libexecdir );
      strcat( buffer, "/post_conf.lc" );
      luaPI_doerror( L, ret, buffer );
   }

   update_time = time( NULL );
   return ;
}

/* Get a power reading
 *
 * This assumes that the config file has "named" the sensors with
 *      "1" through "15".  But if an entry isn't found by number,
 *      it will also be looked for by connector name by pre-pending
 *      a "J" (eg. "J1", "J8", "J15")
 */
void pidev_read( int portNumber, reading_t *sample )
{
   int  len ;

   if( sample == NULL ) { return ; }

   /* Clean the stack */
   lua_settop( L, 0 );

   /* Time we updated the global values? */
   if( time(NULL) - update_time > 60 ) {
      size_t  ulen ;
      int  i ;
      lua_getfield( L, LUA_GLOBALSINDEX, "Update" );
      ulen = lua_objlen( L, -1 );
      for( i = 1 ; i <= ulen ; ++i ) {
         /* Update[i]:update( ) */
         lua_pushinteger( L, i );
         lua_gettable( L, -2 );
         lua_getfield( L, -1, "update" );
         lua_pushvalue( L, -2 );
         lua_pcall( L, 1, 0, 0 );
         lua_pop( L, 1 );  /* Clean up Update[i] */
      }
      lua_pop( L, 1 );  /* Clean up Update */
      update_time = time( NULL );
   }

   /* Some globals we need */
   lua_getfield( L, LUA_GLOBALSINDEX, "byName" );

   lua_getfield( L, LUA_GLOBALSINDEX, "power" );

   /* byName[portNumber] */
   len = snprintf( buffer, sizeof(buffer), "J%d", portNumber );
   if( len >= sizeof(buffer) ) {
      buffer[sizeof(buffer)-1] = '\0' ;
   }

   lua_getfield( L, -2, buffer +1 );
   if( lua_isnil( L, -1 ) ) {
      lua_pop( L, 1 );
      /* Jxxx */
      lua_getfield( L, -2, buffer );
      if( lua_isnil( L, -1 ) ) {
         return ;
      }
   }
   if( lua_isnil( L, -1 ) ) {
      /* Can't find byName[portNumber] */
      return ;
   }

   /* p, v, a = power( s ) */
   lua_pcall( L, 1, 3, 0 );

   if( lua_isnumber( L, 2 ) ) {
      sample->miliwatts = lua_tonumber( L, 2 ) * 1000.0 ;
   }
   if( lua_isnumber( L, 3 ) ) {
      sample->milivolts = lua_tonumber( L, 3 ) * 1000.0 ;
   }
   if( lua_isnumber( L, 4 ) ) {
      sample->miliamps  = lua_tonumber( L, 4 ) * 1000.0 ;
   }

   return ;
}

/* Close any open files */
void pidev_close( void )
{
   return ;
}

/* ex: set sw=3 sta et : */
