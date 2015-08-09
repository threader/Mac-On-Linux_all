/* 
 *   Creation Date: <97/06/21 17:04:48 samuel>
 *   Time-stamp: <2003/06/06 18:25:33 samuel>
 *   
 *	<memory.h>
 *	
 *	memory mapping and ranslation
 *   
 *   Copyright (C) 1997, 1999, 2000, 2001, 2002, 2003 Samuel Rydh
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_MEMORY
#define _H_MEMORY

#include "mmu_mappings.h"

/* ---- extern variables ---- */

extern struct	mmu_mapping ram;
extern struct	mmu_mapping rom;

extern void	mem_init( void );
extern void	mem_cleanup( void );

extern char	*map_zero( char *wanted_ptr, size_t size );
extern char 	*map_phys_mem( char *wanted_ptr, ulong phys_ptr, size_t size, int prot );
extern int	unmap_mem( char *start, size_t length );

/* --- translation --- */

extern int 	mphys_to_lvptr( ulong mptr, char **ret );	/* 0=ram, 1=rom, <0 unmapped */
extern char 	*transl_ro_mphys( ulong mphys );		/* translates rom or ram */
extern void 	*transl_mphys( ulong mphys );

/* inline version for performance critical sections */
static inline void * 
transl_mphys_( ulong mphys ) {
	ulong offs = mphys - ram.mbase;
	return (offs < (ulong)ram.size) ? ram.lvbase + offs : NULL;
}
static inline int
lvptr_is_ram( char *lvptr ) {
	return (ulong)(lvptr - ram.lvbase) < (ulong)ram.size;
}
static inline int
lvptr_is_rom( char *lvptr ) {
	return (ulong)(lvptr - rom.lvbase) < (ulong)rom.size;
}
static inline int
mphys_is_rom( ulong mphys ) {
	return (ulong)(mphys - rom.mbase) < (ulong)rom.size;	/* this also handles the size=0 case */
}

#ifdef OLDWORLD_SUPPORT
extern int	rom_is_writeable( void );
#else
static inline int rom_is_writeable( void ) { return 0; }
#endif
	

/* --- from lib/cache.S --- */
extern void	flush_dcache_range( char *start, char *stop );
extern void	flush_icache_range( char *start, char *stop );

#endif

