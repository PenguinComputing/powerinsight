/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Library of functions to handle low-level details of access
 *   to SPI hardware and Power Insight carriers
 *
 * The MCP3008 10-bit Analog to Digital converter on the
 *   main carrier of PowerInsight v1.0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "pilib.h"
#include "piglobal.h"

/***  Management of ADC in Power Insight v2.1
 ***
 ***  Each ADC chip gets it's own table to store it's state.
 ***  These tables are specific to a particular chip in the system
 ***  and keep links to the various shared resources (mux/bank select,
 ***  SPI bus, etc.) so each is fairly self contained.  This also
 ***  allows us to use the object syntax sugar to implicitly
 ***  reference an ADC when calling it's methods.
 ***/


/* When initializing the chip, we set the internal/external clocking
 *    and throw away the first conversion value.  When the start bit
 *    is the last bit of the first byte, a 10 bit result can be read
 *    in in the last 10 bits of a 24-bit transfer without any shifts
 *    or other reassembly.  This mode is documented in the datasheet
 *    in Figure 6-1.  24 bits are required because the sample/hold
 *    period takes an addtional bit for a total of 17.
 *
 * A shift value of 3 will cause the sample/hold period to straddle
 *    two bytes in an SPI transfer giving more settling time.  See
 *    Figure 5-1.
 */


/* Channel to config word
 * 0x80  Start bit
 * 0x40  Single/__Differential
 * 0x38  A2/A1/A0
 * 0x04  Sample/hold interval
 * 0x03  Null and data bits
 */
static unsigned int chan_map[] = { 0xc0, 0xc8, 0xd0, 0xd8,
                                   0xe0, 0xe8, 0xf0, 0xf8 };

/* pi_mcp3008_mkmsg( mux, [shift] ) -- Create an SPI messaage
 * @mux -- channel to read
 * @shift -- optional shift for the control (default: 3, stretch)
 * -- or --
 * @message -- table with required field "mux" and optional
 *      fields speed, delay_usecs, etc.  returned with tx_buf
 *      filled in.
 * -----
 * @message -- Lua Table with tx_buf and len filled in
 */
int pi_mcp3008_mkmsg(lua_State * L)
{
   int  mux = -1 ;
   int  shift ;
   __u8  tx_buf[4] ;

   shift = luaL_optint( L, 2, 1 );
   if( shift < 0 || shift > 7 ) {
      return luaL_argerror( L, 1, "invalid shift value [0,7]" );
   }

   if( lua_type( L, 1 ) == LUA_TNUMBER ) {
      mux = luaL_checkint( L, 1 );
      lua_createtable( L, 0, 2 );
   } else if( lua_type( L, 1 ) == LUA_TTABLE ) {
      lua_getfield( L, 1, "mux" );
      if( lua_type( L, -1 ) != LUA_TNUMBER ) {
         return luaL_argerror( L, 1, "missing integer field 'mux'" );
      }
      mux = lua_tointeger( L, -1 );
      lua_pushvalue( L, 1 ); /* Copy table to top of stack */
   } else {
      return luaL_argerror( L, 1, "not a number or table" );
   }
   if( mux < 0 || mux > 7 ) {
      return luaL_argerror( L, 1, "invalid mux value [0,7]" );
   }

   tx_buf[0] = chan_map[mux] >> shift ;
   tx_buf[1] = chan_map[mux] << (8-shift);
   tx_buf[2] = 0 ;

   lua_pushlstring( L, (char *)tx_buf, 3 );
   lua_setfield( L, -2, "tx_buf" );
   return 1 ;
}

/* pi_mcp3008_getraw( message, [...] ) -- Get a reading from a message
 * @message -- Table result from an spi_message command created by mkmsg
 * ...  -- List of results (the output of spi_message)
 * -----
 * @reading -- List of readings scaled to "raw" values [0,1)
 */
int pi_mcp3008_getraw(lua_State * L)
{
   int  narg ;
   int  msgstart ;
   int  arg ;
   lua_Number  scale ;
   lua_Number  reading ;

   if( lua_isnumber( L, 1 ) ) {
      scale = lua_tonumber( L, 1 );
      msgstart = 2 ;
   } else {
      scale = 1.0 ;
      msgstart = 1 ;
   }
   narg = lua_gettop( L );
   luaL_checkstack( L, narg+2, "allocating stack for return values" );

   for( arg = msgstart ; arg <= narg ; ++arg ) {
      const __u8 *  tx_buf ;
      const __u8 *  rx_buf ;
      size_t  len ;
      int  control ;
      int  shift ;

      luaL_checktype( L, arg, LUA_TTABLE );

      /* Determine the shift (count leading zeros */
      lua_getfield( L, arg, "tx_buf" );
      tx_buf = (const __u8 *) lua_tolstring( L, -1, &len );
      if( tx_buf == NULL || len != 4 ) {
         return luaL_argerror( L, arg, "tx_buf missing or invalid" );
      }
      control = *tx_buf ;
      shift = 7 ;
      if( (control & 0xf0) == 0 ) {
         shift -= 4 ;
         control <<= 4 ;
      }
      if( (control & 0xc0) == 0 ) {
         shift -= 2 ;
         control <<= 2 ;
      }
      if( (control & 0x80) == 0 ) {
         shift -= 1 ;
      }

      /* Now get the result */
      lua_getfield( L, arg, "rx_buf" );
      rx_buf = (const __u8 *) lua_tolstring( L, -1, &len );
      if( rx_buf == NULL || len != 3 ) {
         return luaL_argerror( L, arg, "rx_buf missing or invalid" );
      }
      reading = ((((rx_buf[0]<<16)|(rx_buf[1]<<8)|rx_buf[2])>>shift)&0x3ff) * scale / 0x3ff ;

      lua_pop( L, 2 ); /* Clean stack of tx_buf, rx_buf */
      lua_pushnumber( L, reading );
   }

   return narg - msgstart +1 ;
}


/* ex: set sw=3 sta et : */
