/* 
 *   Creation Date: <2004/01/16 22:17:01 samuel>
 *   Time-stamp: <2004/06/12 19:03:12 samuel>
 *   
 *	<byteorder.h>
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

#ifndef _H_X86_BYTEORDER
#define _H_X86_BYTEORDER

#include <endian.h>
#include <byteswap.h>

static __inline__ unsigned	ld_be16( const unsigned short *addr) { return bswap_16( *addr ); }
static __inline__ void		st_be16( unsigned short *addr, unsigned short val) { *addr = bswap_16(val); }
static __inline__ unsigned	ld_be32(const ulong *addr) { return bswap_32( *addr ); }
static __inline__ void		st_be32(ulong *addr, ulong val) { *addr = bswap_32(val); }

#define cpu_to_le32(x)		((u32)(x))
#define cpu_to_le16(x)		((u16)(x))

#endif   /* _H_X86_BYTEORDER */
