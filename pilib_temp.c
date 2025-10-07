/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Library of functions to handle low-level details of access
 *   to SPI hardware and Power Insight carriers
 *
 * The Temperature bits...
 *   Functions to convert between temperatures and voltages
 *   for both PTS and K-type thermocouples.
 *   Based on the NIST and datasheet approximation functions.
 * TODO:
 *   Create table based interpolation function.
 *   Test for performance (time and accuracy) vs polynomials.
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

/**********
 * Coefficients from NIST tables for K-type thermocouples
 */

/******* Temp to Volts ********/

/* The equation below 0 °C is of the form
 * E = sum(i=0 to n) c_i t^i.
 */
/* Range: -270.000, 0.000, 11 coeff */
#define negTempLIMIT  -270.0
static const double negTemp[] = {
  0.000000000000E+00,
  0.394501280250E-01,
  0.236223735980E-04,
 -0.328589067840E-06,
 -0.499048287770E-08,
 -0.675090591730E-10,
 -0.574103274280E-12,
 -0.310888728940E-14,
 -0.104516093650E-16,
 -0.198892668780E-19,
 -0.163226974860E-22
};

/* The equation above 0 °C is of the form
 * E = sum(i=0 to n) c_i t^i + a0 exp(a1 (t - a2)^2).
 */
/* Range: 0.000, 1372.000, 10 coeff */
#define posTempLIMIT  1372.0
static const double posTemp[] = {
 -0.176004136860E-01,
  0.389212049750E-01,
  0.185587700320E-04,
 -0.994575928740E-07,
  0.318409457190E-09,
 -0.560728448890E-12,
  0.560750590590E-15,
 -0.320207200030E-18,
  0.971511471520E-22,
 -0.121047212750E-25
};
static const double
 a0 =  0.118597600000E+00,
 a1 = -0.118343200000E-03,
 a2 =  0.126968600000E+03 ;

double posTempAdj( double t )
{
    return a0 * exp( a1 * (t-a2)*(t-a2) );
}


/******* milliVolts to Temp ********/
#define invNegmVLIMIT  -5.891
static const double invNegmV[] = {  /* -200-0 degC, -5.891 to 0 mV */
         0.0000000E+00,
         2.5173462E+01,
        -1.1662878E+00,
        -1.0833638E+00,
        -8.9773540E-01,
        -3.7342377E-01,
        -8.6632643E-02,
        -1.0450598E-02,
        -5.1920577E-04
};
#define invPosmVLIMIT 20.644
static const double invPosmV[] = {  /* 0-500 degC, 0 to 20.644 mV */
         0.000000E+00,
         2.508355E+01,
         7.860106E-02,
        -2.503131E-01,
         8.315270E-02,
        -1.228034E-02,
         9.804036E-04,
        -4.413030E-05,
         1.057734E-06,
        -1.052755E-08
};


/* Evaluate polynomial at X given coefficients and adjustment function
 * @x  Value to evaluate poly at
 * @coeff  Array of coefficients (coeff[0] = a0, [1] = a1, etc.)
 * @n  maximum coefficient number of the array  (sizeof(coeff)/sizeof(*coeff)-1)
 * @adj()  Non-linear function to adjust the result
 */
static double evalPoly( double x, const double * coeff, ssize_t n, double (*adj)(double x) )
{
    double a = 0 ;

    for (  ; n > 0 ; --n ) {
        a += coeff[n] ;
        a *= x ;
    }
    a += *coeff ;

    if( adj != NULL ) {
        a += (*adj)(x) ;
    }

    return a ;
}


/* pi_volt2temp_K( reading, vref ) -- Convert voltage to temp, K-type
 * @reading -- "raw" reading expressed as a fraction [0,1) of vref
 * @vref -- Optional Vref for this reading (default 1.0)
 *          eg. Use 0.001 for result already in mV or 2.048 for
 *              "raw" reading as a fraction of Vref at 2.048 Volts
 * --------
 * Returns temperature in deg C
 */
int pi_volt2temp_K(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  vref ;
   lua_Number  temp ;

   reading = luaL_checknumber( L, 1 );
   vref = luaL_optnumber( L, 2, 1.0 );
   reading *= vref * 1000.0 ;  /* to milli-Volts */
   if( reading < invNegmVLIMIT || reading > invPosmVLIMIT ) {
#ifdef RANGE2ERROR
      return luaL_error( L, "bad arguments #1*#2 to volt2temp_K, out of range [%g, %g] mVolts", invNegmVLIMIT, invPosmVLIMIT );
#else
      lua_pushnumber( L, NAN );
      return 1 ;
#endif
   }

   if( reading < 0 ) {
      temp = evalPoly( reading, invNegmV, sizeof(invNegmV)/sizeof(*invNegmV)-1, NULL );
   } else {
      temp = evalPoly( reading, invPosmV, sizeof(invPosmV)/sizeof(*invPosmV)-1, NULL );
   }

   lua_pushnumber( L, temp );
   return 1 ;
}

/* pi_temp2volt_K( temp, vref ) -- Convert temperature to voltage, K-type
 * @temp -- Temperature in deg C
 * @vref -- Optional reference voltage to scale the result
 *          (eg. Use 0.001 for result in mV)
 * --------
 * Returns "raw" reading expressed as a ratio of vref (may exceed [0,1) )
 */
int pi_temp2volt_K(lua_State * L)
{
   lua_Number  temp ;
   lua_Number  vref ;
   lua_Number  reading ;

   temp = luaL_checknumber( L, 1 );
   vref = luaL_optnumber( L, 2, 1.0 );

   if( temp < negTempLIMIT || temp > posTempLIMIT ) {
#ifdef RANGE2ERROR
      return luaL_error( L, "bad argument #1 to temp2volt_K (out of range [%g, %g] degC)", negTempLimit, posTempLIMIT );
#else
      lua_pushnumber( L, NAN );
      return 1 ;
#endif
   }

   if( temp < 0 ) {
      reading = evalPoly( temp, negTemp, sizeof(negTemp)/sizeof(*negTemp)-1, NULL );
   } else {
      reading = evalPoly( temp, posTemp, sizeof(posTemp)/sizeof(*posTemp)-1, &posTempAdj );
   }
   reading /= 1000.0 * vref ; /* to Volts/Vref */

   lua_pushnumber( L, reading );
   return 1 ;
}


/**********
 * Coefficients from datasheet for PTS resistor in unitless measure of Rt/R0
 */

#define PTS_A  3.9083E-3
#define PTS_B  -5.775E-7
#define PTS_C  -4.183E-12

/* -55 to 0 deg C
 *
 *  Rt = R0 x (1 + A x T + B x T^2 + C x (T - 100)xT^3)
 */
#define negPTSLIMIT -55.0

/*  0 to 155 deg C
 *
 *  Rt/R0 = 1 + A x T + B x T^2
 *
 *  T = -A + sqrt( A*A - 4*B*(1- Rt/R0) )
 *      ---------------------------------
 *               2*B
 */
#define posPTSLIMIT 155.0

/* 27.0k pullup with .1% accuracy.
 *
 *   Rt/R0 = 27 * counts / (MaxCount - counts)
 *
 * With gain from PGA
 *   Rt/R0 = 27 * counts / (MaxCount*gain - counts)
 *
 * Using a ratio [0,1) reading for "counts"
 *   Rt/R0 = PULLUP * reading / (1 - reading)
 */

/* pi_rt2temp_PTS( reading, pullup ) -- Convert reading to temperature, PTS
 * @reading -- Ratio of Vref reading [0,1) for resistor divider (pullup over PTS)
 * @pullup -- Value of pullup resistor (as a ratio to R0 for the PTS resistor)
 *      eg. a 27k pullup with 1k PTS => 27k/1k => 27
 *          a 10k pullup with 100 Ohm PTS => 10k/100 => 100
 * --------
 * Returns temperature in degC
 */
int pi_rt2temp_PTS(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  pullup ;
   lua_Number  Rt_R0 ;
   lua_Number  temp ;

   reading = luaL_checknumber( L, 1 );
   luaL_argcheck( L, reading >= 0.0 && reading < 1.0, 1, "out of range [0,1)" );

   pullup = luaL_checknumber( L, 2 );

   Rt_R0 = (pullup*reading)/(1.0-reading) ;

   if( Rt_R0 < 0.8 || Rt_R0 > 1.6 ) {
      /* Out of range [-51,155] */
      temp = NAN ;
#ifdef RANGE2ERROR
      return luaL_error( L, "bad arguments #1, #2 to rt2temp_PTS (Rt/R0 out of range [%g, %g])", 0.8, 1.6 );
#endif
   } else {
      /* Good enough (<.5LSB) for 16 bits w/10k pullup from -50 to 155 */
      temp = (sqrt(PTS_A*PTS_A - 4*PTS_B + 4*PTS_B*Rt_R0) - PTS_A)/(2*PTS_B);
   }

   lua_pushnumber( L, temp );
   return 1 ;
}


/* pi_temp2rt_PTS( temp ) -- Convert temperature to resistance of PTS
 * @temp -- Temperature in degC
 * --------
 * Returns resistance ratio Rt/R0
 */
int pi_temp2rt_PTS(lua_State * L)
{
   lua_Number  temp ;

   temp = luaL_checknumber( L, 1 );
   luaL_argcheck( L, temp >= negPTSLIMIT && temp <= posPTSLIMIT, 1, "out of range [" tostr(negPTSLIMIT) "," tostr(posPTSLIMIT) "]" );

   if( temp < 0 ) {
      lua_pushnumber( L, (( (temp-100)*temp*PTS_C + PTS_B)*temp + PTS_A) * temp + 1 );
   } else {
      lua_pushnumber( L, (temp * PTS_B + PTS_A) * temp + 1 );
   }

   return 1 ;
}


/**********
 * Coefficients from datasheet for 44004 R25degC resistor with R25 = 2252 Ohms
 */

/* Steinhart-Hart Equation Constants for 44004 (2252 Ohm @ 25degC) +/- 0.2degC
 *
 *  T = 1/(A + B * ln(R) + C * ln(R)^3  (NOTE: T is in deg Kelvin, or "deg C + 273.15")
 *
 *  The inverse equation for Temp to Resistance is hairy with cube roots, exponentiation
 *     and other nasties...  We don't actually /need/ it.  Look it up on Wikipedia or other
 *     references if that changes
 */
#define R25_A  1.468E-3
#define R25_B  2.383E-4
#define R25_C  1.007E-7
#define R25_R  2252
#define R25_K  273.15

/* And actually, we came up with our own approximations using polynomials with
 *    the pullup resistor (1.820k) baked into the formulas
 *
 * 1820 Ohm pullup with .1% accuracy.
 *   Rt = 1820 * counts / (MaxCount - counts)
 *
 * Using a ratio reading [0,1) for "counts"
 *   Rt = 1820 * reading / (1 - reading)
 */

/* This thermistor has a -80 to 120 deg C Working Temp
 * But our counts to temp and back approximations are only
 *    accurate over -10 to 80 degC
 */
#define neg44004LIMIT -10.0
#define pos44004LIMIT 80.0
#define neg44004READING 550.0
#define pos44004READING 3600.0

/* pi_rt2temp_44004( reading ) -- Convert reading to temperature, Thermistor
 * @reading -- Ratio of Vref reading [0,1) for resistor divider (pullup over Rt)
 * --------
 * Returns temperature in degC
 */
int pi_rt2temp_44004(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  temp ;

   reading = luaL_checknumber( L, 1 );
   luaL_argcheck( L, reading >= 0.0 && reading < 1.0, 1, "out of range [0,1)" );

   /* We use a set of coefficents fit to readings from 500 to 3600 of 4096 counts */
   reading *= 4096.0 ;
   if( reading > pos44004READING || reading < neg44004READING ) {
      temp = NAN ;
#ifdef RANGE2ERROR
      return luaL_error( L, "argument #1 to rt2temp_44004 exceeds accuracy range [%g, %g]", neg44004READING, pos44004READING );
#endif
   } else {
      /* These values produce results with less than +/- 0.3 degC errors and assume 1820 Ohm pullup to 2252 Ohm thermistor */
      temp = 79.2012 - 0.0236906 * reading - 3.17644E-9 * (reading-2914.01)*(reading-2587.64)*(reading-1404.34) ;
   }

   lua_pushnumber( L, temp );
   return 1 ;
}


/* pi_temp2rt_44004( temp ) -- Convert temperature to resistance of thermistor
 * @temp -- Temperature in degC
 * --------
 * Returns resistance Rt
 */
int pi_temp2rt_44004(lua_State * L)
{
   lua_Number  temp ;

   temp = luaL_checknumber( L, 1 );
   luaL_argcheck( L, temp >= neg44004LIMIT && temp <= pos44004LIMIT, 1, "out of range [" tostr(neg44004LIMIT) "," tostr(pos44004LIMIT) "]" );

   /* These factors determined by exploring polynomial approximations of the SH equations in log/linear scale
    * For the given range of -10 to 80, they are good for better than +/- 1% error in resistance
    */
   temp += 273.15 ;  /* temp in Kelvin */
   lua_pushnumber( L, exp(22.2333 - 0.0488149*temp + 0.125765E-3*(temp-278.335)*(temp-281.323) ) );

   return 1 ;
}


/* ex: set sw=3 sta et : */
