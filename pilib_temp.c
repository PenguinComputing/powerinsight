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


/******* miliVolts to Temp ********/
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
         8.802193E-06,
        -3.110810E-08
};


/* Evaluate polynomial at X given coefficients and adjustment function */
static double evalPoly( double x, const double * coeff, ssize_t n, double (*adj)(double x) )
{
    double a = 0 ;

    for ( ; n > 0 ; --n ) {
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
   reading *= vref * 1000.0 ;  /* to mili-Volts */
   if( reading < invNegmVLIMIT || reading > invPosmVLIMIT ) {
      return luaL_error( L, "bad arguments to volt2temp_K, out of range [%f,%f]", invNegmVLIMIT/1000.0, invPosmVLIMIT/1000.0 );
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

   luaL_argcheck( L, temp >= negTempLIMIT && temp <= posTempLIMIT, 1, "out of range [" tostr(negTempLIMIT) "," tostr(posTempLIMIT) "]" );

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
 * Coefficients from datasheet for PTS 1k resistor
 */

#define PTS1k_A  3.9083E-3
#define PTS1k_B  -5.775E-7
#define PTS1k_C  -4.183E-12

/* -55 to 0 deg C
 *
 *  Rt = R0 x (1 + A x T + B x T^2 + C x (T - 100)xT^3)
 */

/*  0 to 155 deg C
 *
 *  Rt/R0 = 1 + A x T + B x T^2
 *
 *  T = -A + sqrt( A*A - 4*B*(1- Rt/R0) )
 *      ---------------------------------
 *               2*B
 */

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
#define posPTSLIMIT 155.0

/* pi_rt2temp_PTS( reading, pullup ) -- Convert reading to temperature, PTS
 * @reading -- Ratio of Vref reading [0,1) for resistor divider (pullup over PTS)
 * @pullup -- Value of pullup resistor (in kOhms)
 * --------
 * Returns temperature in degC
 */
int pi_rt2temp_PTS(lua_State * L)
{
   lua_Number  reading ;
   lua_Number  pullup ;
   lua_Number  Rt_R0 ;

   reading = luaL_checknumber( L, 1 );
   luaL_argcheck( L, reading >= 0.0 && reading < 1.0, 1, "out of range [0,1)" );

   pullup = luaL_checknumber( L, 2 );

   Rt_R0 = (pullup*reading)/(1.0-reading) ;

   lua_pushnumber( L, (sqrt(PTS1k_A*PTS1k_A-4*PTS1k_B + 4*PTS1k_B*Rt_R0) - PTS1k_A)/(2*PTS1k_B) );

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
   luaL_argcheck( L, temp >= 0 && temp <= posPTSLIMIT, 1, "out of range [0," tostr(posPTSLIMIT) "]" );

   lua_pushnumber( L, (temp * PTS1k_B + PTS1k_A) * temp + 1 );

   return 1 ;
}

/* ex: set sw=3 sta et : */
