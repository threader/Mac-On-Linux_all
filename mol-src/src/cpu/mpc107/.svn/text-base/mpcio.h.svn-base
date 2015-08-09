/* 
 *   Creation Date: <2003/05/28 14:40:12 samuel>
 *   Time-stamp: <2003/05/28 14:49:58 samuel>
 *   
 *	<mpcio.h>
 *	
 *	MPC I/O access
 *   
 *   Copyright (C) 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_MPCIO
#define _H_MPCIO


/************************************************************************/
/*	I/O access							*/
/************************************************************************/

extern char *_mpc_pcsr;

static inline void
out_le32( volatile u32 *addr, u32 val )
{
        __asm__ __volatile__("stwbrx %1,0,%2; eieio" : "=m" (*addr) :
                             "r" (val), "r" (addr));
}
static inline void
_out_le32( volatile u32 *addr, u32 val )
{
        __asm__ __volatile__("stwbrx %1,0,%2" : "=m" (*addr) :
                             "r" (val), "r" (addr));
}
static inline u32
in_le32( volatile u32 *addr )
{
	u32 ret;
        __asm__ __volatile__("lwbrx %0,0,%1;\n"
			     "twi 0,%0,0;\n"
			     "isync" : "=r" (ret) :
			     "r" (addr), "m" (*addr));
        return ret;
}
static inline u32
_in_le32( volatile u32 *addr )
{
	u32 ret;
        __asm__ __volatile__("lwbrx %0,0,%1;\n"
			     : "=r" (ret) :
			     "r" (addr), "m" (*addr));
        return ret;
}

static inline void
pcsr_write( u32 val, int offs )
{
	out_le32( (u32*)(_mpc_pcsr + offs), val );
}

static inline u32
pcsr_read( int offs )
{
	return in_le32( (u32*)(_mpc_pcsr + offs) );
}


#endif   /* _H_MPCIO */
