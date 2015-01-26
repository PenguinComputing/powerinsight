/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Library of functions to handle low-level details of access
 *   to SPI hardware and Power Insight carriers
 *
 * The file I/O specific bits...
 */

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <linux/types.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "pilib.h"
#include "piglobal.h"

/* pi_open( filename ) -- Get fd for a file
 * @filename -- path to file to open
 * --Returns--
 * @fd -- fd open on file
 */
int pi_open(lua_State * L)
{
   int  fd ;
   const char *  filename ;

   filename = luaL_checkstring( L, 1 );
   fd = open( filename, O_RDWR );
   if( fd < 0 ) {
      return luaL_error( L, "error openining '%s': %s", filename, strerror( errno ) );
   }
   lua_pushinteger( L, fd );
   return 1 ;
}

/* pi_write( fd, data [, ...] ) -- write to an fd
 * @fd -- fd of an open file
 * @data -- information to write
 *              string copied verbatim
 *              number written as byte value
 */
int pi_write(lua_State * L)
{
   int  fd ;
   int  narg ;
   int  idx ;
   luaL_Buffer  b ;
   const char *  data ;
   size_t  len ;
   ssize_t  ret ;

   fd = luaL_checkint( L, 1 );
   narg = lua_gettop( L );

   luaL_buffinit( L, &b );
   for( idx = 2 ; idx <= narg ; ++idx ) {
      const char *  p ;
      size_t  l ;
      lua_Integer  c ;

      switch( lua_type( L, idx ) ) {
      case LUA_TSTRING :
         p = lua_tolstring( L, idx, &l );
         if( p ) {
            luaL_addlstring( &b, p, l );
         }
         break ;
      case LUA_TNUMBER :
         c = lua_tointeger( L, idx );
         luaL_addchar( &b, c );
         break ;
      default :
         luaL_argerror( L, idx, "must be string or number" );
         break ;
      }
   }
   luaL_pushresult( &b );
   data = lua_tolstring( L, -1, &len );
   if( data != NULL ) {
      lseek( fd, 0, SEEK_SET );
      ret = write( fd, data, len );
      if( ret < 0 ) {
         return luaL_error( L, "write failed: %s", strerror(errno) );
      } else if( ret != len ) {
         return luaL_error( L, "incomplete write: %d of %d", ret, len );
      }
   }

   return 0 ;
}

/* pi_read( fd [, len] ) -- read from an fd
 * @fd -- fd of an open file
 * @len -- number of bytes to read (default: 16)
 */
int pi_read(lua_State * L)
{
   int  fd ;
   int  len ;
   __u8 *  buf ;
   size_t  ret ;


   fd = luaL_checkint( L, 1 );
   len = luaL_optint( L, 2, 16 );
   luaL_argcheck( L, len > 0 && len <= 4096, 2, "invalid length" );

   buf = malloc( len );
   if( buf == NULL ) {
      return luaL_error( L, "read buffer allocation failed" );
   }
   ret = read( fd, buf, len );
   if( ret < 0 ) {
      return luaL_error( L, "read failed: %s", strerror(errno) );
   }
   lua_pushlstring( L, buf, len );
   return 1 ;
}

/* ex: set sw=3 sta et : */
