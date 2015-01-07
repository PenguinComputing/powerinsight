/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Power Insight v2.x
 *
 * Support (through config file options) Power Insight v1.0 and v2.1 hardware
 *   including temperature expansion board.
 * Provide readings via command line and library for linking with PIAPI
 *   effort from Sandia National Lab.
 * Assumes Lua 5.1.4
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "powerInsight.h"
#include "piglobal.h"

/* Global variables */
char *  ARGV0 = "powerInsight" ;
char *  libexecdir = PILIBDIR_DEFAULT ;
char *  configfile = PICONFIGFILE_DEFAULT ;
unsigned int  debug = PIDEBUG_DEFAULT ;
int  verbose = PIVERBOSE_DEFAULT ;

/* The Lua state */
lua_State * L = NULL ;

/* Shared space */
char buffer[1024];

/* Parse options.
 * Return 0 if OK, -1 if error (print usage and exit)
 */
extern int optind ;
int parseOptions( int argc, char** argv )
{
   int  option ;
   int  usage = 0 ;

   /* Save command name for errors and other output */
   ARGV0 = argv[0] ;

   while( -1 != (option = getopt( argc, argv, "uvqd:c:D:" )) ) {
      switch( option ) {

      case '?' : /* fallthrough */
      case 'u' : usage = -1 ; break ;

      case 'v' : ++verbose ; break ;
      case 'q' : verbose = -1 ; break ;
      case 'd' :
         debug |= strtoul( optarg, NULL, 0 );
         if( verbose >= 2 )
            fprintf( stderr, "Debug flags now: %04x\n", debug );
         break ;
      case 'c' :
         configfile = optarg ;
         if( verbose >= 2 )
            fprintf( stderr, "Configfile now: %s\n", configfile );
         break ;
      case 'D' :
         libexecdir = optarg ;
         if( verbose >= 2 )
            fprintf( stderr, "Library directory now: %s\n", libexecdir );
         break ;
      }
   }

   return usage ;
   /* Also, argv[optind] is the first non-option argument */
}

void usage( )
{
   printf(
"usage: %s [-u] [-v] [-d flags] [-c file] [-D directory] [chan ...]\n"
"where:\n"
"   -u  print this usage menu\n"
"   -v  increments verbosity\n"
"   -q  quiet mode\n"
"   -d  set one or more debug flags (bitmask)\n"
"   -c  specify location of configuration file\n"
"   -D  specify prefix for library files\n"
"   chan  one or more channels to read and report:\n"
"         eg: J1, J2, J3, ..., J15\n"
"             T0 -- Main carrier temperature\n"
"             Vcc -- Main carrier Vcc\n"
"             T1, T2, ..., T6, T7, T8\n"
"             TJ1, TJ2 -- Temp carrier junction temps\n"
         "",
         ARGV0 );
   exit( 1 );
}

int main( int argc, char ** argv )
{
   int i ;
   int ret ;

   if( parseOptions( argc, argv ) )
      usage( );
   if( verbose >= 1 ) {
      fprintf( stderr,
            "Configfile: %s\n"
            "Library dir: %s\n"
            "Debug flags: 0x%08x\n"
            "Verbosity: %d\n",
            configfile,
            libexecdir,
            debug,
            verbose
         );
   }

   /* Create a Lua instance */
   L = luaL_newstate( );
   if( L == NULL ) {
      fprintf( stderr, "%s: Memory allocation error creating Lua instance.\n", ARGV0 );
      exit( 1 );
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

   /* Push all args and Run "App" */
   if( ! lua_checkstack( L, argc - optind +2 ) ) {
      fprintf( stderr, "%s: Too many arguments\n", ARGV0 );
      exit( 1 );
   }
   lua_getfield( L, LUA_GLOBALSINDEX, "App" );
   for( i = optind ; i < argc ; ++i ) {
      lua_pushstring( L, argv[i] );
   }
   ret = lua_pcall( L, argc - optind, 0, 0 );
   if( ret != 0 ) {
      luaPI_doerror( L, ret, "Running application 'App'" );
   }

   return 0 ;
}

/* ex: set sw=3 sta et : */
