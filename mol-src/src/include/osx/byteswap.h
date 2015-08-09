/* 
 *   Creation Date: <2004/01/16 20:00:58 samuel>
 *   Time-stamp: <2004/01/16 21:09:58 samuel>
 *   
 *	<byteswap.h>
 *	
 *	
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_OSX_BYTESWAP
#define _H_OSX_BYTESWAP

#define bswap_16(x)	( (((x) >> 8) & 0xff) | (((x) << 8) & 0xff00) )
#define bswap_32(x)	( (((x) >> 24) & 0xff) \
			  | (((x) >> 8) & 0xff00) \
			  | (((x) << 8) & 0xff0000) \
			  | (((x) << 24) & 0xff000000))

#endif   /* _H_OSX_BYTESWAP */
