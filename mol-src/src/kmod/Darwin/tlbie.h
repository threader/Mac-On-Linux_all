/* 
 *   Creation Date: <2004/01/25 16:28:32 samuel>
 *   Time-stamp: <2004/03/13 19:11:18 samuel>
 *   
 *	<tlbie.h>
 *	
 *	TLBIE 
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_TLBIE
#define _H_TLBIE

extern void 		(*xx_tlbie_lowmem)( void /* special */ );
extern void		(*xx_store_pte_lowmem)( void /* special */ );
extern int		compat_hash_table_lock;

static inline void
__tlbie( int ea )
{
	register ulong _ea		__asm__ ("r3");
	register ulong _lock		__asm__ ("r7");
	register ulong _func		__asm__ ("r9");

	_func = (ulong)xx_tlbie_lowmem;
	_lock = (ulong)&compat_hash_table_lock;
	_ea = ea;
	
	asm volatile (
		"mtctr	r9		\n"
		"li	r8,0x1235	\n"	/* lock value */
		"mfmsr	r10		\n"
		"rlwinm	r0,r10,0,17,15	\n"	/* clear MSR_EE */
		"mtmsr	r0		\n"
		"bctrl			\n"	/* modifies r0 */
		"mtmsr	r10		\n"
		: 
		: "r" (_ea), "r" (_lock), "r" (_func)
		: "ctr", "lr", "cc", "r8", "r0", "r10"
	);
}

static inline void
__store_PTE( int ea, unsigned long *slot, int pte0, int pte1  )
{
	register ulong _ea		__asm__ ("r3");
	register ulong _pte_slot	__asm__ ("r4");
	register ulong _pte0		__asm__ ("r5");
	register ulong _pte1		__asm__ ("r6");
	register ulong _lock		__asm__ ("r7");
	register ulong _func		__asm__ ("r9");

	_func = (ulong)xx_store_pte_lowmem;
	_ea = ea;
	_pte_slot = (ulong)slot;
	_pte0 = pte0;
	_pte1 = pte1;
	_lock = (ulong)&compat_hash_table_lock;
	
	asm volatile (
		"mtctr	r9		\n"
		"li	r8,0x1234	\n"	/* lock value */
		"mfmsr	r10		\n"
		"rlwinm	r0,r10,0,17,15	\n"	/* clear MSR_EE */
		"mtmsr	r0		\n"
		"bctrl			\n"	/* modifies r0 */
		"mtmsr	r10		\n"
		: 
		: "r" (_ea), "r" (_pte_slot), "r" (_pte0), "r" (_pte1), "r" (_lock), "r" (_func)
		: "ctr", "lr", "cc", "r0", "r8", "r10"
	);
}

#if 0
/************************************************************************/
/*	non-SMP implementation						*/
/************************************************************************/

static inline void
__tlbie( int ea )
{
	asm volatile ("tlbie %0" : : "r"(ea));
}


static inline void
__store_PTE( int ea, unsigned long *slot, int pte0, int pte1 )
{
	/* XXX fixme */
	slot[0] = 0;
	slot[1] = pte1;
	slot[0] = pte0;
	__tlbie( ea );      
}
#endif


#endif   /* _H_TLBIE */
