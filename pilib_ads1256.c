/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Library of functions to handle low-level details of access
 *   to SPI hardware and Power Insight carriers
 *
 * The ADS1256 24-bit Analog to Digital converter on the
 *   temperature carrier
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <linux/types.h>
#include <linux/spi/spidev.h>
#include <errno.h>
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


/* The ADS1256 chip is relatively complicated to configure.  There
 *    are several registers that need to be initialized to put the
 *    chip in a state usable for Power Insight with maximum resolution
 *    It's not clear yet if it helps to enable or disable the buffer
 *    And for production, we probably want to disable auto-calibration
 *    and trigger calibration under program control and monitor the
 *    resulting offset and scale values
 *  Reg  NAME  Init
 *   00  STAT  0x02 buffer ON (or 0x00 for buffer off)
 *   01  MUX   0x68 to read PTS
 *   02  ADCON 0x24 CLKOUT = fCLKIN, SDC off, PGA=16
 *   03  RATE  0xa1 1,000 CPS
 *   04  GPIO  0xc2 LED off (0xc0 turns on LED)
 * 05-7  OFC   0x?? Offset determined by chip calibration cycle
 * 08-a  FSC   0x?? Full-scale determined by chip calibration cycle
 */

struct ads1256_rate {
   int  rate ;  /* samples per second */
   __u8  regval ;
   int  selfcal ;  /* Time to perform SELFCAL (usec) */
   __u32  alpha ;  /* from datasheet page 24 */
   double beta ;
   __u32  fsc ;  /* Ideal full scale calibration */
} ;
#define IDEAL_OFC 0x000000  /* Ideal offset calibration, all rates */

static const struct ads1256_rate ads1256_rate_tbl[] = {
  /* rate   regval  selfcal   alpha      beta      fsc */
   { 30000,  0xf0,     892,  0x400000,  1.8639,  0x44ac08 },
   { 15000,  0xe0,     896,  0x400000,  1.8639,  0x44ac08 },
   {  7500,  0xd0,    1029,  0x400000,  1.8639,  0x44ac08 },
   {  3750,  0xc0,    1300,  0x400000,  1.8639,  0x44ac08 },
   {  2000,  0xb0,    2000,  0x3c0000,  1.7474,  0x494008 },
   {  1000,  0xa1,    3600,  0x3c0000,  1.7474,  0x494008 },
   {   500,  0x92,    6600,  0x3c0000,  1.7474,  0x494008 },
   {   100,  0x82,   31200,  0x4b0000,  2.1843,  0x3a99a0 },
   {    60,  0x72,   50900,  0x3e8000,  1.8202,  0x4651f3 },
   {    50,  0x63,   61800,  0x4b0000,  2.1843,  0x3a99a0 },
   {    30,  0x53,  101300,  0x3e8000,  1.8202,  0x4651f3 },
   {    25,  0x43,  123200,  0x4b0000,  2.1843,  0x3a99a0 },
   {    15,  0x33,  202100,  0x3e8000,  1.8202,  0x4651f3 },
   {    10,  0x23,  307200,  0x5dc000,  2.7304,  0x2ee14c },
   {     5,  0x13,  613800,  0x5dc000,  2.7304,  0x2ee14c },
   {     2,  0x03, 1227200,  0x5dc000,  2.7304,  0x2ee14c }, /* 2.5 sps */
   {     0,  0x00,       0,         0,  0.0000,         0 }
};
#define ads1256_rate_tbl_size  (sizeof(ads1256_rate_tbl)/sizeof(struct ads1256_rate))

/* getrateinfo( rate ) -- Get structure with rate information by rate (>=)
 * @rate -- sampling rate in samples/sec (2-30,000)
 * -----
 * @rateinfo -- struct ads1256_rate pointer
 */
const struct ads1256_rate * getrateinfo( int rate )
{
   int  idx ;

   if( rate >= ads1256_rate_tbl[0].rate ) {
      return ads1256_rate_tbl +0 ;
   } else if( rate <= ads1256_rate_tbl[ads1256_rate_tbl_size-2].rate ) {
      return ads1256_rate_tbl + ads1256_rate_tbl_size -2 ;
   }

   idx = 5 ; /* Guess it's about 1000 */
   while( rate > ads1256_rate_tbl[idx].rate
         || rate <= ads1256_rate_tbl[idx+1].rate
         ) {
      if( rate > ads1256_rate_tbl[idx].rate ) {
         if( idx > 0 ) {
            --idx ;
         } else {
            break ;
         }
      } else if( idx < ads1256_rate_tbl_size -2 ) {
         ++idx ;
      } else {
         break ;
      }
   }
   return ads1256_rate_tbl + idx ;
} 

/* int  gain2reg( gain ) -- Convert gain to register bit pattern
 * @gain -- gain (one of 1, 2, 4, 8, 16, 32, 64)
 * -----
 * @bits -- bit pattern for ADCON register PGA[2:0] (0-6)
 */
static int  gain2reg( int gain )
{
   /* FIXME: Isn't there a kernel or library function that does this? */
   int  reg = 0 ;

   if( gain >= 64 ) {
      return 6 ;
   }
   if( gain >= 16 ) {
      gain /= 16 ;
      reg |= 4 ;
   }
   if( gain >= 4 ) {
      gain /= 4 ;
      reg |= 2 ;
   }
   if( gain >= 2 ) {
      reg |= 1 ;
   }
   return reg ;
}
#define reg2gain(reg) (1<<((reg)&0x07))

/* wait4DRDY( fd, timeout ) -- Wait for DRDY
 * @fd -- spidev device connected to ads1256
 * @timeout -- timeout (minimum to 10msec)
 * -----
 * @DRDY -- returns last DRDY reading (inverted value of #DRDY pin)
 *              or -1 on error and sets errno
 */
static int wait4DRDY( int fd, double timeout )
{
   struct timeval  start ;
   struct timeval  now ;
   unsigned long  loops ;
   struct spi_ioc_transfer  msgs[2] ;
   __u8  bufs[8] ;
   int  ret ;

   if( timeout < 0.010 ) {
      timeout = 0.010 ;
   }

   gettimeofday( &start, NULL );

   /* Initialize the messages */
   memset( msgs, 0, sizeof(msgs) );  /* NOTE: sizeof gets size of total array */
   msgs[0].tx_buf = (__u64) bufs +0 ;
   bufs[0] = 0x10 ;  /* RREG [0] */
   bufs[1] = 0x00 ;  /* +0 more */
   msgs[0].len = 2 ;
   msgs[0].delay_usecs = 8 ;  /* T6: 50 clock periods at 8 MHz */
   msgs[1].rx_buf = (__u64) bufs +4 ;
   msgs[1].len = 1 ;

   loops = 0 ;
   do {
      ++loops ;
      ret = ioctl( fd, SPI_IOC_MESSAGE(2), msgs );
      if( ret == -1 ) {
         return -1 ;
      }
      if( bufs[4] & 1 == 1 ) {
         /* Not ready yet */

         gettimeofday( &now, NULL );
         if( now.tv_sec-start.tv_sec + (now.tv_usec-start.tv_usec)/1000000.0 > timeout ) {
            break ;
         } else if ( timeout >= 0.009 ) {
            /* Wait a bit */
            usleep( 100 );
         }
      }
   } while( bufs[4] & 1 );

   if( debug & DBG_SPI ) {
      fprintf( stderr, "DBG: wait4DRDY(%d) took: %.6f sec, %lu loops, DRDY = %d\n",
            fd, now.tv_sec-start.tv_sec + (now.tv_usec-start.tv_usec)/1000000.0,
            loops,
            !(bufs[4] & 1)
         );
   }

   return !(bufs[4] & 1) ;
}

/* pi_ads1256_wait4DRDY( fd, [timeout] ) -- Wait for DRDY
 * @fd -- spidev device connected to ads1256.  Assumes bank already selected
 * @timeout -- optional timeout.  (defaults to 10msec)
 * -----
 * @DRDY -- returns last DRDY reading (inverted value of #DRDY pin)
 */
int pi_ads1256_wait4DRDY(lua_State * L)
{
   int  fd ;
   lua_Number  timeout ;
   int  ret ;

   fd = luaL_checkint( L, 1 );
   timeout = luaL_optnumber( L, 2, 0.010 );  /* default to 10msec timeout */

   ret = wait4DRDY( fd, timeout );

   if( ret == -1 ) {
      return luaL_error( L, "wait4DRDY(%d): %s", fd, strerror(errno) );
   }

   lua_pushinteger( L, ret );
   return 1 ;
}

/* pi_ads1256_init( fd, [rate], [gain] ) -- Initialize ADS1256
 * @fd -- spidev device connected to ads1256.  Assumes bank already selected
 * @rate -- conversion rate to select (default: 2000)
 * @gain -- gain setting (default: 16)
 * -----
 * @reading -- Reading from PTS sensor
 * @ofc -- SELFCAL calculated ofc (on 0-1 scale)
 * @fsc -- SELFCAL calculated fsc (~1)
 */
int pi_ads1256_init(lua_State * L)
{
   int  fd ;
   int  rate ;
   int  gain ;
   const struct ads1256_rate *  rateinfo ;
   int  gainreg ;
   struct timeval  start ;
   struct timeval  now ;
   unsigned long  loops ;
   struct spi_ioc_transfer  msgs[2] ;
   __u8  bufs[12] ;
   lua_Number  reading ;
   lua_Number  ofc ;
   lua_Number  fsc ;
   int  ret ;

   fd = luaL_checkint( L, 1 );
   rate = luaL_optint( L, 2, 2000 );
   gain = luaL_optint( L, 3, 16 );

   rateinfo = getrateinfo( rate );
   gainreg = gain2reg( gain );

   /* Initialize the message */
   memset( msgs, 0, sizeof(msgs) );
   msgs[0].tx_buf = (__u64) bufs +0 ;
   msgs[0].len = 7 ;
   bufs[0] = 0x50 ;  /* Write register 0 */
   bufs[1] = 0x04 ;  /* +4 more registers */
   bufs[2] = 0x02 ;  /* Enable buffer */
   bufs[3] = 0x68 ;  /* Select PTS sensor channel, AIN6-AINCOM */
   bufs[4] = 0x20 | gainreg ;
   bufs[5] = rateinfo->regval ;
   bufs[6] = 0xc2 ;  /* LED off */
   msgs[0].delay_usecs = 1 ;  /* t11, 4 clocks at 8MHz */
   msgs[1].tx_buf = (__u64) bufs +8 ;
   msgs[1].len = 1 ;
   bufs[8] = 0xf0 ;  /* SELFCAL */
   msgs[1].delay_usecs = rateinfo->selfcal * 4 / 5 ;

   if( debug & DBG_SPI ) {
      gettimeofday( &start, NULL );
   };
   ret = ioctl( fd, SPI_IOC_MESSAGE(2), msgs );
   if( ret < 0 ) {
      return luaL_error( L, "ioctl(%d,2,...) to initialize: %s", fd, strerror(errno) );
   }

   wait4DRDY( fd, rateinfo->selfcal / 1000000.0 );

   if( debug & DBG_SPI ) {
      gettimeofday( &now, NULL );
      fprintf( stderr, "DBG: Init w/SELFCAL took: %.6f sec\n",
            now.tv_sec-start.tv_sec + (now.tv_usec-start.tv_usec)/1000000.0
         );
   }

   /* Restart the conversion */
   memset( msgs, 0, sizeof(msgs) );
   msgs[0].tx_buf = (__u64) bufs +0 ;
   msgs[0].len = 1 ;
   bufs[0] = 0xfc ;  /* SYNC */
   msgs[0].delay_usecs = 4 ;  /* t11, 24 clocks @ 8MHz */
   msgs[1].tx_buf = (__u64) bufs +4 ;
   msgs[1].len = 1 ;
   bufs[4] = 0x00 ;  /* WAKEUP */
   
   if( debug & DBG_SPI ) {
      gettimeofday( &start, NULL );
   };
   ret = ioctl( fd, SPI_IOC_MESSAGE(2), msgs );
   if( ret < 0 ) {
      return luaL_error( L, "ioctl(%d,2,...) to SYNC/WAKE: %s", fd, strerror(errno) );
   }

   wait4DRDY( fd, rateinfo->selfcal / 1000000.0 );

   if( debug & DBG_SPI ) {
      gettimeofday( &now, NULL );
      fprintf( stderr, "DBG: SYNC/WAKE took: %.6f sec\n",
            now.tv_sec-start.tv_sec + (now.tv_usec-start.tv_usec)/1000000.0
         );
   }

   /* Get reading */
   memset( msgs, 0, sizeof(msgs) );
   msgs[0].tx_buf = (__u64) bufs +0 ;
   msgs[0].len = 1 ;
   bufs[0] = 0x01 ;  /* RDATA */
   msgs[0].delay_usecs = 8 ;  /* T6: 50 clock periods at 8 MHz */
   msgs[1].rx_buf = (__u64) bufs +4 ;
   msgs[1].len = 3 ;

   ret = ioctl( fd, SPI_IOC_MESSAGE(2), msgs );
   if( ret < 0 ) {
      return luaL_error( L, "ioctl(%d,2,...) RDATA: %s", fd, strerror(errno) );
   }

   reading = (double)((bufs[4]<<16)|(bufs[5]<<8)|(bufs[6])) / (reg2gain(gainreg) * 0x800000);

   /* Get ofc, fsc */
   memset( msgs, 0, sizeof(msgs) );
   msgs[0].tx_buf = (__u64) bufs +0 ;
   msgs[0].len = 1 ;
   bufs[0] = 0x55 ;  /* RREG [5] */
   bufs[1] = 0x05 ;  /* +5 more */
   msgs[0].delay_usecs = 8 ;  /* T6: 50 clock periods at 8 MHz */
   msgs[1].rx_buf = (__u64) bufs +4 ;
   msgs[1].len = 6 ;

   ret = ioctl( fd, SPI_IOC_MESSAGE(2), msgs );
   if( ret < 0 ) {
      return luaL_error( L, "ioctl(%d,2,...) RREG [OFC, FSC]: %s", fd, strerror(errno) );
   }

   ofc = (double)((bufs[4]) | (bufs[5]<<8) | (bufs[6]<<16)) / rateinfo->alpha ;
   fsc = (double)((bufs[7]) | (bufs[8]<<8) | (bufs[9]<<16)) / rateinfo->fsc ;

   lua_pushnumber( L, reading );
   lua_pushnumber( L, ofc );
   lua_pushnumber( L, fsc );
   return 3 ;
}

/* pi_ads1256_getraw( fd, gain, [timeout] ) -- Initialize ADS1256
 * @fd -- spidev device connected to ads1256.  Assumes channel already selected
 * @gain -- Current gain setting
 * @timeout -- Timeout to wait for DRDY
 * -----
 * @reading -- Reading from selected channel
 */
int pi_ads1256_getraw(lua_State * L)
{
   int  fd ;
   int  gain ;
   lua_Number  timeout ;
   struct spi_ioc_transfer  msgs[2] ;
   __u8  bufs[8] ;
   int  ret ;
   lua_Number  reading ;

   fd = luaL_checkint( L, 1 );
   gain = luaL_checkint( L, 2 );
   timeout = luaL_optnumber( L, 3, 1.0 );

   wait4DRDY( fd, timeout );

   /* Get reading */
   memset( msgs, 0, sizeof(msgs) );
   msgs[0].tx_buf = (__u64) bufs +0 ;
   msgs[0].len = 1 ;
   bufs[0] = 0x01 ;  /* RDATA */
   msgs[0].delay_usecs = 8 ;  /* T6: 50 clock periods at 8 MHz */
   msgs[1].rx_buf = (__u64) bufs +4 ;
   msgs[1].len = 3 ;

   ret = ioctl( fd, SPI_IOC_MESSAGE(2), msgs );
   if( ret < 0 ) {
      return luaL_error( L, "ioctl(%d,2,...) RDATA: %s", fd, strerror(errno) );
   }

   reading = (double)((bufs[4]<<16)|(bufs[5]<<8)|(bufs[6])) / (reg2gain(gain2reg(gain)) * 0x800000);

   lua_pushnumber( L, reading );
   return 1 ;
}

/* ex: set sw=3 sta et : */
