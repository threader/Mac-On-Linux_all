/* 
 *   Creation Date: <2004/02/03 21:30:11 samuel>
 *   Time-stamp: <2004/03/13 16:04:39 samuel>
 *   
 *	<support.cpp>
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

#include "archinclude.h"
#include <IOKit/IOService.h>
extern "C" {
#include "kernel_vars.h"
#include "misc.h"
#include "osx.h"
#include "map.h"
}

static IOMemoryMap *mregs_map[MAX_NUM_SESSIONS];


/************************************************************************/
/*	functions to map things into the kernel address space		*/
/************************************************************************/

char *
map_mregs( kernel_vars_t *kv ) 
{
	const int size = NUM_MREGS_PAGES * 0x1000;
	const int id = kv->session_index;
	IOMemoryDescriptor *desc;
	task_t moltask = current_task();
	
	if( !(desc=IOMemoryDescriptor::withAddress(&kv->mregs, size, kIODirectionOutIn)) )
		return NULL;
	mregs_map[id] = desc->map( moltask, NULL, kIOMapAnywhere | kIOMapCopybackCache );
	desc->release();

	if( !mregs_map[id] )
		return NULL;

	return (char*)mregs_map[id]->getVirtualAddress();
}

void
unmap_mregs( kernel_vars_t *kv )
{
	int id = kv->session_index;
	if( mregs_map[id] )
		mregs_map[id]->release();
	mregs_map[id] = NULL;
}


/************************************************************************/
/*	functions to map things into the kernel address space		*/
/************************************************************************/

void *
map_phys_range( ulong paddr, ulong size, char **ret_addr )
{
	IOMemoryDescriptor *desc;
	IOMemoryMap *m;
	
	if( !(desc=IOMemoryDescriptor::withPhysicalAddress(paddr, size, kIODirectionOutIn)) )
		return NULL;

	if( !(m=desc->map(kIOMapAnywhere | kIOMapCopybackCache)) ) {
		desc->release();
		return NULL;
	}
	*ret_addr = (char*)m->getVirtualAddress();

	//IOLog("map_phys_range called\n");
	return (void*)desc;
}

void
unmap_phys_range( void *handle )
{
	IOMemoryDescriptor *desc = (IOMemoryDescriptor*)handle;
	desc->release();
}


void *
map_virt_range( ulong va, ulong size, char **ret_addr )
{
	IOMemoryDescriptor *desc;
	IOMemoryMap *m;

	if( !(desc=IOMemoryDescriptor::withAddress(va, size, kIODirectionOutIn, current_task())) )
		return NULL;
	if( !(m=desc->map(kIOMapAnywhere | kIOMapCopybackCache)) ) {
		desc->release();
		return NULL;
	}
	*ret_addr = (char*)m->getVirtualAddress();

	return (void*)desc;
}

void
unmap_virt_range( void *handle )
{
	IOMemoryDescriptor *desc = (IOMemoryDescriptor*)handle;
	desc->release();
}


/************************************************************************/
/*	do_get_phys_page						*/
/************************************************************************/

ulong
do_get_phys_page( kernel_vars_t *kv, ulong lvptr )
{
	IOMemoryDescriptor *md;
	IOPhysicalAddress phys;
	
	md = IOMemoryDescriptor::withAddress( (vm_address_t)lvptr, 0x1000,
					      kIODirectionIn, current_task() );
	md->prepare( kIODirectionIn );
	phys = md->getPhysicalSegment( 0, NULL );
	md->complete( kIODirectionIn );
	md->release();

	return phys;
}
