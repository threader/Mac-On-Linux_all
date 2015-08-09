/* 
 *   Creation Date: <2003/05/27 17:06:30 samuel>
 *   Time-stamp: <2003/05/27 17:44:08 samuel>
 *   
 *	<tlbie.h>
 *	
 *	
 *   
 *   Copyright (C) 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_TLBIE
#define _H_TLBIE

extern void	(*xx_store_pte_lowmem)( unsigned long *slot, int pte0, int pte1 );

static inline void __tlbie( int ea ) {
	asm volatile ("tlbie %0" : : "r"(ea));
}

extern void	__store_PTE( int ea, unsigned long *slot, int pte0, int pte1 );

#endif   /* _H_TLBIE */
