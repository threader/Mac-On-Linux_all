/* 
 *   Copyright (C) 1997-2004 Samuel Rydh (samuel@ibrium.se)
 *   Kernel interface
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "kernel_vars.h"
#include "performance.h"
#include "ioctl.h"
#include "mtable.h"
#include "mmu.h"
#include "constants.h"
#include "asmfuncs.h"
#include "misc.h"
#include "emu.h"

kernel_vars_t *alloc_kvar_pages(void)
{
	kernel_vars_t *kv;
	int i, order;
	char *ptr;

	for (i = 1, order = 0; i < NUM_KVARS_PAGES; i = i << 1, order++) ;
	if (!(kv = (kernel_vars_t *) __get_free_pages(GFP_KERNEL, order)))
		return NULL;

	/* To be able to export the kernel variables to user space, we
	 * must set the PG_reserved bit. This is due to a check
	 * in remap_pte_range() in kernel/memory.c (is this bug or a feature?).
	 */
	for (ptr = (char *)kv, i = 0; i < NUM_KVARS_PAGES; i++, ptr += 0x1000)
		SetPageReserved(virt_to_page(ptr));

	return kv;
}

void free_kvar_pages(kernel_vars_t * kv)
{
	char *ptr = (char *)kv;
	int i, order;

	for (i = 0; i < NUM_KVARS_PAGES; i++, ptr += 0x1000)
		ClearPageReserved(virt_to_page(ptr));

	for (i = 1, order = 0; i < NUM_KVARS_PAGES; i = i << 1, order++) ;
	free_pages((unsigned long) kv, order);
}

/************************************************************************/
/*	hash access							*/
/************************************************************************/

unsigned long *map_emulated_hash(kernel_vars_t * kv, unsigned long mbase, unsigned long size)
{
	return (unsigned long *) (kv->mmu.userspace_ram_base + mbase);
}

void unmap_emulated_hash(kernel_vars_t * kv)
{
	/* nothing */
}

/************************************************************************/
/*	kernel lowmem asm <-> kernel C-code switching			*/
/************************************************************************/

typedef int (*kernelfunc_t) (kernel_vars_t *, unsigned long, unsigned long, unsigned long);
typedef void (*trampoline_t) (struct pt_regs * regs);
static trampoline_t old_trampoline;

static void mol_trampoline_vector(struct pt_regs *r)
{
	kernel_vars_t *kv = (kernel_vars_t *) r->gpr[8];

	TICK_CNTR_PUSH(kv);
	r->gpr[3] =
	    (*(kernelfunc_t) r->gpr[3]) (kv, r->gpr[4], r->gpr[5], r->gpr[6]);
	TICK_CNTR_POP(kv, in_kernel);
}

static trampoline_t set_trampoline(trampoline_t tramp)
{
	trampoline_t old;
	extern trampoline_t mol_trampoline;
	old = mol_trampoline;
	mol_trampoline = tramp;
	return old;
}

int arch_common_init(void)
{
	old_trampoline = set_trampoline(mol_trampoline_vector);
	return !old_trampoline;
}

void arch_common_cleanup(void)
{
	set_trampoline(old_trampoline);
}

/************************************************************************
 * Common misc functions
 ************************************************************************/

/************************************************************************/
/*	Performance Info						*/
/************************************************************************/

#ifdef PERFORMANCE_INFO

static void
clear_performance_info( kernel_vars_t *kv )
{
	perf_info_t *p = g_perf_info_table;
	int i;

	for( ; p->name ; p++ )
		*p->ctrptr = 0;
	for( i=0; i<NUM_ASM_BUMP_CNTRS; i++ )
		kv->asm_bump_cntr[i] = 0;
	kv->num_acntrs = 0;
}

static int
get_performance_info( kernel_vars_t *kv, uint ind, perf_ctr_t *r )
{
	perf_info_t *p;
	int len;
	char *name;

	for( p=g_perf_info_table; p->name && ind; p++, ind-- )
		;
	if( !p->name ) {
		extern int __start_bumptable[], __end_bumptable[];
		if( ind >= __end_bumptable - __start_bumptable )
			return 1;
		name = (char*)__start_bumptable + __start_bumptable[ind];
		r->ctr = kv->asm_bump_cntr[ind];
	} else {
		name = p->name;
		r->ctr = *p->ctrptr;
	}
	
	if( (len=strlen(name)+1) > sizeof(r->name) )
		len = sizeof(r->name);
	memcpy( r->name, name, len );
	return 0;
}

#else /* PERFORMANCE_INFO */

static void
clear_performance_info( kernel_vars_t *kv ) 
{
}

static int
get_performance_info( kernel_vars_t *kv, uint ind, perf_ctr_t *r )
{ 
	return 1;
}

#endif /* PERFORMANCE_INFO */

/************************************************************************/
/*	misc								*/
/************************************************************************/

int
do_debugger_op( kernel_vars_t *kv, dbg_op_params_t *pb ) 
{
	int ret = 0;
	
	switch( pb->operation ) {
	case DBG_OP_EMULATE_TLBIE:
		flush_ea_range( kv, (pb->ea & ~0xf0000000), 0x1000 );
		break;

	case DBG_OP_EMULATE_TLBIA:
		clear_all_vsids( kv );
		break;

	case DBG_OP_GET_PTE:
		ret = dbg_get_PTE( kv, pb->context, pb->ea, &pb->ret.pte );
		break;

	case DBG_OP_BREAKPOINT_FLAGS:
		kv->break_flags = pb->param;
		kv->mregs.flag_bits &= ~fb_DbgTrace;
		kv->mregs.flag_bits |= (pb->param & BREAK_SINGLE_STEP)? fb_DbgTrace : 0;
		msr_altered( kv );
		break;
		
	case DBG_OP_TRANSLATE_EA:
		/* param == is_data_access */
		ret = dbg_translate_ea( kv, pb->context, pb->ea, &pb->ret.phys, pb->param );
		break;

	default:
		printk("Unimplemended debugger operation %d\n", pb->operation );
		ret = -ENOSYS;
		break;
	}
	return ret;
}

static void
tune_spr( kernel_vars_t *kv, uint spr, int action )
{
	extern int r__spr_illegal[], r__spr_read_only[], r__spr_read_write[];
	int hook, newhook=0;

	if( spr >= 1024 )
		return;

	hook = kv->_bp.spr_hooks[spr];

	/* LSB of hook specifies whether the SPR is privileged */
	switch( action ) {
	case kTuneSPR_Illegal:
		newhook = (int)r__spr_illegal;
		hook &= ~1;
		break;

	case kTuneSPR_Privileged:
		hook |= 1;
		break;

	case kTuneSPR_Unprivileged:
		hook &= ~1;
		break;

	case kTuneSPR_ReadWrite:
		newhook = (int)r__spr_read_write;
		break;

	case kTuneSPR_ReadOnly:
		newhook = (int)r__spr_read_only;
		break;
	}
	if( newhook )
		hook = (hook & 1) | virt_to_phys( (char*)reloc_ptr(newhook) ); 
	kv->_bp.spr_hooks[spr] = hook;
}

/* return value: <0: system error, >=0: ret value */
int
handle_ioctl( kernel_vars_t *kv, int cmd, int arg1, int arg2, int arg3 ) 
{
	struct mmu_mapping map;
	perf_ctr_t pctr;
	int ret = 0;
	
	switch( cmd ) {
	case MOL_IOCTL_GET_SESSION_MAGIC:
		ret = get_session_magic( arg1 );
		break;

	case MOL_IOCTL_IDLE_RECLAIM_MEMORY:
		mtable_reclaim( kv );
		break;

	case MOL_IOCTL_SPR_CHANGED:
		msr_altered(kv);
		mmu_altered(kv);
		break;

	case MOL_IOCTL_ADD_IORANGE: /* void ( ulong mbase, int size, void *usr_data )*/
		add_io_trans( kv, arg1, arg2, (void*)arg3 );
		break;
	case MOL_IOCTL_REMOVE_IORANGE:	/* void ( ulong mbase, int size ) */
		remove_io_trans( kv, arg1, arg2 );
		break;

	case MOL_IOCTL_ALLOC_EMUACCEL_SLOT: /* EMULATE_xxx, param, ret_addr -- mphys */
		ret = alloc_emuaccel_slot( kv, arg1, arg2, arg3 );
		break;
	case MOL_IOCTL_MAPIN_EMUACCEL_PAGE: /* arg1 = mphys */
		ret = mapin_emuaccel_page( kv, arg1 );
		break;

	case MOL_IOCTL_SETUP_FBACCEL: /* lvbase, bytes_per_row, height */
		setup_fb_acceleration( kv, (char*)arg1, arg2, arg3 );
		break;
	case MOL_IOCTL_TUNE_SPR: /* spr#, action */
		tune_spr( kv, arg1, arg2 );
		break;

	case MOL_IOCTL_MMU_MAP: /* arg1=struct mmu_mapping *m, arg2=map/unmap */
		if( copy_from_user(&map, (struct mmu_mapping*)arg1, sizeof(map)) )
			break;
		if( arg2 )
			mmu_add_map( kv, &map );
		else 
			mmu_remove_map( kv, &map );
		if( copy_to_user((struct mmu_mapping*)arg1, &map, sizeof(map)) )
			ret = -EFAULT;
		break;

	case MOL_IOCTL_GET_PERF_INFO:
		ret = get_performance_info( kv, arg1, &pctr );
		if( copy_to_user((perf_ctr_t*)arg2, &pctr, sizeof(pctr)) )
			ret = -EFAULT;
		break;

#if 0
	case MOL_IOCTL_TRACK_DIRTY_RAM:
		ret = track_lvrange( kv );
		break;
	case MOL_IOCTL_GET_DIRTY_RAM:
		ret = get_track_buffer( kv, (char*)arg1 );
		break;
	case MOL_IOCTL_SET_DIRTY_RAM:
		set_track_buffer( kv, (char*)arg1 );
		break;
#endif
	/* ---------------- performance statistics ------------------ */

	case MOL_IOCTL_CLEAR_PERF_INFO:
		clear_performance_info( kv );
		break;

	default:
		printk("unsupported MOL ioctl %d\n", cmd );
		ret = -ENOSYS;
	}
	return ret;
}
