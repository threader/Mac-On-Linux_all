/* 
 *   Creation Date: <2002/01/13 17:42:35 samuel>
 *   Time-stamp: <2004/02/08 00:32:48 samuel>
 *   
 *	<alloc.c>
 *	
 *	Memory allocation
 *   
 *   Copyright (C) 2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "archinclude.h"
#include "alloc.h"


/************************************************************************/
/*	Kernel memory allocation					*/
/************************************************************************/

#define NUM_PAIRS	61

/* IMPORTANT: These memory allocations are not reentrant */

typedef struct addr_chunk {
	struct addr_chunk *next;
	int		filler;

	void		*addr[ NUM_PAIRS ];
	int		size[ NUM_PAIRS ];
} addr_chunk_t;

static addr_chunk_t 	*table_root;
static int 		free_slots;


void
memory_allocator_cleanup( void )
{
	addr_chunk_t *p, *pnext;
	
	for( p=table_root; p; p=pnext ) {
		pnext = p->next;
		IOFree( p, sizeof(addr_chunk_t) );
		free_slots -= NUM_PAIRS;
	}
	if( free_slots ) {
		free_slots = 0;
		printk("MOL: Kernel memory not released properly!\n");
	}
	table_root = NULL;
}


static int
store_size( void *ptr, int size )
{
	addr_chunk_t *p;
	int i;

	if( !ptr )
		return 0;
	if( !free_slots ) {
		if( !(p=IOMalloc(sizeof(addr_chunk_t))) )
			return 1;
		memset( p, 0, sizeof(addr_chunk_t) );
		p->next = table_root;
		table_root = p;
		free_slots += NUM_PAIRS;
	}
	for( p=table_root; p; p=p->next ) {
		for( i=0; i<NUM_PAIRS; i++ ){
			if( !p->addr[i] ) {
				p->addr[i] = ptr;
				p->size[i] = size;
				free_slots--;
				return 0;
			}
		}
	}
	panic("MOL,store_size: No free slots!\n");
	return 1;
}

static int 
get_size( void *addr )
{
	addr_chunk_t *p;
	int i;
	
	if( !addr )
		panic("get_size: addr == NULL\n");
	for( p=table_root; p; p=p->next )
		for( i=0; i<NUM_PAIRS; i++ )
			if( p->addr[i] == addr ) {
				int size = p->size[i];
				p->size[i]=0;
				p->addr[i]=NULL;
				free_slots++;
				return size;
			}
	panic("MOL,get_size: No such address\n");
	return 0;
}


void *
kmalloc_mol( int size )
{
	void *addr = IOMalloc(size);

	if( store_size(addr, size) ) {
		IOFree(addr, size);
		addr = NULL;
	}
	return addr;
}

void
kfree_mol( void *p )
{
	IOFree( p, get_size(p) );
}

void *
vmalloc_mol( int size )
{
	void *addr = IOMallocAligned( size, 0x1000 );
	if( store_size(addr, size) ) {
		IOFreeAligned( addr, size );
		addr = NULL;
	}
	return addr;
}

void
vfree_mol( void *p )
{
	IOFreeAligned( p, get_size(p) );
}

void *
kmalloc_cont_mol( int size )
{
	IOPhysicalAddress dummy;
	void *addr = IOMallocContiguous( size, 0x1000 /*align*/, &dummy);

	if( store_size(addr, size) ) {
		IOFreeContiguous( addr, size );
		addr = NULL;
	}
	return addr;
}

void
kfree_cont_mol( void *p )
{
	IOFreeContiguous( p, get_size(p) );
}


/************************************************************************/
/*	Misc memory related functions					*/
/************************************************************************/

/* Not defined in any header... */
extern vm_offset_t vm_map_get_phys_page( vm_map_t map, vm_offset_t offset);

ulong
tophys_mol( void *p )
{
	ulong phys = vm_map_get_phys_page( get_task_map(kernel_task), trunc_page(p) );

	if( !phys ) {
		printk("tophys_mol: VA %08lX translates to 0!\n", (ulong)p );
	}
	phys = phys << 12; /* A hack, but I've not figured out why yet */
	phys += phys ? (ulong)(p-trunc_page(p)) : 0;
	return phys;
}
