/* 
 *   Creation Date: <2004/01/16 14:46:00 samuel>
 *   Time-stamp: <2004/01/16 17:08:39 samuel>
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

#ifndef _H_PPC_BYTEORDER
#define _H_PPC_BYTEORDER

#include "mol_config.h"

#ifdef __darwin__
#include <machine/endian.h>
#else
#include <endian.h>
#endif

static __inline__ unsigned ld_le16( const unsigned short *addr) {
        unsigned val;
        __asm__ ("lhbrx %0,0,%1" : "=r" (val) : "r" (addr), "m" (*addr) );
        return val;
}
static __inline__ void st_le16( unsigned short *addr, unsigned short val) {
        __asm__ ("sthbrx %1,0,%2" : "=m" (*addr) : "r" (val), "r" (addr) );
}
static __inline__ unsigned ld_le32(const ulong *addr) {
        unsigned val;
        __asm__ ("lwbrx %0,0,%1" : "=r" (val) : "r" (addr), "m" (*addr) );
        return val;
}
static __inline__ void st_le32(ulong *addr, ulong val) {
        __asm__ ("stwbrx %1,0,%2" : "=m" (*addr) : "r" (val), "r" (addr) );
}

#define cpu_to_be64( x ) ( x )
#define cpu_to_le64( x ) \
({ \
	u64 __x = (x); \
	((u64)( \
		(u64)(((u64)(__x) & (u64)0x00000000000000ffULL) << 56) | \
		(u64)(((u64)(__x) & (u64)0x000000000000ff00ULL) << 40) | \
		(u64)(((u64)(__x) & (u64)0x0000000000ff0000ULL) << 24) | \
		(u64)(((u64)(__x) & (u64)0x00000000ff000000ULL) <<  8) | \
	        (u64)(((u64)(__x) & (u64)0x000000ff00000000ULL) >>  8) | \
		(u64)(((u64)(__x) & (u64)0x0000ff0000000000ULL) >> 24) | \
		(u64)(((u64)(__x) & (u64)0x00ff000000000000ULL) >> 40) | \
		(u64)(((u64)(__x) & (u64)0xff00000000000000ULL) >> 56) )); \
})

#define cpu_to_be32( x ) ( x )
#define cpu_to_le32( x ) \
({ \
        u32 __x = (x); \
        ((u32)( \
                (((u32)(__x) & (u32)0x000000ffUL) << 24) | \
                (((u32)(__x) & (u32)0x0000ff00UL) <<  8) | \
                (((u32)(__x) & (u32)0x00ff0000UL) >>  8) | \
                (((u32)(__x) & (u32)0xff000000UL) >> 24) )); \
})


#define cpu_to_be16( x ) ( x )
#define cpu_to_le16( x ) \
({ \
        u16 __x = (x); \
        ((u16)( \
                (((u16)(__x) & (u16)0x00ffU) << 8) | \
                (((u16)(__x) & (u16)0xff00U) >> 8) )); \
})


#endif   /* _H_BYTEORDER */
