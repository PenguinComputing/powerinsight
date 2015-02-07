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
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
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

/* pi_i2c_read( bus, addr, len ) -- Read [len] bytes at addr
 * @bus -- fd of spi to affect
 * @addr -- register address to select
 * @len -- number of bytes to read
 *
 * TODO: Maybe this should be an i2c-msg.  Unfortunately, we'd have to
 *   do a mess of data wrangling to use I2C_RDWR ioctl( )
 */
int pi_i2c_read( lua_State * L )
{
   int  fd ;
   unsigned char  addr ;
   int  len ;
   char *  buf ;
   ssize_t  ret ;

   fd = luaL_checkint( L, 1 );

   addr = luaL_checkint( L, 2 );
   luaL_argcheck( L, addr != (addr & 0xff), 2, "invalid address" );

   len = luaL_checkint( L, 3 );
   luaL_argcheck( L, len > 0 && len <= 4096, 3, "invalid length [1-4096]" );

   ret = write( fd, &addr, 1 );
   if( ret == -1 ) {
      return luaL_error( L, "i2c_read: write(addr) error: %s", strerror(errno) );
   }

   buf = malloc( len );
   if( buf == NULL ) {
      return luaL_error( L, "read buffer allocation failed" );
   }
   ret = read( fd, buf, len );
   if( ret < 0 ) {
      int  save_errno = errno ;
      free( buf );
      return luaL_error( L, "read failed: %s", strerror(save_errno) );
   }

   lua_pushlstring( L, buf, len );
   free( buf );
   return 1 ;
}

/* pi_i2c_write( bus, addr, data ) -- Write [data] at addr
 * @bus -- fd of spi to affect
 * @addr -- register address to select
 * @data -- bytes to write
 */
int pi_i2c_write( lua_State * L )
{
   int  fd ;
   int  addr ;
   const char *  data ;
   size_t  len ;
   char *  buf ;
   ssize_t  ret ;

   fd = luaL_checkint( L, 1 );

   addr = luaL_checkint( L, 2 );
   luaL_argcheck( L, addr != (addr & 0xff), 2, "invalid address" );

   data = lua_tolstring( L, 3, &len );
   luaL_argcheck( L, data != NULL && len > 0 && len <= 4096, 3, "invalid data" );

   buf = malloc( len +1 );
   if( buf == NULL ) {
      return luaL_error( L, "write buffer allocation failed" );
   }
   *buf = addr ;
   memcpy( buf +1, data, len );
   ret = write( fd, buf, len+1 );
   if( ret < 0 ) {
      int  save_errno = errno ;
      free( buf );
      return luaL_error( L, "write failed: %s", strerror(save_errno) );
   } else if( ret != len+1 ) {
      return luaL_error( L, "incomplete write: %d of %d", ret, len+1 );
   }

   free( buf );
   return 0 ;
}


/* ex: set sw=3 sta et : */
