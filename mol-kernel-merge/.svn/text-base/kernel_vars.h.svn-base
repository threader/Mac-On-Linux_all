/* 
 *   Creation Date: <97/07/14 15:53:06 samuel>
 *   Time-stamp: <2004/02/21 21:37:37 samuel>
 *   
 *	<kernel_vars.h>
 *	
 *	Variables used by the kernel
 *   
 *   Copyright (C) 1997-2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef H_KERNEL_VARS
#define H_KERNEL_VARS

#define MAX_NUM_SESSIONS	8

#include "mac_registers.h"

#ifdef PERFORMANCE_INFO
#define NUM_ASM_BUMP_CNTRS	64
#define NUM_ASM_TICK_CNTRS	6
#endif

#ifndef __ASSEMBLY__
#include "mmu_mappings.h"
#include "skiplist.h"

#ifndef DUMPVARS
#include "alloc.h"
#include "locks.h"
#else
typedef int		mol_mutex_t;
typedef int		atomic_t;
#endif

typedef struct {
	unsigned long		word[2];		/* upper, lower */
} ppc_bat_t;

typedef struct mac_bat {
	int		valid;			/* record in use */
	unsigned long		base;
	unsigned long		mbase;
	unsigned long		size;

	unsigned long		wimg:4;			/* WIMG-bits */
	unsigned long		vs:1;			/* valid supervisor mode */
	unsigned long		vp:1;			/* valid user mode */
	unsigned long		ks:1;			/* key superuser */
	unsigned long		ku:1;			/* key user */
	unsigned long		pp:2;			/* page protection */

	/* possibly we should track inserted PTEs here... */
} mac_bat_t;

typedef struct {
	struct vsid_ent	*vsid[16];		/* entries might be NULL */
	struct vsid_ent *unmapped_vsid[16];	/* entries might be NULL, linux_vsid_sv used */

	unsigned long		emulator_sr[16];	/* segment registers used by the userspace process */
	
	unsigned long		user_sr[16];		/* segment registers for MSR=user */
	unsigned long		sv_sr[16];		/* segment registers for MSR=sv */
	unsigned long		unmapped_sr[16];	/* segment registers for unmapped mode */
	unsigned long		split_sr[16];		/* segment registers used in split mode */

	unsigned long		cur_sr_base;		/* (physical) pointer to user_sr or sv_sr */
	unsigned long		sr_inst;		/* (physical) pointer to us user_sr or sv_sr */
	unsigned long		sr_data;		/* (physical) pointer to us user_sr or sv_sr */

	unsigned long		illegal_sr;		/* used for the lazy segment register impl. */ 

	ppc_bat_t	split_dbat0;		/* loaded to DBAT0 (used in splitmode) */
	ppc_bat_t	transl_dbat0;		/* DBAT0 mapping the framebuffer */

	unsigned long		emulator_context;	/* context of emulator (equals VSID >> 4) in Linux */

	unsigned long		userspace_ram_base;	/* userspace RAM base */
	size_t		ram_size;

	unsigned long		bat_hack_count;		/* HACK to speed up MacOS 9.1 */
	mac_bat_t	bats[8];		/* 4 IBAT + 4 DBAT */

#ifdef EMULATE_603
	unsigned long		ptes_d_ea_603[64];	/* EA4-EA19 of dPTE */
	mPTE_t		ptes_d_603[64];		/* Data on-chip PTEs (603-emulation) */
	unsigned long		ptes_i_ea_603[64];	/* EA4-EA19 of iPTE */
	mPTE_t		ptes_i_603[64];		/* Instruction on-chip PTEs (603-emulation) */
#endif
	/* emulated PTE hash */
	unsigned long		hash_mbase;		/* mac physical hash base */
	unsigned long		*hash_base;		/* kernel pointer to mac hash */
	unsigned long		hash_mask;		/* hash mask (0x000fffff etc) */

	/* context number allocation */
	int		next_mol_context;	/* in the range FIRST .. LAST_MOL_CONTEXT(n) */
	int		first_mol_context;	/* first context number this session may use */
	int		last_mol_context;	/* last context number this session may use */

	unsigned long		pthash_sr;		/* segment register corresponding to */ 
	unsigned long		pthash_ea_base;		/* pthash_ea_base */
	void		*pthash_inuse_bits;	/* bitvector (one bit per PTE) */
	unsigned long		pthash_inuse_bits_ph;	/* physical base address */

	/* various tables */
	struct io_data 		*io_data;	/* translation info */
	struct fb_data		*fb_data;	/* ea -> PTE table */
	struct tracker_data	*tracker_data;	/* Keeps track of modified pages */

	/* mtable stuff */
	skiplist_t		vsid_sl;	/* skiplist (with vsid_ent_t objects) */
	struct vsid_info	*vsid_info;	/* mtable data */

	char   		*lvptr_reservation;	/* lvptr associated with PTE to be inserted */
	int		lvptr_reservation_lost;	/* set if reservation is lost (page out) */
} mmu_vars_t;


/* variables which are private to the low level assembly code */
typedef struct {
	unsigned long		spr_hooks[NUM_SPRS];	/* hooks */

	ppc_bat_t 	ibat_save[4];		/* kernel values of the BAT-registers */
	ppc_bat_t 	dbat_save[4];

	unsigned long		_msr;			/* MSR used in mac-mode (_not_ the emulated msr) */

	/* saved kernel/emulator registers */
	unsigned long		emulator_nip;
	unsigned long		emulator_msr;
	unsigned long		emulator_sprg2;
	unsigned long		emulator_sprg3;
	unsigned long		emulator_kcall_nip;
	unsigned long 		emulator_stack;
	unsigned long 		emulator_toc;		/* == r2 on certain systems */

	/* DEC and timebase */
	unsigned long		dec_stamp;		/* linux DEC = dec_stamp - tbl */
	unsigned long		int_stamp;		/* next DEC event = int_stamp - tbl */

	/* splitmode */
	int		split_nip_segment;	/* segment (top 4) used for inst. fetches */

	/* segment register offset table */
	unsigned long		msr_sr_table[ 4*8 ];	/* see emulation.S */

	unsigned long		tmp_scratch[4];		/* temporary storage */
} base_private_t;


#ifdef PERFORMANCE_INFO
#define MAX_ACC_CNTR_DEPTH	8
typedef struct acc_counter {
	unsigned long 		stamp;
	unsigned long		subticks;
} acc_counter_t;
#endif

typedef struct kernel_vars {
	struct mac_regs		mregs;			/* must go first */
	char 			page_filler[0x1000 - (sizeof(mac_regs_t)&0xfff) ];

	base_private_t	 	_bp;
	char 			aligner[32 - (sizeof(base_private_t)&0x1f) ];
	mmu_vars_t		mmu;
	char 			aligner2[16 - (sizeof(mmu_vars_t)&0xf) ];

	unsigned long		emuaccel_mphys;		/* mphys address of emuaccel_page */
	int			emuaccel_size;		/* size used */
	unsigned long		emuaccel_page_phys;	/* phys address of page */
	unsigned long		emuaccel_page;		/* page used for instruction acceleration */

	int			break_flags;
	unsigned long		kvars_tophys_offs;	/* physical - virtual address of kvars */
	struct kernel_vars	*kvars_virt;		/* me */
	int			session_index;

	mol_mutex_t		ioctl_sem;		/* ioctl lock */

#ifdef PERFORMANCE_INFO
	unsigned long		asm_bump_cntr[NUM_ASM_BUMP_CNTRS];
	unsigned long		asm_tick_stamp[NUM_ASM_TICK_CNTRS];
	int			num_acntrs;
	acc_counter_t		acntrs[MAX_ACC_CNTR_DEPTH];
#endif

	void			*main_thread;	/* pointer to the main thread task_struct */

} kernel_vars_t;

#define NUM_KVARS_PAGES		((sizeof(kernel_vars_t)+0xfff)/0x1000)

typedef struct {
	kernel_vars_t		*kvars[MAX_NUM_SESSIONS];
	int			magic;
	unsigned long 		kvars_ph[MAX_NUM_SESSIONS];
	mol_mutex_t		lock;
	atomic_t		external_thread_cnt;
} session_table_t;

#define SESSION_LOCK		down_mol( &g_sesstab->lock )
#define SESSION_UNLOCK		up_mol( &g_sesstab->lock )

extern session_table_t		*g_sesstab;


#endif /* __ASSEMBLY__ */
#endif
