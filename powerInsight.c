/* Copyright (c) 2014  Penguin Computing, Inc.
 *  All rights reserved
 */

/* Power Insight v2.x
 *
 * Support (through config file options) Power Insight v1.0 and v2.1 hardware
 *   including temperature expansion board.
 * Provide readings via command line and library for linking with PIAPI
 *   effort from Sandia National Lab.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <lua.h>
#include <lauxlib.h>
#include <lualib.h>
#include "powerInsight.h"

/* Global variables */
char * ARGV0 = "powerInsight" ;
char * configfile = "/etc/powerinsight.conf" ;  /* DEFAULT_CONFIGFILE */
char * libexecdir = "/usr/lib/powerinsight" ;  /* DEFAULT_LIBDIR */
lua_State * L = NULL ;
unigned int  debug = 0 ;
#define DBG_LUA   0x0001
#define DBG_SPI   0x0002
int verbose = 0 ;  /* -1 = quiet, 1 = more detail */

/* Parse options.
 * Return 0 if OK, -1 if error (print usage and exit)
 */
int parseOptions( argc, char** argv )
{
   int  option ;
   int  usage = 0 ;

   /* Save command name for errors and other output */
   ARGV0 = argv[0] ;

   while( -1 != (option = getopt( argc, argv, "uvd:c:D:" )) ) {
      switch( option ) {

      case '?' : /* fallthrough */
      case 'u' : usage = -1 ; break ;

      case 'v' : ++verbose ; break ;
      case 'd' :
         debug |= strtoul( optarg, NULL, 0 );
         if( verbose >= 2 )
            fprintf( stderr, "Debug flags now: %04x\n", debug );
         break ;
      case 'c' :
         configfile = optarg ;
         if( verbose >= 1 )
            fprintf( stderr, "Configfile now: %s\n", configfile );
         break ;
      case 'D' :
         libexecdir = optarg ;
         if( verbose >= 1 )
            fprintf( stderr, "Library directory now: %s\n", libexecdir );
         break ;
      }
   }

   return usage ;
   /* Also, argv[optind] is the first non-option argument */
}

void usage( )
{
   printf(
"usage: %s [-u] [-v] [-d flags] [-c file] [-D directory] [chan ...]\n"
"where:\n"
"   -u  print this usage menu\n"
"   -v  increments verbosity\n"
"   -d  set one or more debug flags (bitmask)\n"
"   -c  specify location of configuration file\n"
"   -D  specify prefix for library files\n"
"   chan  one or more channels to read and report:\n"
"         eg: J1, J2, J3, ..., J15\n"
"             T0 -- Main carrier temperature\n"
"             Vcc -- Main carrier Vcc\n"
"             T1, T2, ..., T6, T7, T8\n"
"             TJ1, TJ2 -- Temp carrier junction temps\n"
         ARGV0 );
   exit( 1 );
}



/* vim: set sw=3 sta et : */
