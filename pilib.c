/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Library of functions to handle low-level details of access
 *   to SPI hardware and Power Insight carriers
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "pilib.h"
#include "piglobal.h"

/* List of method call names for sensors.
 * ORDER IS IMPORTANT.  See pilib.h for more details.
 *      TODO: If you add or remove entries, check the
 *      rest of the library for changes that might be required
 */
const char * const  piMethodNames[] = {
      "power", "volt", "amp", "temp", "reading", NULL
   };

luaL_Reg pi_funcs[] = {
         {"open",        pi_open},
         {"spi_mode",    pi_spi_mode},
         {"spi_maxspeed", pi_spi_maxspeed},
         {"spi_message", pi_spi_message},
         {"write",       pi_write},
         {"read",        pi_read},
         {"i2c_device",  pi_i2c_device},
         {"i2c_read",    pi_i2c_read},
         {"i2c_write",   pi_i2c_write},
         {"ads1256_init", pi_ads1256_init},
         {"ads1256_wait4DRDY", pi_ads1256_wait4DRDY},
         {"ads1256_getraw", pi_ads1256_getraw},
         {"ads1256_setmux", pi_ads1256_setmux},
         {"ads1256_getraw_setmuxC", pi_ads1256_getraw_setmuxC},
         {"ads1256_rxbuf2raw", pi_ads1256_rxbuf2raw},
/*       {"ads8344_init", pi_ads8344_init}, *** Declared in init_final.lua */
         {"ads8344_mkmsg", pi_ads8344_mkmsg},
         {"ads8344_getraw", pi_ads8344_getraw},
/*       {"mcp3008_init", pi_mcp3008_init}, *** Declared in init_final.lua */
         {"mcp3008_mkmsg", pi_mcp3008_mkmsg},
         {"mcp3008_getraw", pi_mcp3008_getraw},
         {"sc620_init",  pi_sc620_init},
         {"setbank",     pi_setbank},
         {"sens_5v",     pi_sens_5v},
         {"sens_12v",    pi_sens_12v},
         {"sens_3v3",    pi_sens_3v3},
         {"sens_acs713_20", pi_sens_acs713_20},
         {"sens_acs713_30", pi_sens_acs713_30},
         {"sens_acs723_10", pi_sens_acs723_10},
         {"sens_acs723_20", pi_sens_acs723_20},
         {"sens_shunt10", pi_sens_shunt10},
         {"sens_shunt25", pi_sens_shunt25},
         {"sens_shunt50", pi_sens_shunt50},
         {"volt2temp_K", pi_volt2temp_K},
         {"temp2volt_K", pi_temp2volt_K},
         {"rt2temp_PTS", pi_rt2temp_PTS},
         {"temp2rt_PTS", pi_temp2rt_PTS},
         {"setled_temp", pi_setled_temp},
         {"setled_main", pi_setled_main},
         {"gettime",     pi_gettime},
         {"filter",      pi_filter},
         {"verbose",     pi_verbose},
         {"debug",       pi_debug},
         {"Sensors",     pi_Sensors},
         {"addSensors",  pi_addSensors},
         {"addConnectors", pi_addConnectors},
         {NULL, NULL},
         };

/* FIXME: Not implemented yet ... */
int pi_sc620_init(lua_State * L) { return 0 ; }

int pi_setled_temp(lua_State * L) { return 0 ; }
int pi_setled_main(lua_State * L) { return 0 ; }


/* Load the pi library into a lua_State */
int pi_register( lua_State *L )
{
   int  ret ;

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

   /* Debug bits for use with pi.debug() */
   lua_pushlightuserdata( L, (void *)DBG_LUA );
   lua_setfield( L, -2, "DBG_LUA" );
   lua_pushlightuserdata( L, (void *)DBG_SPI );
   lua_setfield( L, -2, "DBG_SPI" );
   lua_pushlightuserdata( L, (void *)DBG_WAIT );
   lua_setfield( L, -2, "DBG_WAIT" );

   /* Other constants?
   lua_pushlightuserdata( L, (void *)XXX );
   lua_setfield( L, -2, "XXX" );
    */

   /* CAREFUL ...
    *    Only put things here which are required to finalize
    *    pi_register, done more easily in Lua than in C, and
    *    not more easily done in init_final.lua which will
    *    run immediately after this.
    */
   /* dostring "init" for initialization from lua */
   ret = luaL_loadstring(L,
/**** BEGIN LUA CODE ****/
"pi.version = 'v0.3'\n"
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
      errstr = luaL_checkstring( L, -1 );
      if( errstr != NULL ) {
         fprintf( stderr, "%s: %s\n", ARGV0, errstr );
      }
      fflush( stderr );
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

/* pi_filter( last, factor, new )
 * @last -- Previous value
 * @factor -- Filter factor 0 = faster, 1 = slower
 * @new -- New value to add to filter
 * -----
 * @next -- New output of filter
 *
 * Use like this:
 *    vcc = pi.filter( vcc, 0.8, 4.096/reading )
 * In terms of a digital low-pass filter, the "time constant"
 *    is related to the factor by the following identity
 *    factor = exp(-1/tau)
 */
int pi_filter(lua_State * L)
{
   lua_Number  last ;
   lua_Number  factor ;
   lua_Number  new ;

   factor = luaL_checknumber( L, 2 );
   luaL_argcheck( L, factor >= 0.0 && factor <= 1.0, 2, "out of range [0,1]" );

   new = luaL_checknumber( L, 3 );

   if( lua_isnil( L, 1 ) ) {
      /* Assume first time, uninitialized last */
      last = new ;
   } else {
      last = luaL_checknumber( L, 1 );
      /* If filter was infected with NaN, reset it */
      if( isnan( last ) ) {
         last = new ;
      }
   }

   lua_pushnumber( L, new + (last - new)*factor );
   return 1 ;
}

/* Functions to allow get/set of debug and test individual bits */
int pi_debug( lua_State *L )
{
   if( lua_isnumber( L, 1 ) ) {
      /* Set the global */
      debug = lua_tointeger( L, 1 );
   } else if( lua_islightuserdata( L, 1 ) ) {
      /* Test bits */
      int result = 0 ;
      int arg = lua_gettop( L );
      for( ; arg > 0 ; --arg ) {
         luaL_checktype( L, arg, LUA_TLIGHTUSERDATA );
         result = result || (debug & (unsigned int)lua_touserdata( L, arg ));
      }
      lua_pushboolean( L, result );
   } else {
      /* Get current value */
      lua_pushnumber( L, debug );
   }

   /* return new value, or current value */
   return 1 ;
}

/* Get/set the verbose global */
int pi_verbose( lua_State *L )
{
   if( lua_isnumber( L, 1 ) ) {
      verbose = lua_tointeger( L, 1 );
   } else {
      lua_pushnumber( L, verbose );
   }

   /* return new value, or current value */
   return 1 ;
}

/* addConnectors(...) creates connectors in the S table (partial sensor
 *      objects) with the connector names, chip select info and
 *      bank settings for each.  An __index metatable is also
 *      set for each object to reduce duplication.
 *
 * pi_addConnectors( hdr, index, ... )
 * @hdr -- Header object to which these sensors are connected
 * @index -- __index method for metatable with items common to all sensors
 * @... -- List of connector table objects
 * -----
 * @count -- Number of connectors processed
 */
int pi_addConnectors(lua_State * L)
{
   int  nargs = lua_gettop( L );
   int  haveMetatbl = 0 ;
   char  prefix[16] ;
   size_t  plen ;  /* Prefix length */
   const char *  p ;
   int  gS ;  /* global S */
   int  gSlen ;  /* Current objlen of S */
   int  gbyName ;  /* global byName */
   int  idx ;


   /* Check arguments:  Table, Table, 3 or more arguments */
   luaL_checktype( L, 1, LUA_TTABLE );

   if( lua_isnoneornil( L, 2 ) ) {
      /* nil or too few arguments */
      haveMetatbl = 0 ;
   } else if( lua_isfunction( L, 2 ) ) {
      /* If it's a function, make it __index */
      lua_createtable( L, 0, 1 );
      lua_pushvalue( L, 2 );
      lua_setfield( L, -2, "__index" );
      lua_replace( L, 2 );  /* Replace function in stack with metatable */
      haveMetatbl = 2 ;  /* Slot for metatable */
   } else if( lua_istable( L, 2 ) ) {
      /* Does it look like it's already a metatable? */
      lua_getfield( L, 2, "__index" );
      if( lua_isnil( L, -1 ) ) {
         /* Nope, so create a metatable with this as __index */
         lua_createtable( L, 0, 1 );
         lua_pushvalue( L, 2 );
         lua_setfield( L, -2, "__index" );
         lua_replace( L, 2 );
         haveMetatbl = 2 ;
      } else {
         /* Cool, just use it as is ... */
         haveMetatbl = 2 ;
      }
      lua_pop( L, 1 );  /* Clean up getfield */
   } else {
      return luaL_argerror( L, 2, "not __index func/table or metatable" );
   }

   if( nargs < 3 ) {
      return luaL_error( L, "requires three or more arguments" );
   }

   lua_getfield( L, 1, "prefix" );
   p = lua_tolstring( L, -1, &plen );
   strncpy( prefix, p, sizeof(prefix) );
   prefix[sizeof(prefix)-1] = '\0' ;
   if( strlen( prefix ) != plen ) {
      fprintf( stderr, "%s: Embedded zero in prefix: [%.*s], truncated", ARGV0, (int)sizeof(prefix), prefix );
      plen = strlen( prefix );
   }

   /* Get globals:
    *    S (sensors table),
    *    byName (index of connector and sensor names)
    */
   lua_getfield( L, LUA_GLOBALSINDEX, "S" );
   gS = lua_gettop( L );
   if( lua_type( L, gS ) != LUA_TTABLE ) {
      return luaL_error( L, "global S is not a table" );
   }
   gSlen = lua_objlen( L, -1 );

   lua_getfield( L, LUA_GLOBALSINDEX, "byName" );
   gbyName = lua_gettop( L );
   if( lua_type( L, gbyName ) != LUA_TTABLE ) {
      return luaL_error( L, "global byName is not a table" );
   }

   for( idx = 3 ; idx <= nargs ; ++idx ) {
      /* Verify is table */
      if( lua_type( L, idx ) != LUA_TTABLE ) {
         return luaL_argerror( L, idx, "not a table" );
      }

      if( haveMetatbl ) {
        /* Set metatable hook */
        lua_pushvalue( L, haveMetatbl );
        lua_setmetatable( L, idx );
      }

      /* Add prefix to connector name */
      lua_getfield( L, idx, "conn" );
      if( lua_isnil( L, -1 ) ) {
         return luaL_argerror( L, idx, "missing 'conn' field" );
      }
      p = lua_tostring( L, -1 );
      strncpy( prefix + plen, p, sizeof(prefix) - plen -1 );
      lua_pop( L, 1 );  /* Done with 'conn' */

      /* Index connector name (add to byName) */
      lua_pushstring( L, prefix );
      lua_pushvalue( L, -1 );
      lua_pushvalue( L, -1 );  /* We need three copies for the following code */
      lua_setfield( L, idx, "conn" );  /* Change conn field, use one copy */
      lua_gettable( L, gbyName ); /* Does this conn name already exist?, use one copy */
      if( ! lua_isnil( L, -1 ) ) {
         return luaL_error( L, "bad argument #%d to addConnectors (duplicate connector name: %s)", idx, prefix );
      }
      lua_pop( L, 1 ); /* clean up gettable */
      lua_pushvalue( L, idx );
      lua_settable( L, gbyName ); /* Index by conn in byName, use last copy */

      /* Store in S */
      lua_pushnumber( L, ++gSlen );
      lua_pushvalue( L, idx );
      lua_settable( L, gS );
   }

   lua_pushnumber( L, nargs - 2 );
   return 1 ;
}

/* Sensors(...) adds sensor items details to the the list
 *      of sensors.  A matching connector must be found
 *      in the list.  If a matching connector does
 *      not already exist, an error is thrown
 *
 * pi_Sensors( ... )
 * @... -- List of sensor detail objects
 * -----
 * @count -- Number of sensors processed
 */
int pi_Sensors(lua_State * L)
{
   int  nargs = lua_gettop( L );
   int  gS ;  /* global S */
   int  gbyName ;  /* global byName */
   int  gTypes ;  /* global Types */
   const char *  p ;
   int  idx ;


   /* Get globals: S (sensors table),
    *           byName (index of names),
    *           Types (index of sensor transfer functions)
    */
   lua_getfield( L, LUA_GLOBALSINDEX, "S" );
   gS = lua_gettop( L );
   if( lua_type( L, gS ) != LUA_TTABLE ) {
      return luaL_error( L, "global S is not a table" );
   }

   lua_getfield( L, LUA_GLOBALSINDEX, "byName" );
   gbyName = lua_gettop( L );
   if( lua_type( L, gbyName ) != LUA_TTABLE ) {
      return luaL_error( L, "global byName is not a table" );
   }

   lua_getfield( L, LUA_GLOBALSINDEX, "Types" );
   gTypes = lua_gettop( L );
   if( lua_type( L, gTypes ) != LUA_TTABLE ) {
      return luaL_error( L, "global Types is not a table" );
   }

   for( idx = 1 ; idx <= nargs ; ++idx ) {

      if( lua_type( L, idx ) != LUA_TTABLE ) {
         return luaL_argerror( L, idx, "not a table" );
      }

      /* Find connector in S -- if not throw error */
      lua_getfield( L, idx, "conn" );
      if( lua_isnil( L, -1 ) ) {
         return luaL_argerror( L, idx, "missing 'conn' field" );
      }
      p = lua_tostring( L, -1 );
      if( p == NULL ) {
         return luaL_argerror( L, idx, "'conn' is not a string" );
      }
      if( verbose >= 1 ) {
         fprintf( stderr, "Processing: %s ...\n", p );
      }

      lua_gettable( L, gbyName );  /* consume 'conn' */
      if( lua_isnil( L, -1 ) ) {
         return luaL_error( L, "bad argument #%d to Sensors (connector '%s' not found)", idx, p );
      }

      /* STACK NOTE:  <param>, S, byName, Types, byName[idx.conn] aka S[conn], <top> */

      /* Verify S.conn.name not set -- if so, throw error */
      lua_getfield( L, -1, "name" );
      if( ! lua_isnil( L, -1 ) ) {
         return luaL_argerror( L, idx, "connector already declared, name already set" );
      }
      lua_pop( L, 1 );  /* Clean up getfield */

      /* Get new name */
      lua_getfield( L, idx, "name" );
      p = lua_tostring( L, -1 );
      if( lua_isnil( L, -1 ) ) {
         /* No user-specified name, so use an empty string,
          * but don't add to byName
          */
         lua_pop( L, 1 );  /* clean up nil */
         lua_pushstring( L, "" );  /* name value to set */
      } else if( p == NULL ) {
         return luaL_argerror( L, idx, "name is not a string" );
      } else {
         /* Verify byName.name not set -- if so, throw error */
         lua_pushvalue( L, -1 );  /* Make copy */
         lua_gettable( L, gbyName );  /* Use copy */
         if( ! lua_isnil( L, -1 ) ) {
            return luaL_error( L, "bad argument #%d to Sensors (name '%s' already used)", idx, p );
         }
         lua_pop( L, 1 );  /* Clean gettable */

         /* Add S.conn.name to byName index */
         lua_pushvalue( L, -1 );  /* Copy name */
         lua_pushvalue( L, -3 );  /* Copy connector */
         lua_settable( L, gbyName );  /* byName[new.name] = conn, use copies */
      }

      /* Copy name to S.conn */
      lua_setfield( L, -2, "name" );  /* Uses name value at TOS */

      /* STACK NOTE:  <param>, S, byName, Types, byName[idx.conn] aka S[conn], <top> */

      /* Copy "everything" (exceptions: Xcs, mux, conn, name) to S.conn
       *        for temp, volt, amp:  if isString, lookup in Types and
       *                substitute -- or throw error
       */
      lua_pushnil( L );
      while( lua_next( L, idx ) != 0 ) {
         /* key @ -2, value @ -1 */
         if( lua_isstring( L, -2 ) ) {
            p = lua_tostring( L, -2 );
            if( strcmp( "name", p ) == 0 || strcmp( "conn", p ) == 0 ) {
               /* Already handled */
               lua_pop( L, 1 );
               continue ;
            } else if( strcmp( "mux", p ) == 0
                  || ((*p=='t' || *p=='a' || *p=='v') && strcmp("cs", p+1)==0)
                  ) {
               /* Don't copy */
               fprintf( stderr, "WARNING: Sensors() NOT copying field %s\n", p );
               lua_pop( L, 1 );  /* Drop value */
               continue ; 
            } else if( strcmp( "temp", p ) == 0 || strcmp( "volt", p ) == 0
                  || strcmp( "amp", p ) == 0 ) {
               /* Check value for string or function */
               if( lua_isstring( L, -1 ) ) {
                  lua_tostring( L, -1 );  /* Force to string */
                  lua_gettable( L, gTypes );
                  if( lua_isnil( L, -1 ) ) {
                     /* Unrecognized Type */
                     return luaL_error( L, "bad argument #%d to Sensors (%s has unrecognized value)", idx, p );
                  }
                  /* Right value is now at TOS */
               } else if( ! lua_isfunction( L, -1 ) ) {
                  /* It's not usable */
                  return luaL_error( L, "bad argument #%d to Sensors (%s is invalid value)", idx, p );
               } /* else TOS is a function, drop through */
            } /* else just copy it, drop through */

            /* Copy it */
            lua_pushvalue( L, -2 );  /* Copy key */
            lua_insert( L, -2 );  /* Swap -1, TOS */
            lua_settable( L, -4 );  /* Consume copy and value */
         } else if( lua_isnumber( L, -2 ) ) {
            /* Just copy it */
            lua_pushvalue( L, -2 );
            lua_insert( L, -2 );
            lua_settable( L, -4 );
         } else {
            /* What could it be? Ignore it */
            fprintf( stderr, "WARNING: Sensors NOT copying unknown field.\n" );
            lua_pop( L, 1 );  /* Drop value */
         }
      }

      lua_pop( L, 1 );  /* Drop S[conn] */
      if( gTypes != lua_gettop( L ) ) {
         fprintf( stderr, "%s: INTERNAL ERROR: Stack push/pop mismatch in Sensors: TOS is %d, should be %d\n", ARGV0, lua_gettop( L ), gTypes );
      }
   }

   lua_pushnumber( L, nargs );
   return 1 ;
}

/* addSensors(...) does the same as Sensors, but only if a matching
 *      connector does NOT already exist.  This function effectively
 *      combines functions of both addConnectors and Sensors
 *
 * pi_addSensors( hdr, ... )
 * @hdr -- Header object to which these sensors are connected
 * @... -- List of complete connector/sensor table objects
 * -----
 * @count -- Number of objects processed
 */
int pi_addSensors(lua_State * L)
{
   int  nargs = lua_gettop( L );
   char  prefix[16] ;
   size_t  plen ;  /* Prefix length */
   const char *  p ;
   int  gS ;  /* global S */
   int  gSlen ;  /* Current objlen of S */
   int  gbyName ;  /* global byName */
   int  gTypes ;  /* global Types */
   const char * const *  mn ;  /* piMethodNames pointer */
   int  idx ;


   /* Check arguments:  Table, 2 or more arguments */
   luaL_checktype( L, 1, LUA_TTABLE );

   if( nargs < 2 ) {
      return luaL_error( L, "requires two or more arguments" );
   }

   lua_getfield( L, 1, "prefix" ); /* from hdr */
   p = lua_tolstring( L, -1, &plen );
   strncpy( prefix, p, sizeof(prefix) );
   prefix[sizeof(prefix)-1] = '\0' ;
   if( strlen( prefix ) != plen ) {
      fprintf( stderr, "%s: Embedded zero in prefix: [%.*s], truncated", ARGV0, (int)sizeof(prefix), prefix );
      plen = strlen( prefix );
   }

   /* Get globals:
    *    S (sensors table),
    *    byName (index of connector and sensor names)
    *    Types (index of sensor transfer functions)
    */
   lua_getfield( L, LUA_GLOBALSINDEX, "S" );
   gS = lua_gettop( L );
   if( lua_type( L, gS ) != LUA_TTABLE ) {
      return luaL_error( L, "global S is not a table" );
   }
   gSlen = lua_objlen( L, -1 );

   lua_getfield( L, LUA_GLOBALSINDEX, "byName" );
   gbyName = lua_gettop( L );
   if( lua_type( L, gbyName ) != LUA_TTABLE ) {
      return luaL_error( L, "global byName is not a table" );
   }

   lua_getfield( L, LUA_GLOBALSINDEX, "Types" );
   gTypes = lua_gettop( L );
   if( lua_type( L, gTypes ) != LUA_TTABLE ) {
      return luaL_error( L, "global Types is not a table" );
   }

   for( idx = 2 ; idx <= nargs ; ++idx ) {
      /* Verify is table */
      if( lua_type( L, idx ) != LUA_TTABLE ) {
         return luaL_argerror( L, idx, "not a table" );
      }

      /* Add prefix to connector name */
      lua_getfield( L, idx, "conn" );
      if( lua_isnil( L, -1 ) ) {
         return luaL_argerror( L, idx, "missing 'conn' field" );
      }
      p = lua_tostring( L, -1 );
      strncpy( prefix + plen, p, sizeof(prefix) - plen -1 );
      lua_pop( L, 1 );  /* Done with "conn" */

      /* Index connector name (add to byName) */
      lua_pushstring( L, prefix );
      lua_pushvalue( L, -1 );
      lua_pushvalue( L, -1 );  /* We need three copies for the following code */
      lua_setfield( L, idx, "conn" );  /* Change conn field, use one copy */

      lua_gettable( L, gbyName ); /* Does this conn name already exist?, use one copy */
      if( ! lua_isnil( L, -1 ) ) {
         return luaL_error( L, "bad argument #%d to addSensors (duplicate connector name: %s)", idx, prefix );
      }
      lua_pop( L, 1 ); /* clean up gettable */

      lua_pushvalue( L, idx );  /* Value */
      lua_settable( L, gbyName );  /* Index by conn in byName, use last copy */

      /* Foreach "temp", "volt", "amp" */
      for( mn = piMethodNames ; *mn != NULL ; ++mn ) {
         lua_getfield( L, idx, *mn );
         /* Check value for string or function */
         if( lua_isstring( L, -1 ) ) {
            lua_tostring( L, -1 );  /* Force to string */
            /* Lookup string in Types */
            lua_gettable( L, gTypes );  /* Replaces TOS */
            if( lua_isnil( L, -1 ) ) {
               /* Unrecognized Type */
               return luaL_error( L, "bad argument #%d to addSensors (%s is unrecognized type)", idx, *mn );
            }
            /* Replace value in table */
            lua_setfield( L, idx, *mn );  /* Cleans stack */
         } else if( ! lua_isnil( L, -1 ) && ! lua_isfunction( L, -1 ) ) {
            /* It exists, but is not usable */
            return luaL_error( L, "bad argument #%d to addSensors (%s is invalid value)", idx, *mn );
         } else {
            lua_pop( L, 1 );  /* Clean up getfield */
         }
      }

      /* Store in S */
      lua_pushnumber( L, ++gSlen );
      lua_pushvalue( L, idx );
      lua_settable( L, gS );

      if( gTypes != lua_gettop( L ) ) {
         fprintf( stderr, "%s: INTERNAL ERROR: Stack push/pop mismatch in addSensors: TOS is %d, should be %d\n", ARGV0, lua_gettop( L ), gTypes );
      }
   }

   lua_pushnumber( L, nargs - 1 );
   return 1 ;
}


/* ex: set sw=3 sta et : */
