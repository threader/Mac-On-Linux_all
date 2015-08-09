/* 
 *   Creation Date: <97/06/21 17:01:31 samuel>
 *   Time-stamp: <2004/03/13 14:37:56 samuel>
 *   
 *	<memory.c>
 *	
 *	Memory functions
 *   
 *   Copyright (C) 1997, 1999, 2000, 2001, 2002, 2003, 2004 Samuel Rydh
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 */

#include "mol_config.h"
#include <sys/mman.h>
#include "debugger.h"
#include "memory.h"
#include "wrapper.h"
#include "promif.h"
#include "mmu_mappings.h"
#include "res_manager.h"
#include "verbose.h"
#include "booter.h"
#include "session.h"

/* #define DEBUG_LOCK_MEM  */
#define WANTED_RAM_BASE		((char*)0x40000000)
#define	DEFAULT_RAM_SIZE	64
/* #define SESSION_SAVE_SUPPORT	 */

struct mmu_mapping		ram, rom;
static int			fmapped_ram;		/* file-mapped RAM */


/************************************************************************/
/*	session support							*/
/************************************************************************/

#ifdef SESSION_SAVE_SUPPORT
static int
save_ram( void )
{
	int size,i, j, fd, wsize;
	char *lvbase, *p;
	off_t offs;

	if( fmapped_ram ) {
		if( msync(ram.lvbase, ram.size, MS_SYNC) < 0 ) {
			perrorm("msync");
			return 1;
		}
		return 0;
	}

	/* save dirty page information... */
	if( !(size=_get_dirty_RAM(NULL)) )
		return 1;

	p = alloca(size);
	_get_dirty_RAM( p );

	if( write_session_data("RAMp", 0, p, size) )
		return 1;

	/* prepare to save RAM */
	align_session_data( 0x1000 );

	if( write_session_data("RAM", 0, NULL, ram.size) )
		return 1;
	if( get_session_data_fd("RAM", 0, &fd, &offs, NULL) )
		return 1;

	lseek( fd, offs, SEEK_SET );
	ftruncate( fd, offs );

	/* ...and the actual data */
	lvbase = ram.lvbase;
	for( wsize=0, i=0; i<size; i++ ) {
		for( j=0; j<8 && lvbase + wsize < ram.lvbase + ram.size; j++ ) {
			if( p[i] & (1<<j) ) {
				wsize += 0x1000;
			} else {
				if( wsize && write(fd, lvbase, wsize) != wsize )
					return 1;
				lvbase += wsize + 0x1000;

				wsize = 0;
				lseek( fd, 0x1000, SEEK_CUR );
			}
		}
	}
	if( wsize && write(fd, lvbase, wsize) != wsize )
		return 1;

	return 0;
}
#endif

static void
load_ram( void )
{
	ssize_t size;
	off_t offs;
	int fd;

	if( get_session_data_fd("RAM", 0, &fd, &offs, &size) )
		fatal("failed restoring RAM");

	ram.flags = 0;
	ram.size = size;
	ram.lvbase = mmap( WANTED_RAM_BASE, ram.size, 
			   PROT_EXEC | PROT_READ | PROT_WRITE, MAP_SHARED, fd, offs );
	if( ram.lvbase == MAP_FAILED )
		fatal("could not map RAM");
	fmapped_ram = 1;
}


/************************************************************************/
/*	mapping functions						*/
/************************************************************************/

char *
map_zero( char *wanted_addr, size_t size )
{
	char *ptr;

	ptr = mmap( wanted_addr, size, PROT_EXEC | PROT_READ | PROT_WRITE, 
		    MAP_ANON | MAP_PRIVATE, -1, 0 );

	if( ptr == MAP_FAILED ) {
		perrorm("map_zero: mmap");
		ptr = NULL;
	}
#ifdef __darwin__
	printm("Locking RAM\n");
	if( mlock(ptr, size) )
		fatal("mlock");
#endif
	return ptr;
}

static void 
map_ram( void ) 
{
	int rsize = get_numeric_res("ram_size");

	if( rsize == -1 )
		rsize = DEFAULT_RAM_SIZE;
	if( rsize >= 0x1000 )
		fatal("The RAM size should be given in MB!\n");

	ram.flags = 0;
	ram.size = rsize * 1024 * 1024;
	ram.mbase = 0;

	if( !(ram.lvbase=map_zero(WANTED_RAM_BASE, ram.size)) )
		fatal("failed to map RAM (out of memory?)");

#ifdef DEBUG_LOCK_MEM
	printm("DEBUG: Locking memory...\n");
	if( mlock(ram.lvbase, ram.size) )
		perrorm("mlock");
#endif
}

/* This function is used to map a physical oldworld ROM and the console 
 * framebuffer. The phys_ptr argument doesn't need to be page aligned.
 */
char *
map_phys_mem( char *wanted_ptr, ulong phys_ptr, size_t size, int prot  ) 
{
	ulong page_offs = (phys_ptr & 0xfff);
	ulong page_start = phys_ptr - page_offs;
	char *ret;
	int fd;
	
	if( (fd=open("/dev/mem", O_RDWR)) < 0 ) {
		perrorm("/dev/mem, open:");
		return 0;
	}

	size = size + page_offs;
	if( size & 0xfff )
		size = (size & 0xfffff000) + 0x1000;

	ret = mmap( wanted_ptr, size, prot, MAP_SHARED, fd, (off_t)page_start );

	if( ret == MAP_FAILED ) {
		perrorm("map_phys_mem: mmap");
		close( fd );
		return NULL;
	}
	close( fd );

	ret += page_offs;
	return ret;
}

/* unmap_mem is a wrapper of munmap. It does the same page 
 * alignment as map_phys_mem above.
 */
int
unmap_mem( char *start, size_t length )
{
	ulong page_offs, page_start;

	/* mmap wants page-aligned parameters */
	page_offs = (ulong)start & 0xfff;
	page_start = (ulong)start - page_offs;
	length += page_offs;

	if( length & 0xfff )
		length = (length & 0xfffff000UL) + 0x1000;
	return munmap( start, length );
}

/************************************************************************/
/*	mphys <-> lvptr translation					*/
/************************************************************************/

/* 0=ram, 1=rom, <0 error */
int 
mphys_to_lvptr( ulong mptr, char **ret )
{
	if( (ulong)(mptr - ram.mbase) < (ulong)ram.size ) {
		*ret = ram.lvbase + mptr - ram.mbase;
		return 0;
	}
	if( (ulong)(mptr - rom.mbase) < (ulong)rom.size ) {
		*ret = rom.lvbase + mptr - rom.mbase;
		return 1;
	}
	return -1;
}

void *
transl_mphys( ulong mphys )
{
	return transl_mphys_( mphys );
}

/* translate rom or ram */
char *
transl_ro_mphys( ulong mphys )
{
	if( (ulong)(mphys - ram.mbase) < (ulong)ram.size )
		return ram.lvbase + (mphys - ram.mbase);

	if( rom.size && (ulong)(mphys - rom.mbase) < (ulong)rom.size )
		return rom.lvbase + (mphys - rom.mbase);

	return NULL;
}

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

void
mem_init( void ) 
{
	mol_device_node_t *dn;

	/* platforms might handle RAM */
	if( !ram.lvbase ) {
		if( loading_session() )
			load_ram();
		else
			map_ram();
		_set_ram( (ulong)ram.lvbase, (ulong)ram.size );
	}
	_add_mmu_mapping( &ram );	

	/* Fix the /memory/reg property (if present) */
	if( (dn=prom_find_dev_by_path("/memory")) ) {
		ulong tab[8];
		memset( tab, 0, sizeof(tab) );
		tab[0] = ram.mbase;
		tab[1] = ram.size;
		prom_add_property( dn, "reg", (char*)tab, sizeof(tab) );
	}

#ifdef SESSION_SAVE_SUPPORT
	session_save_proc( save_ram, NULL, kStaticChunk );
	_track_dirty_RAM();
#endif
}

void 
mem_cleanup( void ) 
{
	if( ram.lvbase ) {
		_remove_mmu_mapping( &ram );
		munmap( ram.lvbase, ram.size );
		ram.lvbase = 0;
	}
}
