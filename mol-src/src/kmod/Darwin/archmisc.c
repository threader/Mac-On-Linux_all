/* 
 *   Creation Date: <2004/02/01 17:58:42 samuel>
 *   Time-stamp: <2004/03/13 19:13:13 samuel>
 *   
 *	<archmisc.c>
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
#include "context.h"
#include "asmfuncs.h"
#include "hash.h"
#include "mmu.h"
#include "map.h"
#include "osx.h"

#define MMU	(kv->mmu)

/* GLOBALS */
int		munge_inverse;		/* vsid_add * inverse = 1 mod 2^20 */

void		(*xx_tlbie_lowmem)( void );
void		(*xx_store_pte_lowmem)( void );
int		compat_hash_table_lock;

kernel_vars_t *
alloc_kvar_pages( void )
{
	IOPhysicalAddress phys;
	kernel_vars_t *kv = IOMallocContiguous( NUM_KVARS_PAGES * 0x1000, 0x1000, &phys );
	return kv;
}

void
free_kvar_pages( kernel_vars_t *kv )
{
	IOFreeContiguous( kv, NUM_KVARS_PAGES * 0x1000 );
}

int
arch_common_init( void )
{
	int i;

	/* calculate the multiplicative inverse of incrVSID */
	for( i=1 ; ((i * incrVSID) & 0xfffff) != 1 ; i+= 2 )
		;
	munge_inverse = i;

	/* printk("VSID add %d, inverse %d\n", MUNGE_ADD_NEXT, i ); */
	return 0;
}

void
arch_common_cleanup( void )
{
}


/************************************************************************/
/*	session specific initialization					*/
/************************************************************************/

int
arch_mmu_init( kernel_vars_t *kv )
{
	MMU.os_sdr1 = _get_sdr1();
	MMU.mol_sdr1 = ptehash.sdr1;
	return 0;
}


/************************************************************************/
/*	map emulated hash						*/
/************************************************************************/

static void 	*hash_handle[ MAX_NUM_SESSIONS ];


ulong *
map_emulated_hash( kernel_vars_t *kv, ulong mbase, ulong size )
{
	int id = kv->session_index;
	char *addr;
	
	hash_handle[id] = map_virt_range( MMU.userspace_ram_base + mbase, size, &addr );
	if( !hash_handle[id] )
		addr = NULL;

	if( !addr )
		printk("MOL: failed to map emulated hashtable\n");

	/* printk("map_emulated_hash - out (ea %08x)\n", (int)addr); */
	return (ulong*)addr;
}

void
unmap_emulated_hash( kernel_vars_t *kv )
{
	unmap_virt_range( hash_handle[kv->session_index] );
	/* printk("emulated hash unmapped\n"); */
}
