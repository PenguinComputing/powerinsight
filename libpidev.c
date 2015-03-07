/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* pidev library for PowerAPI */

/* Override defaults if no specified on compiler command line with -Dxxx=yyy */
#ifndef PILIBDIR_DEFAULT
   #define PILIBDIR_DEFAULT "."
#endif
#ifndef PICONFIGFILE_DEFAULT
   #define PICONFIGFILE_DEFAULT "pidev.conf"
#endif

#include <stdio.h>
#include <string.h>
#include <math.h>
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


/* Helper function for _read and _temp functions */
int pidev_read_helper( char prefix, int portNumber, reading_t * sample )
{
   char  name[8] ;
   char *  p = name + sizeof(name) -1 ;

   if( sample == NULL ) {
      return PIERR_NOSAMPLE ;
   }

   if( portNumber < 1 || portNumber > MAX_PORTNUM ) {
      return PIERR_NOTFOUND ;
   }

   *p-- = '\0' ;
   while( p > name && portNumber ) {
      *p-- = portNumber % 10 + '0' ;
      portNumber /= 10 ;
   }
   *p = prefix ;

   return pidev_read_byname( p, sample );
}

/* Set the library "global" values */
PIEXPORT(pidev_setup)
int pidev_setup( char * a, char * l, char * c, unsigned int d, int v )
{
   if( a != NULL ) { ARGV0 = a ; }
   if( l != NULL ) { libexecdir = l ; }
   if( c != NULL ) { configfile = c ; }
   debug = d ;
   verbose = v ;

   return PIERR_SUCCESS ;
}

/* Open/init the library */
PIEXPORT(pidev_open)
int pidev_open( )
{
   int  ret ;

   /* Create a Lua instance */
   L = luaL_newstate( );
   if( L == NULL ) {
      fprintf( stderr, "%s: Memory allocation error creating Lua instance.\n", ARGV0 );
      return PIERR_ERROR ;
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

   return PIERR_SUCCESS ;
}

/* Get a [power] reading by connector number (J##)
 *
 * This depends on the MainCarrier or config file creating names
 *      "J1" through "J15" (and higher) for each sensor.
 *      It calls read_byname to do the work and so can read any
 *      sensor if named appropriately.
 */
PIEXPORT(pidev_read)
int pidev_read( int portNumber, reading_t * sample )
{
   return pidev_read_helper( 'J', portNumber, sample );
}


/* Get a temperature reading by connector number (T##)
 *
 * This depends on the MainCarrier or config file creating names
 *      "T1" through "T8" (and higher) for each sensor.
 *      It calls read_byname to do the work and so can read any
 *      sensor if named appropriately.
 */
PIEXPORT(pidev_temp)
int pidev_temp( int portNumber, reading_t * sample )
{
   return pidev_read_helper( 'T', portNumber, sample );
}


/* Get a reading by name
 *
 * This looks up the name, and calls the "power", "volt", "temp", "amp"
 *      or "reading" method on the sensor.  They are checked in that order
 *      and whichever is found first is called.
 */
PIEXPORT(pidev_read_byname)
int pidev_read_byname( char * name, reading_t * sample )
{
   if( sample == NULL ) { return PIERR_NOSAMPLE ; }

   if( debug & DBG_PIDEV ) {
      fprintf( stderr, "DBG: read_byname( '%s', %p )\n", name, sample );
      fflush( stderr );
   }

   /* Clean the stack */
   lua_settop( L, 0 );

   /* pi.tryUpdate( )  globals */
   lua_getfield( L, LUA_GLOBALSINDEX, "pi" );
   lua_getfield( L, -1, "tryUpdate" );
   if( debug & DBG_PIDEV ) { fprintf( stderr, "DBG: Calling tryUpdate ..." ); }
   lua_pcall( L, 0, 0, 0 );
   if( debug & DBG_PIDEV ) {
      fprintf( stderr, " Done\n" );
      fflush( stderr );
   }
   lua_pop( L, 1 ); /* pi */

   /* The name index */
   lua_getfield( L, LUA_GLOBALSINDEX, "byName" );

   /* byName[name] */
   lua_getfield( L, -1, name );
   if( lua_isnil( L, -1 ) ) {
      if( debug & DBG_PIDEV ) {
         fprintf( stderr, "DBG: read_byname: '%s' not found\n", name );
      }
      return PIERR_NOTFOUND ;
   }
   if( debug & DBG_PIDEV ) {
      fprintf( stderr, "DBG: byName.%s is %s\n",
            name,
            lua_typename( L, lua_type( L, -1 ) )
         );
   }

   /* Look for a method to run */

   /* power */
   if( debug & DBG_PIDEV ) {
      fprintf( stderr, "DBG: Looking for byName.%s.%s\n",
            name, piMethodNames[PIMN_POWER]
         );
   }
   lua_getfield( L, -1, piMethodNames[PIMN_POWER] );
   if( lua_isfunction( L, -1 ) ) {
      if( debug & DBG_PIDEV ) {
         fprintf( stderr, "DBG: Found %s as %s\n",
               piMethodNames[PIMN_POWER],
               lua_typename( L, lua_type( L, -1 ) )
            );
      }
      /* Found method:  p, v, a = s:power( ) */
      lua_replace( L, -3 );  /* Replace byName in the stack */
      if( lua_pcall( L, 1, 3, 0 ) != 0 ) {
         goto failure ;
      }

      if( lua_isnumber( L, -3 ) ) {
         sample->watt = lua_tonumber( L, -3 );
      } else {
         sample->watt = NAN ;
      }
      if( lua_isnumber( L, -2 ) ) {
         sample->volt = lua_tonumber( L, -2 );
      } else {
         sample->volt = NAN ;
      }
      if( lua_isnumber( L, -1 ) ) {
         sample->amp  = lua_tonumber( L, -1 );
      } else {
         sample->amp = NAN ;
      }
      goto success ;
   }
   lua_pop( L, 1 ); /* Clean up */

   /* temp */
   if( debug & DBG_PIDEV ) {
      fprintf( stderr, "DBG: Looking for byName.%s.%s\n",
            name, piMethodNames[PIMN_TEMP]
         );
   }
   lua_getfield( L, -1, piMethodNames[PIMN_TEMP] );
   if( lua_isfunction( L, -1 ) ) {
      if( debug & DBG_PIDEV ) {
         fprintf( stderr, "DBG: Found %s as %s\n",
               piMethodNames[PIMN_TEMP],
               lua_typename( L, lua_type( L, -1 ) )
            );
      }
      /* Found method:  v = s:temp( ) */
      lua_replace( L, -3 );  /* Replace byName in the stack */
      if( lua_pcall( L, 1, 1, 0 ) != 0 ) {
         goto failure ;
      }

      if( lua_isnumber( L, -1 ) ) {
         sample->temp = lua_tonumber( L, -1 );
      } else {
         sample->temp = NAN ;
      }
      sample->volt = sample->amp = NAN ;

      goto success ;
   }
   lua_pop( L, 1 ); /* Clean up */

   /* volt */
   if( debug & DBG_PIDEV ) {
      fprintf( stderr, "DBG: Looking for byName.%s.%s\n",
            name, piMethodNames[PIMN_VOLT]
         );
   }
   lua_getfield( L, -1, piMethodNames[PIMN_VOLT] );
   if( lua_isfunction( L, -1 ) ) {
      if( debug & DBG_PIDEV ) {
         fprintf( stderr, "DBG: Found %s as %s\n",
               piMethodNames[PIMN_VOLT],
               lua_typename( L, lua_type( L, -1 ) )
            );
      }
      /* Found method:  v = s:volt( ) */
      lua_replace( L, -3 );  /* Replace byName in the stack */
      if( lua_pcall( L, 1, 1, 0 ) != 0 ) {
         goto failure ;
      }

      if( lua_isnumber( L, -1 ) ) {
         sample->volt = lua_tonumber( L, -1 );
      } else {
         sample->volt = NAN ;
      }
      sample->reading = sample->volt ;
      sample->amp = NAN ;

      goto success ;
   }
   lua_pop( L, 1 ); /* Clean up */

   /* amp */
   if( debug & DBG_PIDEV ) {
      fprintf( stderr, "DBG: Looking for byName.%s.%s\n",
            name, piMethodNames[PIMN_AMP]
         );
   }
   lua_getfield( L, -1, piMethodNames[PIMN_AMP] );
   if( lua_isfunction( L, -1 ) ) {
      if( debug & DBG_PIDEV ) {
         fprintf( stderr, "DBG: Found %s as %s\n",
               piMethodNames[PIMN_AMP],
               lua_typename( L, lua_type( L, -1 ) )
            );
      }
      /* Found method:  v = s:amp( ) */
      lua_replace( L, -3 );  /* Replace byName in the stack */
      if( lua_pcall( L, 1, 1, 0 ) != 0 ) {
         goto failure ;
      }

      if( lua_isnumber( L, -1 ) ) {
         sample->amp = lua_tonumber( L, -1 );
      } else {
         sample->amp = NAN ;
      }
      sample->reading = sample->amp ;
      sample->volt = NAN ;

      goto success ;
   }
   lua_pop( L, 1 ); /* Clean up */

   /* reading */
   if( debug & DBG_PIDEV ) {
      fprintf( stderr, "DBG: Looking for byName.%s.%s\n",
            name, piMethodNames[PIMN_READING]
         );
   }
   lua_getfield( L, -1, piMethodNames[PIMN_READING] );
   if( lua_isfunction( L, -1 ) ) {
      if( debug & DBG_PIDEV ) {
         fprintf( stderr, "DBG: Found %s as %s\n",
               piMethodNames[PIMN_READING],
               lua_typename( L, lua_type( L, -1 ) )
            );
      }
      /* Found method:  v = s:reading( ) */
      lua_replace( L, -3 );  /* Replace byName in the stack */
      if( lua_pcall( L, 1, 1, 0 ) != 0 ) {
         goto failure ;
      }

      if( lua_isnumber( L, -1 ) ) {
         sample->reading = lua_tonumber( L, -1 );
      } else {
         sample->reading = NAN ;
      }
      sample->volt = sample->amp = NAN ;

      goto success ;
   }
   /* lua_pop( L, 1 );  -* Clean up */

failure:
   /* No method found */
   sample->reading = sample->volt = sample->amp = NAN ;
   if( debug & DBG_PIDEV ) {
      fprintf( stderr, "DBG: read_byname returning NOTFOUND\n" );
   }
   return PIERR_NOTFOUND ;

success:
   if( debug & DBG_PIDEV ) {
      fprintf( stderr, "DBG: read_byname returning SUCCESS\n" );
   }
   return PIERR_SUCCESS ;
}

/* Close any open files */
PIEXPORT(pidev_close)
int pidev_close( void )
{
   /* FIXME: Not sure what to do here...  How do we shut down
    *   all the open file descriptors and he Lua instance?
    */
   return PIERR_SUCCESS ;
}

/* ex: set sw=3 sta et : */
