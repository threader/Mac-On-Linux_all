/* 
 *   Creation Date: <1999/03/20 07:38:38 samuel>
 *   Time-stamp: <2002/07/20 19:29:22 samuel>
 *   
 *	<logger.h>
 *	
 *	Interface to some logging functions
 *   
 *   Copyright (C) 1999, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef __LOGGER_H_
#define __LOGGER_H_

#include <Types.h>

#ifdef DEBUG
#define DBG(x)	do {x;} while(0)
#else
#define	DBG(x)  do {} while(0)
#endif

extern int	lprintf( char *fmt,... );
extern OSStatus	CheckStatus( OSStatus value, char *message);

#endif
