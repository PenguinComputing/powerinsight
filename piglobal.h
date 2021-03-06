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

/* General verbosity */
extern int verbose ;
/* -1 = quiet, 0 = normal detail, 1+ = more detail */
#ifndef PIVERBOSE_DEFAULT
#define PIVERBOSE_DEFAULT 0
#endif

/* Debug flags */
extern unsigned int  debug ;
#define DBG_LUA   0x0001
#define DBG_SPI   0x0002
#define DBG_WAIT  0x0004
#define DBG_PIDEV 0x0008

#ifndef PIDEBUG_DEFAULT
#define PIDEBUG_DEFAULT 0
#endif

/* Macro to declare functions for export when creating a shared library.
 *
 * See Makefile for how this gets converted to a list of symbols to
 *      export for specific targets.  Here, we define it to make the
 *      directive go away when compiling the .c file...
 */
#define PIEXPORT(x)

/* Import the library into a Lua instance */
int pi_register(lua_State * L);

/* PI Library Lua helpers */
void luaPI_doerror( lua_State * L, int ret, const char * attempt );

#endif  /* PIGLOBAL_H */
/* ex: set sw=3 sta et : */
