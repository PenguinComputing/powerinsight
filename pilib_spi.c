/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Library of functions to handle low-level details of access
 *   to SPI hardware and Power Insight carriers
 *
 * The SPI specific bits...
 */

#include <stdio.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "pilib.h"
#include "piglobal.h"

/* pi_spi_mode( spi, mode ) -- Set SPI mode
 * @spi -- fd of spi to affect
 * @mode -- constant mode value (lightuserdata)
 */
int pi_spi_mode(lua_State * L)
{
   int  fd ;
   __u8  mode ;
   int  ret ;

   fd = luaL_checkint( L, 1 );
   luaL_checktype( L, 2, LUA_TLIGHTUSERDATA );
   mode = (__u8)lua_touserdata( L, 2 );

   if( debug & DBG_SPI ) {
      fprintf( stderr, "spi_mode(%d) = 0x%02x\n", fd, mode );
   }
   ret = ioctl( fd, SPI_IOC_WR_MODE, &mode );
   if( ret == -1 ) {
      return luaL_error( L, "%s", strerror(errno) );
   } else {
      return 0 ;
   }
}

int pi_spi_maxspeed(lua_State * L) { return 0 ; }
int pi_spi_message(lua_State * L) { return 0 ; }

/* ex: set sw=3 sta et : */
