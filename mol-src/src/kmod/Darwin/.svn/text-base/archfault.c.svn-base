/* 
 *   Creation Date: <2004/02/01 18:24:32 samuel>
 *   Time-stamp: <2004/03/13 14:56:16 samuel>
 *   
 *	<archfault.c>
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
#include "kernel_vars.h"
#include "misc.h"
#include "mmu.h"
#include "mtable.h"
#include "osx.h"

ulong 
get_phys_page( kernel_vars_t *kv, ulong lvptr, int request_rw )
{
	ulong phys;
	
	make_lvptr_reservation( kv, (char*)lvptr );
	phys = do_get_phys_page( kv, lvptr );

	return phys;
}



/************************************************************************/
/*	TESTING								*/
/************************************************************************/

#if 0
#include <kern/thread.h>
#include <mach/thread_act.h>

/* not defined in any header... */
extern kern_return_t vm_fault( vm_map_t map, vm_offset_t vaddr, vm_prot_t fault_type,
			       boolean_t change_wiring, int interruptible, pmap_t pmap,
			       vm_offset_t pmap_addr );
extern vm_offset_t vm_map_get_phys_page( vm_map_t map, vm_offset_t offset);

ulong 
get_phys_page( kernel_vars_t *kv, ulong lvptr, int request_rw )
{
	task_t task = current_task();
	vm_map_t map = get_task_map( task );
	ulong phys;

	//printk("get_phys_page is unimplemented\n");

	phys = vm_map_get_phys_page( map, trunc_page(lvptr) );
	if( !phys ) {
		vm_fault( map, trunc_page(lvptr),
			  VM_PROT_READ | (request_rw? VM_PROT_WRITE : 0),
			  FALSE, THREAD_ABORTSAFE, NULL, 0 );
		phys = vm_map_get_phys_page( map, trunc_page(lvptr) );
		printk("secondary faulting: %lx -> %lx\n", lvptr, phys );
	}
	if( !phys ) {
		static char *scratch = NULL;
		if( !scratch )
			scratch = (char*)alloc_page_mol();
		printk("NULL page %08lx(!)\n", lvptr);
		phys = tophys_mol( scratch );
	}
	//printk("lvptr %08lx -> PHYS %08lx\n", lvptr, phys );
	return phys;
}

#endif
