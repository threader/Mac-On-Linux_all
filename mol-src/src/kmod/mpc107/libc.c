/* 
 *   Creation Date: <2003/05/27 14:36:23 samuel>
 *   Time-stamp: <2003/05/27 17:27:04 samuel>
 *   
 *	<libc.c>
 *	
 *	  mini library implementation.
 *
 *   Copyright (C) 2003 Samuel Rydh (samuel@ibrium.se)
 *
 *   Some of the functions originate from linux/lib/string.c:
 *	
 *	  Copyright (C) 1991, 1992  Linus Torvalds
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#include "archinclude.h"

void *
memcpy( void *dest, const void *src, size_t count )
{
	char *tmp = (char*)dest;
	char *s = (char*)src;

	while( count-- )
		*tmp++ = *s++;

	return dest;
}

void *
memmove( void * dest, const void *src, size_t count )
{
	char *tmp, *s;

	if( dest <= src ) {
		tmp = (char*)dest;
		s = (char*)src;
		while (count--)
			*tmp++ = *s++;
	}
	else {
		tmp = (char*)dest + count;
		s = (char*)src + count;
		while( count-- )
			*--tmp = *--s;
	}
	return dest;
}

void *
memset(void * s,int c,size_t count)
{
	char *xs = (char*)s;

	while (count--)
		*xs++ = c;
	return s;
}

