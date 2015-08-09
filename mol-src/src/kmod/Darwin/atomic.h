/* 
 *   Creation Date: <2004/01/25 16:53:23 samuel>
 *   Time-stamp: <2004/01/25 22:28:55 samuel>
 *   
 *	<atomic.h>
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

#ifndef _H_ATOMIC
#define _H_ATOMIC

#include <libkern/OSAtomic.h>

typedef SInt32 mol_atomic_t;

static inline int atomic_inc_return_mol( mol_atomic_t *at ) { 
	return OSIncrementAtomic(at) + 1; 
}
static inline void atomic_inc_mol( mol_atomic_t *at ) { 
	OSIncrementAtomic(at); 
}
static inline void atomic_dec_mol( mol_atomic_t *at ) { 
	OSDecrementAtomic(at);
}
static inline SInt32 atomic_read_mol( mol_atomic_t *at ) {
	return *at;
}

#endif   /* _H_ATOMIC */



