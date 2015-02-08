/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Library of functions to handle low-level details of access
 *   to i2c hardware and Power Insight carriers
 *
 * The i2c specific bits...
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <linux/types.h>
#include <linux/i2c-dev.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "pilib.h"
#include "piglobal.h"

/* pi_i2c_device( bus, addr ) -- Select i2c device by address
 * @bus -- fd of i2c bus
 * @addr -- address (7-bit) to select
 */
int pi_i2c_device( lua_State * L )
{
   int  fd ;
   unsigned long  addr ;
   int  ret ;

   fd = luaL_checkint( L, 1 );
   addr = luaL_checkint( L, 2 );
   luaL_argcheck( L, addr >= 0 && addr <= 0x7f, 2, "invalid 7-bit i2c device address" );

   ret = ioctl( fd, I2C_SLAVE, addr );
   if( ret == -1 ) {
      return luaL_error( L, "ioctl(SLAVE_ADDR) error: %s", strerror(errno) );
   }

   return 0 ;
}

/* pi_i2c_read( bus, addr ) -- Read byte at addr
 * @bus -- fd of spi to affect
 * @addr -- register address to select
 *
 * TODO: Maybe this should be an i2c-msg.  Unfortunately, we'd have to
 *   do a mess of data wrangling to use I2C_RDWR ioctl( ), but we'd be
 *   able to read/write more than 1 byte at a time.
 */
int pi_i2c_read( lua_State * L )
{
   int  fd ;
   int  addr ;
   unsigned char  buf ;
   ssize_t  ret ;

   fd = luaL_checkint( L, 1 );

   addr = luaL_checkint( L, 2 );
   luaL_argcheck( L, addr == (addr & 0xff), 2, "invalid address" );

   buf = addr ;
   ret = write( fd, &buf, 1 );
   if( ret == -1 ) {
      return luaL_error( L, "i2c_read: write(addr) error: %s", strerror(errno) );
   }

   ret = read( fd, &buf, 1 );
   if( ret < 0 ) {
      return luaL_error( L, "read failed: %s", strerror(errno) );
   }

   lua_pushnumber( L, buf );
   return 1 ;
}

/* pi_i2c_write( bus, addr, data ) -- Write data to addr
 * @bus -- fd of spi to affect
 * @addr -- register address to select
 * @data -- byte to write
 */
int pi_i2c_write( lua_State * L )
{
   int  fd ;
   int  addr ;
   int  data ;
   unsigned char  buf[2] ;
   ssize_t  ret ;

   fd = luaL_checkint( L, 1 );

   addr = luaL_checkint( L, 2 );
   luaL_argcheck( L, addr == (addr & 0xff), 2, "invalid address" );

   data = luaL_checkint( L, 3 );
   luaL_argcheck( L, data == (data & 0xff), 3, "invalid data" );

   buf[0] = addr ;
   buf[1] = data ;
   ret = write( fd, buf, 2 );
   if( ret < 0 ) {
      return luaL_error( L, "write failed: %s", strerror(errno) );
   } else if( ret != 2 ) {
      return luaL_error( L, "incomplete write: %d of %d", ret, 2 );
   }

   return 0 ;
}


/* ex: set sw=3 sta et : */
