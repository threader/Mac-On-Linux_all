/* 
 *   Creation Date: <1999/02/25 05:20:54 samuel>
 *   Time-stamp: <2001/12/29 18:24:45 samuel>
 *   
 *	<verbose.h>
 *	
 *	Assert / logging etc
 *   
 *   Copyright (C) 1999, 2000, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_VERBOSE
#define _H_VERBOSE

//#include "debugger.h"

#define SET_VERBOSE_NAME( name ) \
	static const char *_vp_vname=name;

/* Use LOG for messages which should always be printed */
#define LOG( format, args...) \
	printm( "%s: " format, _vp_vname , ## args)
#define LOG_( format, args...) \
	printm( format, ## args)

/* use LOG_ERR instead of perror */
#define LOG_ERR( format, args...) \
	({ printm( "----> (%s) " format, _vp_vname , ## args); \
	printm( ": %s\n", strerror(errno) ); })
#ifdef __linux__
#define LOG_ERR_() \
	({ printm( "----> (%s) line %d, %s(): %s\n", __FILE__, __LINE__, \
		   __FUNCTION__, strerror(errno) ); })
#else
#define LOG_ERR_() \
	({ printm( "----> (%s) line %d, : %s\n", __FILE__, __LINE__, \
		   strerror(errno) ); })
#endif /* __linux__ */

/* And VPRINT for debugging messages */

#ifdef VERBOSE
#define VPRINT(format, args...) \
	printm( "%s: " format, _vp_vname , ## args)
#define VPRINT_(format, args...) \
	printm( format, ## args)

#else
#define VPRINT(format...)    do {} while(0)
#define VPRINT_(format...)   do {} while(0)
#endif

#endif   /* _H_VERBOSE */

