/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Library of functions to handle low-level details of access
 *   to SPI hardware and Power Insight carriers
 *
 * The sensor bits...
 *   Functions to convert readings into voltages or currents
 * TODO:
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "pilib.h"
#include "piglobal.h"

/* We need to stringify MACRO limit values */
#define tostr(s) xstr(s)
#define xstr(s) #s

/* pi_sens_5v -- 5V sensor divider
 * @reading -- "raw" reading in range [0,1)
 * @vref -- optional reference voltage -- default 4.096
 */
int pi_sens_5v(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  vref ;

   reading = luaL_checknumber( L, 1 );
   vref = luaL_optnumber( L, 2, 4.096 );

   lua_pushnumber( L, reading * vref * (414.0/249) );
   return 1;
}

/* pi_sens_12v -- 12V sensor divider
 * @reading -- "raw" reading in range [0,1)
 * @vref -- optional reference voltage -- default 4.096
 */
int pi_sens_12v(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  vref ;

   reading = luaL_checknumber( L, 1 );
   vref = luaL_optnumber( L, 2, 4.096 );

   lua_pushnumber( L, reading * vref * (535.0/133) );
   return 1;
}

/* pi_sens_3v3 -- 3v3 sensor divider
 * @reading -- "raw" reading in range [0,1)
 * @vref -- optional reference voltage -- default 4.096
 */
int pi_sens_3v3(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  vref ;

   reading = luaL_checknumber( L, 1 );
   vref = luaL_optnumber( L, 2, 4.096 );

   lua_pushnumber( L, reading * vref * (121.0/110) );
   return 1;
}

/* pi_sens_acs713_20 -- ACS713 current sensor (20A)
 * @reading -- "raw" reading in range [0,1)
 * @vref -- optional reference voltage -- default 5.00 (not used)
 */
int pi_sens_acs713_20(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  vref = 5.0 ;

   reading = luaL_checknumber( L, 1 );
//   vref = luaL_optnumber( L, 2, 5.0 );

   lua_pushnumber( L, (reading - 0.1) * (5.0/0.185) );
   return 1;
}

/* pi_sens_acs713_30 -- ACS713 current sensor (30A)
 * @reading -- "raw" reading in range [0,1)
 * @vref -- optional reference voltage -- default 5.00 (not used)
 */
int pi_sens_acs713_30(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  vref = 5.0 ;

   reading = luaL_checknumber( L, 1 );
//   vref = luaL_optnumber( L, 2, 5.0 );

   lua_pushnumber( L, (reading - 0.1) * (5.0/0.133) );
   return 1;
}

/* pi_sens_acs723_10 -- ACS723 current sensor (10A)
 * @reading -- "raw" reading in range [0,1)
 * @vref -- optional reference voltage -- default 5.00 (not used)
 */
int pi_sens_acs723_10(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  vref = 5.0 ;

   reading = luaL_checknumber( L, 1 );
//   vref = luaL_optnumber( L, 2, 5.0 );

   lua_pushnumber( L, (reading - 0.1) * (5.0/0.400) );
   return 1;
}

/* pi_sens_acs723_20 -- ACS723 current sensor (20A)
 * @reading -- "raw" reading in range [0,1)
 * @vref -- optional reference voltage -- default 5.00 (not used)
 */
int pi_sens_acs723_20(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  vref = 5.0 ;

   reading = luaL_checknumber( L, 1 );
//   vref = luaL_optnumber( L, 2, 5.0 );

   lua_pushnumber( L, (reading - 0.1) * (5.0/0.200) );
   return 1;
}

/* The shunt sensor is made from a high precision differential
 *    amplifier with a 10x gain that amplifies the voltage across
 *    a shunt resistor.  The result is input to a op-amp summation
 *    circuit to add an offset and a small additional gain.
 * The offset is set by resistors R4 and R5 between Vcc and GND
 * The secondary gain is set by R1 and R2 feedback resistors
 * The summation is done using R3 resistor in series and the
 *    Thevinin equivalent resistance of the offset R4/R5 divider.
 * The transfer function is:
 *   ( Current * Shunt ) * 10  =>  Vamplified (Va)
 *   R3  =>  Ramplified (Ra)
 *   Vcc * R4/(R4+R5)  =>  Voffset (Voff)
 *   R4*R5/(R4+R5)  =>  Roffset (Roff)
 *   (R1+R2)/R1  =>  Gain2
 *   Voff*Gain2*Ra/(Roff+Ra)  =>  Offset
 *   Va*Gain2*Roff/(Ra+Roff)  =>  Signal
 *   Offset + Signal  =>  Sensor output
 * The selected resistors values are: (all in kOhms)
 *   R1 = 15.0,  R2 = 30.1,  R3 = 10.0,  R4 = 10.2,  R5 = 309
 * Plugging in and solving gives:
 *   Va = I * Rshunt * 10
 *   Ra = 10.0
 *   Voff = Vcc * 10.2/319.2
 *   Roff = 3151.8/319.2
 *   Gain2 = 45.1/15
 *   Offset = Vcc*(10.2/319.2)*(45.1/15)*10.0/(10.0+3151.8/319.2)
 *   Offset = Vcc*(4600.2/95157)
 *   Signal = I*Shunt*10*(45.1/15)*(3151.8/319.2)/(10.0+3151.8/319.2)
 *   Signal = I*Shunt*(1421461.8/95157)
 *   Sensor = Vcc*(4600.2/95157) + I*Shunt*(1421461.8/95157)
 * A "raw" Reading is [0,1) fraction of Vcc
 *   Sensor = Vcc * Reading
 * Solving for I gives
 *   I = Vcc * (Reading * 95157 - 4600.2) / (Shunt * 1421461.8)
 *
 * pi_sens_shunt10 -- Shunt resistor current sensor (10mOhm)
 * @reading -- "raw" reading in range [0,1)
 * @vcc -- recommended reference voltage -- default 5.00
 */
int pi_sens_shunt10(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  vcc ;

   reading = luaL_checknumber( L, 1 );
   vcc = luaL_optnumber( L, 2, 5.0 );

   lua_pushnumber( L, (reading-(4600.2/95157))*vcc*(95157/(0.010*1421461.8)) );
   return 1;
}

/* pi_sens_shunt25 -- Shunt resistor current sensor (25mOhm)
 * @reading -- "raw" reading in range [0,1)
 * @vcc -- recommended reference voltage -- default 5.00
 */
int pi_sens_shunt25(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  vcc ;

   reading = luaL_checknumber( L, 1 );
   vcc = luaL_optnumber( L, 2, 5.0 );

   lua_pushnumber( L, (reading-(4600.2/95157))*vcc*(95157/(0.025*1421461.8)) );
   return 1;
}

/* pi_sens_shunt50 -- Shunt resistor current sensor (50mOhm)
 * @reading -- "raw" reading in range [0,1)
 * @vcc -- recommended reference voltage -- default 5.00
 */
int pi_sens_shunt50(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  vcc ;

   reading = luaL_checknumber( L, 1 );
   vcc = luaL_optnumber( L, 2, 5.0 );

   lua_pushnumber( L, (reading-(4600.2/95157))*vcc*(95157/(0.050*1421461.8)) );
   return 1;
}

/* ex: set sw=3 sta et : */
