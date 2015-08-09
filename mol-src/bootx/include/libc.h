/* 
 *   Creation Date: <2002/07/20 13:05:15 samuel>
 *   Time-stamp: <2002/07/23 13:27:33 samuel>
 *   
 *	<libc.h>
 *	
 *	Minimal set of headers
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *
 */

#ifndef _H_LIBC
#define _H_LIBC

/* Types */
#include <sys/types.h>
#ifndef ulong
#define ulong			unsigned long
#endif /* ulong */

/* Byteswap */
#ifdef __linux__
#include <byteswap.h>
#endif /* __linux__ */

#ifdef __darwin__
#include <architecture/byte_order.h>
#define bswap_16(x)		OSSwapInt16(x)
#define bswap_32(x)		OSSwapInt32(x)
#endif /* __darwin__ */

#define printf	printm
extern int	printm( const char *fmt, ... );

#include "libclite.h"

#ifndef __P		/* prototype support */
#define	__P(x)	x
#endif

#ifndef bswap16
#define bswap16	bswap_16
#define bswap32	bswap_32
#endif

#endif   /* _H_LIBC */
