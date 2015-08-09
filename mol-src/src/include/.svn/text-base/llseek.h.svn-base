/* 
 *   Creation Date: <1999/03/29 05:04:40 samuel>
 *   Time-stamp: <2003/05/23 15:40:01 samuel>
 *   
 *	<long_lseek.h>
 *	
 *	64bit lseek
 *   
 *   Copyright (C) 1999, 2000, 2001, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_LONG_LSEEK
#define _H_LONG_LSEEK

#include <unistd.h>

static inline int
blk_lseek( int fd, long block, long offs )
{
	if( offs & ~0x1ff )
		return -2;
#ifdef OFF_T_IS_64
	return lseek( fd, ((off_t)block << 9) | offs, SEEK_SET ) == -1 ? -1 : 0;
#else
	return lseek64( fd, ((llong)block<<9) | offs, SEEK_SET ) == -1 ? -1 : 0;
#endif
}

#ifdef OFF_T_IS_64
#define open64 open
#endif

#if 0
static inline llong ltell( int fd ) {
	return lseek64( fd, 0, SEEK_CUR );
}
#endif

/* util */
extern ulong get_file_size_in_blks( int fd );

#endif   /* _H_LONG_LSEEK */
