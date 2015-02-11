/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

#ifndef PILIB_H
#define PILIB_H

/* Library of functions to handle low-level details of access
 *   to SPI hardware and Power Insight carriers
 */

#include <lua.h>

/* Library methods */
int pi_open(lua_State * L);
int pi_write(lua_State * L);
int pi_read(lua_State * L);
int pi_spi_mode(lua_State * L);
int pi_spi_maxspeed(lua_State * L);
int pi_spi_message(lua_State * L);
int pi_i2c_device(lua_State * L);
int pi_i2c_read(lua_State * L);
int pi_i2c_write(lua_State * L);
int pi_ads1256_init(lua_State * L);
int pi_ads1256_wait4DRDY(lua_State * L);
int pi_ads1256_getraw(lua_State * L);
int pi_ads1256_setmux(lua_State * L);
int pi_ads1256_getraw_setmuxC(lua_State * L);
int pi_ads1256_rxbuf2raw(lua_State *L);
/* int pi_ads8344_init(lua_State * L); *** Declared in init_final.lua */
int pi_ads8344_mkmsg(lua_State * L);
int pi_ads8344_getraw(lua_State * L);
/* int pi_mcp3008_init(lua_State * L); *** Declared in init_final.lua */
int pi_mcp3008_mkmsg(lua_State * L);
int pi_mcp3008_getraw(lua_State * L);
int pi_sc620_init(lua_State * L);
int pi_setbank(lua_State * L);
int pi_sens_5v(lua_State * L);
int pi_sens_12v(lua_State * L);
int pi_sens_3v3(lua_State * L);
int pi_sens_acs713_20(lua_State * L);
int pi_sens_acs713_30(lua_State * L);
int pi_sens_acs723_10(lua_State * L);
int pi_sens_acs723_20(lua_State * L);
int pi_sens_shunt10(lua_State * L);
int pi_sens_shunt25(lua_State * L);
int pi_sens_shunt50(lua_State * L);
int pi_volt2temp_K(lua_State * L);
int pi_temp2volt_K(lua_State * L);
int pi_rt2temp_PTS(lua_State * L);
int pi_temp2rt_PTS(lua_State * L);
int pi_setled_temp(lua_State * L);
int pi_setled_main(lua_State * L);
int pi_gettime(lua_State * L);
int pi_filter(lua_State * L);
int pi_AddConnectors(lua_State * L);
int pi_Sensors(lua_State * L);
int pi_AddSensors(lua_State * L);

#endif /* PILIB_H */

/* ex: set sw=3 sta et : */
