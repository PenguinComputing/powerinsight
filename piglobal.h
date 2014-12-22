/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

#ifndef PIGLOBAL_H
#define PIGLOBAL_H

#include <lua.h>

/* Power Insight v2.x
 *
 * Globals provided by main wrapper around pilib
 *    and external interfaces to pilib
 */

/* The application name (typically argv[0]) */
extern char * ARGV0 ;

/* Location of suplimental files (libraries, Lua code, etc.) */
extern char * libexecdir ;
#ifndef PILIBDIR_DEFAULT
#define PILIBDIR_DEFAULT "/usr/lib/powerinsight"
#endif

/* Configuration file */
extern char * configfile ;
#ifndef PICONFIGFILE_DEFAULT
#define PICONFIGFILE_DEFAULT "/etc/powerinsight.conf"
#endif

/* Debug/verbose flags */
extern unsigned int  debug ;

/* 0 = quiet, 1 = normal detail, 2+ = more detail */
#define VERBOSE   0x000f
#define DBG_LUA   0x0010
#define DBG_SPI   0x0020

#ifndef PIDEBUG_DEFAULT
#define PIDEBUG_DEFAULT (1 & VERBOSE)
#endif
#define verbose  (debug & VERBOSE)
extern int verbose_set( int v );

/* Import the library into a Lua instance */
int pi_register(lua_State * L);

/* PI Library Lua helpers */
void luaPI_doerror( lua_State * L, int ret, const char * attempt );

#endif  /* PIGLOBAL_H */
/* ex: set sw=3 sta et : */
