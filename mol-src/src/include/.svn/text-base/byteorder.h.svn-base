/* 
 *   Creation Date: <2001/10/29 00:29:21 samuel>
 *   Time-stamp: <2004/01/17 14:08:25 samuel>
 *   
 *	<byteorder.h>
 *	
 *	Big endian/Little endian conversion
 *   
 *   Copyright (C) 2001, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *
 *   The swap macros are borrowed from the the swap.h linux header.
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_BYTEORDER
#define _H_BYTEORDER

#ifdef __darwin__
#include "asm/byteswap.h"
#else
#include <byteswap.h>
#endif
#include "cpu/mol_byteorder.h"

#ifdef __powerpc__
#if BYTE_ORDER == LITTLE_ENDIAN
#error "FIX ENDIAN!"
#endif
#endif

#define be64_to_cpu( x ) cpu_to_be64( x )
#define le64_to_cpu( x ) cpu_to_le64( x )
#define be32_to_cpu( x ) cpu_to_be32( x )
#define le32_to_cpu( x ) cpu_to_le32( x )
#define be16_to_cpu( x ) cpu_to_be16( x )
#define le16_to_cpu( x ) cpu_to_le16( x )

#if BYTE_ORDER == BIG_ENDIAN

#define BE32_FIELD( dummy )	do {} while(0)
#define BE16_FIELD( dummy )	do {} while(0)

static __inline__ unsigned	ld_be16( const unsigned short *addr) { return *addr; }
static __inline__ void		st_be16( unsigned short *addr, unsigned short val) { *addr = val; }
static __inline__ unsigned	ld_be32( const ulong *addr) { return *addr; }
static __inline__ void		st_be32(ulong *addr, ulong val) { *addr = val; }

#else /* BYTE_ORDER == BIG_ENDIAN */

/* careful - these macros are not typechecked */
#define BE32_FIELD( field )	do { field = ld_be32( (ulong*)&field ); } while (0)
#define BE16_FIELD( field )	do { field = ld_be16( (ushort*)&field ); } while (0)

static __inline__ unsigned	ld_le16( const unsigned short *addr) { return *addr; }
static __inline__ void		st_le16( unsigned short *addr, unsigned short val) { *addr = val; }
static __inline__ unsigned	ld_le32(const ulong *addr) { return *addr; }
static __inline__ void		st_le32(ulong *addr, ulong val) { *addr = val; }

#endif /* BYTE_ORDER == BIG_ENDIAN */

#endif   /* _H_BYTEORDER */
