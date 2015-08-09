/* 
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "kernel_vars.h"
#include "mmu.h"
#include "mtable.h"
#include "map.h"
#include "asmfuncs.h"
#include "emu.h"
#include "performance.h"
#include "mmu_contexts.h"
#include "context.h"
#include "hash.h"

#ifdef CONFIG_SMP
void (*xx_tlbie_lowmem) (void);
void (*xx_store_pte_lowmem) (void);
#else
void (*xx_store_pte_lowmem) (unsigned long * slot, int pte0, int pte1);
#endif

int arch_mmu_init(kernel_vars_t * kv)
{
	kv->mmu.emulator_context = current->mm->context.id;
	return 0;
}

/************************************************************************
 * Common mmu functions 
 ************************************************************************/

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

int 
init_mmu( kernel_vars_t *kv )
{
	int success;

	success =
		!arch_mmu_init( kv ) &&
		!init_contexts( kv ) &&
		!init_mtable( kv ) &&
		!init_mmu_io( kv ) &&
		!init_mmu_fb( kv ) &&
		!init_mmu_tracker( kv );

	if( !success ) {
		cleanup_mmu( kv );
		return 1;
	}

	clear_vsid_refs( kv );

	/* SDR1 is set from fStartEmulation */
	return 0;
}

void 
cleanup_mmu( kernel_vars_t *kv )
{
	/* We have to make sure the flush thread are not using the mtable
	 * facilities. The kvars entry has been clear so we just have
	 * to wait around until no threads are using it.
	 */
	while( atomic_read(&g_sesstab->external_thread_cnt) )
		;

	cleanup_mmu_tracker( kv );
	cleanup_mmu_fb( kv );
	cleanup_mmu_io( kv );
	cleanup_mtable( kv );
	cleanup_contexts( kv );

	if( kv->mmu.pthash_inuse_bits )
		kfree( kv->mmu.pthash_inuse_bits );
	if( kv->mmu.hash_base )
		unmap_emulated_hash( kv );

	memset( &kv->mmu, 0, sizeof(mmu_vars_t) );
}


/************************************************************************/
/*	misc								*/
/************************************************************************/

/* All vsid entries have been flushed; clear dangling pointers */
void
clear_vsid_refs( kernel_vars_t *kv )
{
	int i;
	for( i=0; i<16; i++ ) {
		kv->mmu.vsid[i] = NULL;
		kv->mmu.unmapped_vsid[i] = NULL;

		kv->mmu.user_sr[i] = kv->mmu.illegal_sr;
		kv->mmu.sv_sr[i] = kv->mmu.illegal_sr;
		kv->mmu.unmapped_sr[i] = kv->mmu.illegal_sr;
		kv->mmu.split_sr[i] = kv->mmu.illegal_sr;
	}
	invalidate_splitmode_sr( kv );
}

/*
 * This function is called whenever the mac kv->mmu-registers have 
 * been manipulated externally.
 */
void
mmu_altered( kernel_vars_t *kv )
{
	int i;

	for( i=0; i<16; i++ ) {
		kv->mmu.vsid[i] = NULL;
		kv->mmu.user_sr[i] = kv->mmu.illegal_sr;
		kv->mmu.sv_sr[i] = kv->mmu.illegal_sr;
	}
	invalidate_splitmode_sr( kv );

	do_mtsdr1( kv, kv->mregs.spr[S_SDR1] );

	for( i=0; i<16; i++ )
		do_mtbat( kv, S_IBAT0U+i, kv->mregs.spr[ S_IBAT0U+i ], 1 );
}

/*
 * A page we might be using is about to be destroyed (e.g. swapped out).
 * Any PTEs referencing this page must be flushed. The context parameter
 * is vsid >> 4.
 *
 * ENTRYPOINT!
 */
void 
do_flush( ulong context, ulong va, ulong *dummy, int n ) 
{
	int i;
	kernel_vars_t *kv;
	BUMP( do_flush );

	atomic_inc( &g_sesstab->external_thread_cnt );

	for( i=0; i<MAX_NUM_SESSIONS; i++ ) {
		if( !(kv=g_sesstab->kvars[i]) || context != kv->mmu.emulator_context )
			continue;

		BUMP_N( block_destroyed_ctr, n );
		for( ; n-- ; va += 0x1000 )
			flush_lvptr( kv, va );
		break;
	}

	atomic_dec( &g_sesstab->external_thread_cnt );
}


/************************************************************************/
/*	Debugger functions						*/
/************************************************************************/

int
dbg_get_PTE( kernel_vars_t *kv, int context, ulong va, mPTE_t *retptr ) 
{
	ulong base, mask;
	ulong vsid, ptmp, stmp, *pteg, *steg;
	ulong cmp;
        ulong *uret = (ulong*)retptr;
	int i, num_match=0;

	switch( context ) {
	case kContextUnmapped:
		vsid = kv->mmu.unmapped_sr[va>>28];
		break;
	case kContextMapped_S:
		vsid = kv->mmu.sv_sr[va>>28];
		break;
	case kContextMapped_U:
		vsid = kv->mmu.user_sr[va>>28];
		break;
	case kContextEmulator:
		vsid = (MOL_MUNGE_CONTEXT(kv->mmu.emulator_context) + ((va>>28) * MOL_MUNGE_ESID_ADD)) & 0xffffff;
		break;
        case kContextKernel:
                vsid = 0;
                break;
	default:
		printk("get_PTE: no such context: %d\n", context );
		return 0;
	}
	
	/* mask vsid and va */
	vsid &= 0xffffff;
	va &= 0x0ffff000;

	/* get hash base and hash mask */
	base = (ulong)ptehash.base;
	mask = ptehash.pteg_mask >> 6;

	/* hash function */
	ptmp = (vsid ^ (va>>12)) & mask;
	stmp = mask & ~ptmp;
	pteg = (ulong*)((ptmp << 6) + base);
	steg = (ulong*)((stmp << 6) + base);

	/* construct compare word */
	cmp = 0x80000000 | (vsid <<7) | (va>>22);

	/* look in primary PTEG */
	for( i=0; i<8; i++ ) {
		if( cmp == pteg[i*2] ) {
			if( !num_match++ && uret ) {
				uret[0] = pteg[i*2];
				uret[1] = pteg[i*2+1];
			}
			if( num_match == 2 ) {
				printk("Internal ERROR: duplicate PTEs!\n");
				printk("p-hash: low_pte: %08lX  high_pte: %08lX\n",
				      uret ? uret[0]:0, retptr? uret[1]:0 );
			}
			if( num_match >= 2 ) {
				printk("p-hash: low_pte: %08lX  high_pte: %08lX\n",
				       pteg[i*2], pteg[i*2+1] );
			}
		}
	}

	/* look in secondary PTEG */
	cmp |= 0x40;
	for( i=0; i<8; i++ ) {
		if( cmp == steg[i*2] ) {
			if( !num_match++ && uret ) {
				uret[0] = steg[i*2];
				uret[1] = steg[i*2+1];
			}
			if( num_match == 2 ) {
				printk("Internal ERROR: duplicate PTEs!\n");
				printk("?-hash: low_pte: %08lX  high_pte: %08lX\n",
				      uret? uret[0]:0, uret? uret[1]:0 );
			}
			if( num_match >= 2 ) {
				printk("s-hash: low_pte: %08lX  high_pte: %08lX\n",
				       steg[i*2], steg[i*2+1] );
			}
		}
	}
	return num_match;
}
