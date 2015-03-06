/* Shared library interface to Power Insight v2.1
 * For use with carrier board 10016423 and 10020355
 *	and expansion board 10019889 (Temperature)
 *
 * NOTE: This is different from a previous version of this
 *	interface.  The structure is changed and the
 *	function interfaces have a different signature.
 *
 * Copyright (C) Penguin Computing and Sandia National Lab
 *	2013, 2014, 2015
 */
#ifndef PIDEV_H
#define PIDEV_H

#define MAX_PORTNUM  60

typedef struct {
    union { double  reading, watt, temp ; };
    double  volt ;  /* Voltage component of Watt measurement */
    double  amp ;  /* Amperage component */
} reading_t ;

/* Change default global parameters.  Call before calling pidev_open */
int pidev_setup(
        char * ARGV0,  /* printed in error messages */
        char * LIBEXECDIR,  /* location of xxx.lc files */
        char * CONFIGFILE,  /* sensor config file */
        unsigned int DEBUG,  /* debug flags for debug messages */
        int VERBOSE  /* Increase verbosity */
    );

/* Call to initialize the library, MUST be called before read, etc. */
int pidev_open( void );

/* Read a sensor named "J#" where # is the portNumber */
int pidev_read( int portNumber, reading_t * sample );

/* Read a sensor named "T#" where # is the portNumber */
int pidev_temp( int portNumber, reading_t * sample );

/* Read a sensor by name */
int pidev_read_byname( char * name, reading_t * sample );

/* Close the library */
int pidev_close( void );

/* The above functions return 0 on success and one of the following
 *      on failure
 */
#define PIERR_SUCCESS  0
#define PIERR_NOSAMPLE  -1
#define PIERR_NOTFOUND  -2
#define PIERR_ERROR  -3

/* ex: set sw=4 sta et : */
#endif
