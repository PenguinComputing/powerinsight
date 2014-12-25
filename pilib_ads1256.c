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

int pi_init_ads1256(lua_State * L)
{

   /* Lua?
    * pi.spi_message( spi,
    *           { tx_buf=string.char(0x50,0x04,0x02,0x68,0x24,0xa1,0xc2),
                  delay_usec = 1 },  -- WREG
    *           { tx_buf = "\240" } )  -- SELFCAL
    * Wait for DRDY
    * pi.spi_message( spi,
    *           { tx_buf = "\252", delay_usec = 4 },  -- SYNC
    *           { tx_buf = "\0" } )  -- WAKEUP
    * Wait for DRDY
    */
   return 0 ;
}

int pi_getraw_ads1256(lua_State * L)
{
   return 0 ;
}

/* ex: set sw=3 sta et : */
