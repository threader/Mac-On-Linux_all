/* 
 *   Creation Date: <2003/05/27 15:45:23 samuel>
 *   Time-stamp: <2004/03/13 13:28:37 samuel>
 *   
 *	<alloc.h>
 *	
 *	Memory allocation and mappings
 *   
 *   Copyright (C) 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_ALLOC
#define _H_ALLOC

#ifdef __cplusplus
extern "C" {
#endif

extern void 	*kmalloc_mol( int size );
extern void 	kfree_mol( void *addr );
extern void 	*vmalloc_mol( int size );
extern void 	vfree_mol( void *p );
extern void 	*kmalloc_cont_mol( int size );
extern void 	kfree_cont_mol( void *addr );
extern ulong 	tophys_mol( void *addr );
extern void 	memory_allocator_cleanup( void );

static inline ulong alloc_page_mol( void ) {
	char *p = (char*)IOMallocContiguous( 0x1000, 0x1000, NULL );
	if( p )
		memset( p, 0, 0x1000 );
	return (ulong)p;
}
static inline void free_page_mol( ulong addr ) {
	IOFreeContiguous( (void*)addr, 0x1000 );
}

static inline void flush_icache_mol( ulong start, ulong stop ) {
	flush_dcache( (vm_offset_t)start, stop-start, FALSE /* virt addr */ );
	invalidate_icache( (vm_offset_t)start, stop-start, FALSE /* virt addr */);
}

#ifdef __cplusplus
}
#endif

#endif   /* _H_ALLOC */
