/* 
 *   Creation Date: <2004/06/13 00:44:55 samuel>
 *   Time-stamp: <2004/06/13 00:49:06 samuel>
 *   
 *	<timer.h>
 *	
 *	
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_ASM_TIMER
#define _H_ASM_TIMER

static inline void get_timestamp( timestamp_t *timestamp ) {
	unsigned int tbu, tbl, tbu2;
	do {
		asm volatile("mftbu %0" : "=r" (tbu));
		asm volatile("mftb %0" : "=r" (tbl));
		asm volatile("mftbu %0" : "=r" (tbu2));
	} while( tbu2 != tbu );
	timestamp->hi = tbu;
	timestamp->lo = tbl;
}

static inline ullong get_mticks_( void ) { 
	unsigned int tbu, tbl, tbu2;
	do {
		asm volatile("mftbu %0" : "=r" (tbu));
		asm volatile("mftb %0" : "=r" (tbl));
		asm volatile("mftbu %0" : "=r" (tbu2));
	} while( tbu2 != tbu );
	return ((ullong)tbu << 32) | tbl;
}

static inline ulong get_tbl( void ) {
	ulong ret;
	asm volatile("mftb %0" : "=r" (ret) : );
	return ret;
}

#endif   /* _H_ASM_TIMER */
