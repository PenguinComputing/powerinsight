/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Library of functions to handle low-level details of access
 *   to SPI hardware and Power Insight carriers
 *
 * The SPI specific bits...
 */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "pilib.h"
#include "piglobal.h"

/* pi_spi_mode( spi [, mode] ) -- Get/Set SPI mode
 * @spi -- fd of spi to affect
 * @mode -- constant mode value (lightuserdata)
 */
int pi_spi_mode(lua_State * L)
{
   int  fd ;
   int  request ;
   __u8  mode ;
   int  RDnWR ;  /* READ/not WRITE */
   int  ret ;

   fd = luaL_checkint( L, 1 );
   if( lua_gettop( L ) > 1 ) {
      RDnWR = 0 ; /* WRITE */
      request = SPI_IOC_WR_MODE ;
      luaL_checktype( L, 2, LUA_TLIGHTUSERDATA );
      mode = (__u8)(unsigned long)lua_touserdata( L, 2 );
      if( debug & DBG_SPI ) {
         fprintf( stderr, "spi_mode(%d) = 0x%02x\n", fd, mode );
      }
   } else {
      RDnWR = 1 ; /* READ */
      request = SPI_IOC_RD_MODE ;
   }

   ret = ioctl( fd, request, &mode );
   if( ret == -1 ) {
      return luaL_error( L, "ioctl(MODE) error: %s", strerror(errno) );
   }

   if( RDnWR ) {
      if( debug & DBG_SPI ) {
         fprintf( stderr, "spi_mode(%d) = 0x%02x\n", fd, mode );
      }
      /* FIXME: Maybe return one of the pi.SPI_XXX values?
       *    or return a string?  Or ditch the lightuserdata
       *    and just use integers so they can be bitwise
       *    manipulated.  Of course Lua 5.1 doesn't have bitwise
       *    operations...
       */
      lua_pushinteger( L, mode );
      return 1 ;
   } else {
      return 0 ;
   }
}

/* pi_spi_maxspeed( spi [, speed] ) -- Get/Set SPI MAX_SPEED_HZ
 * @spi -- fd of spi to affect
 * @speed -- speed in Hz
 */
int pi_spi_maxspeed(lua_State * L)
{
   int  fd ;
   int  request ;
   __u32  maxspeed ;
   int  RDnWR ;  /* READ/not WRITE */
   int  ret ;

   fd = luaL_checkint( L, 1 );
   if( lua_gettop( L ) > 1 ) {
      RDnWR = 0 ; /* WRITE */
      request = SPI_IOC_WR_MAX_SPEED_HZ ;
      maxspeed = luaL_checkint( L, 2 );
      luaL_argcheck( L,
         maxspeed >= 1500 && maxspeed <= 2400000,
         2, "1500Hz <= maxspeed <= 2.4MHz"
         );

      if( debug & DBG_SPI ) {
         fprintf( stderr, "spi_maxspeed(%d) = %lu\n", fd, (unsigned long)maxspeed );
      }
   } else {
      RDnWR = 1 ; /* READ */
      request = SPI_IOC_RD_MAX_SPEED_HZ ;
   }

   ret = ioctl( fd, request, &maxspeed );
   if( ret == -1 ) {
      return luaL_error( L, "ioctl(MAX_SPEED_HZ) error: %s", strerror(errno) );
   }

   if( RDnWR ) {
      if( debug & DBG_SPI ) {
         fprintf( stderr, "spi_maxspeed(%d) = %lu\n", fd, (unsigned long)maxspeed );
      }
      lua_pushinteger( L, maxspeed );
      return 1 ;
   } else {
      return 0 ;
   }
}

/* pi_spi_message( spi, {msg} [, {msg} ...] ) -- Send messages
 * @spi -- fd of spi to affect
 * @msg -- table with struct spi_ioc_transfer values
 * --------
 * Returns list of tables with "rx_buf" filled in
 */
int pi_spi_message(lua_State * L)
{
   int  fd ;
   struct spi_ioc_transfer *  msgs ;
   int  cmsg ;  /* Current message in msgs */
   int  narg ;  /* Number of arguments to function */
   size_t  rxsize ;  /* storage required of rxbufs */
   char *  rxbufs ;
   char *  crxbuf ;
   struct timeval  before, after ;
   int  ret ;

   fd = luaL_checkint( L, 1 );
   narg = lua_gettop( L );
   if( narg < 2 ) {
      return luaL_error( L, "too few arguments" );
   }

   /* Allocate msgs array */
   msgs = calloc( narg -1, sizeof(struct spi_ioc_transfer) );
   if( msgs == NULL ) {
      return luaL_error( L, "message buffer allocation failed" );
   }

   /* Walk list of arguments */
   rxsize = 0 ;
   for( cmsg = 0 ; cmsg <= narg -2 ; ++cmsg ) {
      /* Verify it's a table */
      if( lua_type( L, cmsg +2 ) != LUA_TTABLE ) {
         free( msgs );
/* <----- */
         return luaL_argerror( L, cmsg +2, "not a table" );
      }
      /* If has "tx_buf" -- It's a transmit, set len */
      lua_getfield( L, cmsg +2, "tx_buf" );
      if( lua_isstring( L, -1 ) ) {
         size_t  len ;
         msgs[cmsg].tx_buf = (__u64) lua_tolstring( L, -1, &len );
         msgs[cmsg].len = len ;
         if( len < 1 || len > 1000 ) {
            free( msgs );
/* <----- */
            return luaL_argerror( L, cmsg +2, "invalid tx_buf message length (<1 or >1000)" );
         }
         lua_pushnumber( L, len );
         lua_setfield( L, cmsg +2, "len" );
         /* NOTE: Leave tx_buf on the stack to save the string from GC */

      /* Else If has "len" -- It's a read, set tx_buf to NULL */
      } else {
         lua_pop( L, 1 ); /* Clean "tx_buf" left on the stack by getfield */
         lua_getfield( L, cmsg +2, "len" );
         if( lua_isnumber( L, -1 ) ) {
            lua_Integer  len ;
            len = lua_tointeger( L, -1 );
            if( len < 1 || len > 1000 ) {
               free( msgs );
/* <----- */
               return luaL_argerror( L, cmsg +2, "invalid message len (<1 or >1000)" );
            }
            msgs[cmsg].tx_buf = (__u64) NULL ;
            msgs[cmsg].len = len ;

      /* Else throw error, invalid msg */
         } else {
            free( msgs );
/* <----- */
            return luaL_argerror( L, cmsg +2, "missing tx_buf or len value" );
         }
         lua_pop( L, 1 ); /* Clean "len" left on the stack by getfield */
      }

      /* Count how many bytes we need for rx_bufs */
      rxsize += msgs[cmsg].len ;

      /* Set other members -- speed_hz, delay_usecs, bits_per_word, cs_change */
      lua_getfield( L, cmsg +2, "speed_hz" );
      if( lua_isnumber( L, -1 ) ) {
         lua_Integer  speed_hz = lua_tointeger( L, -1 );
         if( speed_hz >= 1500 && speed_hz <= 2400000 ) {
            msgs[cmsg].speed_hz = speed_hz ;
         }
      }
      lua_pop( L, 1 );

      lua_getfield( L, cmsg +2, "delay_usecs" );
      if( lua_isnumber( L, -1 ) ) {
         lua_Integer  delay_usecs = lua_tointeger( L, -1 );
         if( delay_usecs >= 0 && delay_usecs <= 100000 ) {
            msgs[cmsg].delay_usecs = delay_usecs ;
         }
      }
      lua_pop( L, 1 );

      lua_getfield( L, cmsg +2, "bits_per_word" );
      if( lua_isnumber( L, -1 ) ) {
         lua_Integer  bits_per_word = lua_tointeger( L, -1 );
         if( bits_per_word >= 8 && bits_per_word <= 32 ) {
            msgs[cmsg].bits_per_word = bits_per_word ;
         }
      }
      lua_pop( L, 1 );

      lua_getfield( L, cmsg +2, "cs_change" );
      msgs[cmsg].cs_change = lua_toboolean( L, -1 );
      lua_pop( L, 1 );
   }

   /* Allocate storage for rx_buf's */
   rxbufs = malloc( rxsize );
   if( rxbufs == NULL ) {
      free( msgs );
/* <----- */
      return luaL_error( L, "rx_buf buffer allocation failed" );
   }

   /* For all messages */
   crxbuf = rxbufs ;
   for( cmsg = 0 ; cmsg <= narg -2 ; ++cmsg ) {
      /* Allocate rx_buf based on len */
      msgs[cmsg].rx_buf = (__u64) crxbuf ;
      crxbuf += msgs[cmsg].len ;
   }
   if( rxsize != crxbuf - rxbufs ) {
      fputs( "internal error, rxsize != crxbuf - rxbufs\n", stderr );
   }

   /* Call ioctl */
   if( debug & DBG_SPI ) {
      gettimeofday( &before, NULL );
   }
   ret = ioctl( fd, SPI_IOC_MESSAGE(narg -1), msgs );
   if( ret == -1 ) {
      int save_errno = errno ;
      free( rxbufs );
      free( msgs );
/* <----- */
      return luaL_error( L, "ioctl(%d,%d,...) call: %s", fd, narg -1, strerror(save_errno) );
   }
   if( debug & DBG_SPI ) {
      gettimeofday( &after, NULL );
      fprintf( stderr, "DBG: ioctl(%d, %d, ...) took: %.6f sec, ret = %d\n",
         fd, narg -1,
         after.tv_sec-before.tv_sec + (after.tv_usec-before.tv_usec)/1000000.0,
         ret
         );
   }

   /* Walk list of arguments */
   crxbuf = rxbufs ;
   for( cmsg = 0 ; cmsg <= narg -2 ; ++cmsg ) {
      if( debug & DBG_SPI ) {
         int  i ;
         __u8 *  p ;
         fputs( "DBG: ", stderr );
         if( msgs[cmsg].speed_hz != 0 ) fprintf( stderr, "clk=%d ", msgs[cmsg].speed_hz );
         if( msgs[cmsg].delay_usecs != 0 ) fprintf( stderr, "dly=%d ", msgs[cmsg].delay_usecs );
         if( msgs[cmsg].bits_per_word != 0 ) fprintf( stderr, "bpw=%d ", msgs[cmsg].bits_per_word );
         if( msgs[cmsg].cs_change ) fputs( "!CS ", stderr );

         if( msgs[cmsg].tx_buf != (__u64) NULL ) {
            p = (__u8 *) msgs[cmsg].tx_buf ;
            fputs( "TX", stderr );
            for( i = 0 ; i < msgs[cmsg].len ; ++i ) {
               fprintf( stderr, ":%02x", p[i] );
            }
            fputc( ' ', stderr );
         } else {
            fprintf( stderr, "len=%d ", msgs[cmsg].len );
         }
         fputs( " RX", stderr );
         p = (__u8 *) msgs[cmsg].rx_buf ;
         for( i = 0 ; i < msgs[cmsg].len ; ++i ) {
            fprintf( stderr, ":%02x", p[i] );
         }
         fputc( '\n', stderr );
      }

      /* Add "rx_buf" to table */
      lua_pushlstring( L, crxbuf, msgs[cmsg].len );
      lua_setfield( L, cmsg +2, "rx_buf" );
      crxbuf += msgs[cmsg].len ;
   }

   /* Drop the tx_buf strings */
   lua_settop( L, narg );

   /* Return list of tables */
   return narg -1 ;
}

/* pi_setbank( bank, num ) -- Set bank control bits
 * @bank -- table of fd for bank control bits
 * @num -- bank number to select
 * --------
 * Returns nothing
 *
 * NOTE: Also used for general GPIO toggling
 */
int pi_setbank(lua_State * L)
{
   int  fd ;
   int  bank ;
   size_t  bankbits ;
   int  cur ;  /* Current bit settings */
   int  idx ;
   int  ret ;

   luaL_checktype( L, 1, LUA_TTABLE );
   bank = luaL_checkint( L, 2 );

   bankbits = lua_objlen( L, 1 );
   luaL_argcheck( L, bankbits >= 1 && bankbits <= 16, 1, "invalid number of bank bits (1 to 16)" );

   /* Check for saved bank.cur value */
   lua_getfield( L, 1, "cur" );
   if( lua_isnumber( L, -1 ) ) {
      cur = lua_tonumber( L, -1 );
      /* Have any bits that matter changed? */
      if( ((cur ^ bank) & (1<<bankbits)-1) == 0 ) {
/* <---- No changes */
         return 0 ;
      }
   } else {
      cur = -1 ;  /* No previous usable value available */
   }
   /* lua_pop( L, 1 ); -- leave dirt on stack */

   for( idx = 1 ; idx <= bankbits ; ++idx ) {
      if( cur < 0 || (bank ^ cur) & (1<<(idx-1)) ) {
         lua_pushinteger( L, idx );
         lua_gettable( L, 1 );
         if( lua_type( L, -1 ) != LUA_TNUMBER ) {
            return luaL_argerror( L, 1, "bank bit not an fd" );
         }
         fd = lua_tointeger( L, -1 );
         lua_pop( L, 1 );  /* Have to clean up in loops */

         lseek( fd, 0, SEEK_SET );
         ret = write( fd, bank & (1<<(idx-1)) ? "1\n" : "0\n", 2 );
         if( ret < 0 ) {
            return luaL_error( L, "write failed: %s", strerror(errno) );
         } else if( ret != 2 ) {
            return luaL_error( L, "incomplete write: %d of 2", ret );
         }
      }
   }

   /* Save as s.cur */
   lua_pushinteger( L, bank );
   lua_setfield( L, 1, "cur" );

   return 0 ;
}

/* ex: set sw=3 sta et : */
