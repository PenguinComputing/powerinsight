/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Library of functions to handle low-level details of access
 *   to SPI hardware and Power Insight carriers
 *
 * The ADS8344 16-bit Analog to Digital converter on the
 *   main carrier and power expansion boards
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


/* The ADS8344 chip is perhaps better as a differential ADC rather
 *    than single ended as we use it here.  The result is the mux
 *    select is a little strange and we need a channel map for that
 *
 * When initializing the chip, we set the internal/external clocking
 *    and throw away the first conversion value.  Getting a reading
 *    is pretty simple with the exception that the quality of the
 *    reading is rate dependant.  The faster we grab the reading
 *    the more "wrong" the reading is.  You can see this by making
 *    repeated readings over time (as fast as possible) and at
 *    different speeds (clock rates on the SPI link)
 *
 * Lastly, there is an interesting quirk in the chip where it
 *    takes 25 bits to get a complete reading.  This means each
 *    reading is going to take 32 bits no matter what.  There are
 *    three useful ways of aligning the request and the results.
 *
 * -- Immediate start bit.  In this mode, the first bit shifted
 *    out is the start bit.  24 bits later, you have 15 bits of the
 *    result and if you start another command, you can pick up the
 *    final 16th bit from that next command.  In practice, we're
 *    not trying that hard, so we just shift a byte of zeros to
 *    pick up 16 bits.  This "24-bit" mode is documented in the
 *    data sheet.
 *
 * -- Stretched Acquisition.  In this mode, we place the acquisition
 *    phase bits across the byte boundary between two byte transfers
 *    This may provide some additional time for the acquisition of a
 *    voltage and improve the results.  Clever, but it's not entirely
 *    clear if it actually results in better readings.
 *
 * -- Aligned result.  In this mode, the start bit is the last bit
 *    of the first byte.  This allows a 16 bit result to be read in
 *    in the last 2 bytes of a 32-bit transfer without any shifts or
 *    other reassembly.  This mode is documented in the datasheet.
 */


/* Channel to config word
 * NOTE: See datasheet Table I. for non-obvious mapping of channel
 *          to A2/A1/A0 settings in single-ended mode.
 * 0x80  Start bit
 * 0x#0  A2/A1/A0
 * 0x04  Single Ended
 * 0x0#  Power Down (00 = down, 01 = internal clock, 11 = external clock)
 */
static unsigned int chan_map[] = { 0x87, 0xc7, 0x97, 0xd7,
                                   0xa7, 0xe7, 0xb7, 0xf7 };

/* pi_ads8344_mkmsg( mux, [shift] ) -- Create an SPI messaage
 * @mux -- channel to read
 * @shift -- optional shift for the control (default: 1, stretch)
 * -- or --
 * @message -- table with required field "mux" and optional
 *      fields speed, delay_usecs, etc.  returned with tx_buf
 *      filled in.
 * -----
 * @message -- Lua Table with tx_buf and len filled in
 */
int pi_ads8344_mkmsg(lua_State * L)
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
   tx_buf[3] = 0 ;

   lua_pushlstring( L, (char *)tx_buf, 4 );
   lua_setfield( L, -2, "tx_buf" );
   return 1 ;
}

/* pi_ads8344_getraw( [scale], message, [...] ) -- Get a reading from a message
 * @scale -- Scale factor to apply to readings
 * @message -- Table result from an spi_message command created by mkmsg
 * ...  -- Additional results (the output of spi_message)
 * -----
 * @reading -- List of readings scaled to "raw" values [0,1)
 */
int pi_ads8344_getraw(lua_State * L)
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
      if( rx_buf == NULL || len != 4 ) {
         return luaL_argerror( L, arg, "rx_buf missing or invalid" );
      }
      reading = ((((rx_buf[1]<<16)|(rx_buf[2]<<8)|rx_buf[3])>>shift)&0xffff) * scale / 65536.0 ;

      lua_pop( L, 2 ); /* Clean stack of tx_buf, rx_buf */
      lua_pushnumber( L, reading );
   }

   return narg - msgstart +1 ;
}


/* ex: set sw=3 sta et : */
