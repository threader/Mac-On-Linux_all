/* 
 *   Creation Date: <2001/01/31 21:05:23 samuel>
 *   Time-stamp: <2004/02/22 14:19:39 samuel>
 *
 *   
 *	<cpu.c>
 *	
 *	
 *   
 *   Copyright (C) 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "molcpu.h"
#include "mac_registers.h"
#include "wrapper.h"
#include "memory.h"
#include "session.h"
#include "timer.h"
#include "res_manager.h"
#include "rvec.h"
#include "os_interface.h"
#include "constants.h"
#include "drivers.h"
#include "promif.h"
#include "mol_assert.h"
#include "breakpoints.h"
#include "processor.h"
#include "byteorder.h"
#include "booter.h"
#include "hostirq.h"

/* exports to mainloop_asm.S */
extern int	_rvec_spr_read( int dummy_rvec, int sprnum, int gprnum );
extern int	_rvec_io_read( int dummy_rvec, ulong mphys_ioaddr, void *usr_data );
extern int	_rvec_io_write( int dummy_rvec, ulong mphys_ioaddr, void *usr_data );


typedef struct {
	int	first;
	int	last;
} spr_range_t;

static spr_range_t ill_603[] = {
	{ 0, 0 },	{ 2, 7 },	{ 10, 17 },	{ 20, 21 },	{ 23, 24 },
	{ 28, 267 },	{ 270, 271 }, 	{ 276, 281 },	{ 283, 283 },	{ 286, 286 },
	{ 288, 527 },	{ 544, 975 },	{ 983, 1009 },	{ 1011, 1023 },
	{ -1, -1 }
};

static spr_range_t ill_604[] = {
	{ 0, 0 },	{ 2, 7 },	{ 10, 17 },	{ 20, 21 },	{ 23, 24 },
	{ 28, 267 },	{ 270, 271 }, 	{ 276, 281 },	{ 283, 283 },	{ 286, 286 },
	{ 288, 527 },	{ 544, 951 },	{ 960, 1007 },	{ 1011, 1012 },
	{ 1014, 1022 }, 
	{ -1, -1 }
};

static spr_range_t ill_7455[] = {
	{ 0, 0 },	{ 2, 7 },	{ 10, 17 },	{ 20, 21 },	{ 23, 24 },
	{ 28, 255 },	{ 257, 267 },	{ 270, 271 }, 	{ 280, 281 },	{ 283, 283 },
	{ 286, 286 },	{ 288, 527 },	{ 544, 559 },	{ 576, 927 },	{ 931, 935 },
	{ 943, 943 },	{ 947, 950 },	{ 960, 979 },	{ 1004, 1007 }, 
	{ -1, -1 }
};

/* G3 */
static spr_range_t ill_750[] = {
	{ 0, 0 },	{ 2, 7 },	{ 10, 17 },	{ 20, 21 },	{ 23, 24 },
	{ 28, 267 },	{ 270, 271 }, 	{ 276, 281 },	{ 283, 283 },	{ 286, 286 },
	{ 288, 527 },	{ 544, 935 },	{ 944, 951 },	{ 960, 1007 },	{ 1011, 1012 },
	{ 1014, 1016 }, { 1018, 1018 }, { 1023, 1023 },
	{ -1, -1 }
};

/* G4 */
static spr_range_t ill_7400[] = {
	{ 0, 0 },	{ 2, 7 },	{ 10, 17 },	{ 20, 21 },	{ 23, 24 },	
	{ 28, 255 },	{ 257, 267 },	{ 270, 271 }, 	{ 276, 281 },	{ 283, 283 },
	{ 286, 286 },	{ 288, 527 },	{ 544, 927 },	{ 931, 934 },	{ 945, 950 },
	{ 960, 1007 },	{ 1011, 1012 },	{ 1016, 1016 }, { 1018, 1018 },
	{ -1, -1 }
};

/* G5 */
static spr_range_t ill_970[] = {
	{ -1, -1 }
};

/* Cell */
static spr_range_t ill_cell[] = {
	{ -1, -1 }
};

static int user_sprs[] = {
	/* performance monitor registers */
	S_UBAMR, 
	S_USIAR, S_USDAR,
	S_UMMCR0, S_UMMCR1, S_UMMCR2,
	S_UPMC1, S_UPMC2, S_UPMC3, S_UPMC4, S_UPMC5, S_UPMC6,
	S_TBRL, S_TBRU,

	-1 /* END */
};

static int rw_sprs[] = {
	/* common */
	S_SRR0, S_SRR1, S_SPRG0, S_SPRG1, S_SPRG2, S_SPRG3, S_DSISR, S_DAR, S_EAR,

	/* extended */
	S_SPRG4, S_SPRG5, S_SPRG6, S_SPRG7,

	/* memory/cache/thermal */
	/*S_MSSCR0,*/ S_MSSCR1,		/* Memory Subsystem Control Register */
	S_LDSTCR,			/* Load/Store Control Register */
	S_ICTC,				/* Instruction Cache Throttling Control Register */

	/* 603/74xx software hash walk (603 too) */
	S_PTEHI, S_PTELO,		/* PTE high/low */
	S_TLBMISS,			/* read-only on the 7455 */

	/* 603-only */
	S_DMISS, S_DCMP, S_HASH1, S_HASH2,

	/* performance monitor */
	S_BAMR,				/* Breakpoint Mask Address Register */
	S_SIAR,	S_SDAR,			/* Sampled Instruction/Data Address Register */
	S_MMCR0, S_MMCR1, S_MMCR2,	/* Monitor Mode Control Register 0 */
	S_PMC1,	S_PMC2, S_PMC3,
	S_PMC4, S_PMC5,	S_PMC6,

	/* debugger / breakpoints */
	S_IABR,	S_DABR,			/* Instruction/Data Addres Breakpoint Register */

	/* misc */
	S_PIR,

	/* partially undocumented SPRs (valid on the 7455) */
	984, 985, 986, 987, 988, 989, 990, 991, 992, 993, 994, 995, 996, 997,
	998, 999, 1000, 1001, 1002, 1003,

	/* XXX: these might need special handling eventually */
	S_L3PM,
	S_ICTRL,
	S_HID0, S_HID1,

	-1 /* END */
};

static int ro_sprs[] = {
	S_PVR,				/* PVR */
	
	/* user performance monitor registers */
	S_UBAMR, 
	S_USIAR, S_USDAR,
	S_UMMCR0, S_UMMCR1, S_UMMCR2,
	S_UPMC1, S_UPMC2, S_UPMC3, S_UPMC4, S_UPMC5, S_UPMC6,

	-1 /* END */
};

typedef struct {
	uint		pvr;
	uint		mask;
	uint		hid0;
	int		altivec;
	spr_range_t	*illegal_sprs;
	char		*alias;
	char		*name;
} cpu_entry_t;

static cpu_entry_t cpuinfo[] = {
	/* first entry is the default one */	
	{ 0x00080202,	0xffff0000,	0,		0, ill_750, 	"G3",	"750"	},
	{ 0x00010001,	0xffff0000,	0x80010080,	0, NULL,	NULL,	"601"	},
	{ 0x00030001,	0xffff0000,	0,		0, ill_603,	NULL,	"603"	},
	{ 0x00060101,	0xffff0000,	0, 		0, ill_603,	NULL,	"603e"	},
	{ 0x00070101,	0xffff0000,	0,		0, ill_603,	NULL,	"603ev"	},
	{ 0x00040103,	0xffff0000,	0,		0, ill_604,	NULL,	"604"	},
	{ 0x000c0209,	0xffff0000,	0,		1, ill_7400,	"G4",	"7400"	},
	{ 0x800c1101,	0xffff0000,	0,		1, NULL,	NULL,	"7410"	},
	{ 0x80000201,	0xffff0000,	0,		1, NULL,	NULL,	"7450"	},
	{ 0x80010302,	0xffff0000,	0x0410c1bc,	1, ill_7455,	NULL,	"7455"	},
	{ 0x80020101,	0xffff0000,	0,		1, NULL,	NULL,	"7447"	},
	{ 0x80030101,	0xffff0000,	0,		1, NULL,	NULL,	"7447A" },
	{ 0x80040202,	0xffff0000,	0,		1, NULL,	NULL,	"7448"	},
	{ 0x00390202,	0xffff0000, 	0,		1, ill_970,	NULL,	"970"	},
	{ 0x003C0300,	0xffff0000,	0,		1, ill_970,	NULL,	"970FX"	},
	{ 0x00700501,	0xffff0000,	0,		0, ill_cell,	NULL,	"Cell"	},
	{ 0,		0,		0,		0, NULL,	NULL, 	NULL	},
};

static struct {
	int		break_flags;
} molcpu;



/************************************************************************/
/*	Debugger Support						*/
/************************************************************************/

static int __dbg
rvec_break( int dummy_rvec, int break_flag )
{
	switch( break_flag ) {
	case BREAK_RFI:
		if( (molcpu.break_flags & BREAK_USER) ) {
			if( !(mregs->msr & MSR_PR) )
				return 0;
			clear_break_flag( BREAK_USER );
		}
		printm("BREAK-RFI (%08lX)\n", mregs->nip );
		break;
	case BREAK_EA_PAGE:
		/* breakpoint might not have been written */
		printm("ea-break at %08lX\n", mregs->nip );
		restore_breakpoints();
		setup_breakpoints();
		break;
	case BREAK_SINGLE_STEP:
		if( molcpu.break_flags & BREAK_SINGLE_STEP_CONT ) {
			if( !is_stop_breakpoint( mregs->nip ) )
				return 0;
			clear_break_flag( BREAK_SINGLE_STEP_CONT );
		}
		//setup_breakpoints();
		break;
	default:
		printm("rvec_break: Unknown break flag %d\n", break_flag );
		break;
	}
	stop_emulation();
	clear_break_flag( break_flag );
	return 0;
}

void 
set_break_flag( int flag )
{
	if( flag & BREAK_USER )
		flag |= BREAK_RFI;
	
	molcpu.break_flags |= flag;
	_breakpoint_flags( molcpu.break_flags );
}

void
clear_break_flag( int flag )
{
	molcpu.break_flags &= ~flag;
	_breakpoint_flags( molcpu.break_flags );
}


/************************************************************************/
/*	Exceptions							*/
/************************************************************************/

static inline void 
mac_exception( int vector, int sbits ) 
{
	int msr = mregs->msr;

	mregs->spr[S_SRR0] = mregs->nip;
	mregs->spr[S_SRR1] = (msr & (0xffff|MSR_VEC)) | (sbits & ~(0xffff|MSR_VEC));
	msr &= (MSR_ME | MSR_IP);
	mregs->nip = (msr & MSR_IP) ? (0xfff00000 | vector) : vector;
	mregs->msr = msr;
	msr_modified();
}

static int
rvec_dsi_trap( int dummy_rvec, ulong dar, ulong dsisr )
{
	//printm("DSI: DAR %08lX, DSISR %08lX\n", dar, dsisr );

	mregs->spr[S_DAR] = dar;
	mregs->spr[S_DSISR] = dsisr;
	mac_exception( 0x300, 0 );
	return 0;
}

static int
rvec_isi_trap( int dummy_rvec, ulong nip, ulong srr1 )
{
	//printm("ISI: NIP %08lX, SRR1 %08lX\n", nip, srr1 );
	mac_exception( 0x400, srr1 );
	return 0;
}

static int
rvec_alignment_trap( int dummy_rvec, ulong dar, ulong dsisr )
{
	//printm("Alignement Trap: NIP %08lX, SRR1 %08lX\n", dar, dsisr );
	mregs->spr[S_DAR] = dar;
	mregs->spr[S_DSISR] = dsisr;
	mac_exception( 0x600, 0 );
	return 0;
}


void
irq_exception( void )
{
	/* This function is called from mainloop.c */
	mac_exception( 0x500, 0 );
}

static int
rvec_altivec_trap( int dummy_rvec )
{
	if( mregs->no_altivec ) {
		printm("AltiVec is disabled\n");
		mac_exception( 0x700, MOL_BIT(12) );
	} else {
		mac_exception( 0xf20, 0 );
	}
	return 0;
}

static int
rvec_altivec_assist( int dummy_rvec, ulong srr1 )
{
	/* 7400 */
	mac_exception( 0x1600, srr1 );
	return 0;
}

static int
rvec_trace_trap( int dummy_rvec )
{
	/* This might be a debugger-trace */
	if( molcpu.break_flags & BREAK_SINGLE_STEP ) {
		rvec_break( RVEC_BREAK, BREAK_SINGLE_STEP );
		return 0;
	}
#if 1
	printm("MAC-TRACE\n");
//	stop_emulation();
#endif
	mac_exception( 0xd00, 0 );
	return 0;
}



/************************************************************************/
/*	603 Exceptions							*/
/************************************************************************/

#ifdef EMULATE_603

static void
exception_603( int trap, ulong dsisr ) 
{
	dsisr |= mregs->cr & 0xf0000000;
	mregs->gprsave_603[0] = mregs->gpr[0];
	mregs->gprsave_603[1] = mregs->gpr[1];
	mregs->gprsave_603[2] = mregs->gpr[2];
	mregs->gprsave_603[3] = mregs->gpr[3];

	mac_exception( trap, dsisr );

	mregs->msr |= MSR_TGPR;
	mregs->flag_bits |= fb_603_AltGPR;
}

static int
rvec_imiss_trap( int dummy_rvec )
{
	/* printm("IMISS trap\n"); */
	exception_603( 0x1000, MOL_BIT(13) );
	return 0;
}

static int
rvec_dmiss_load_trap( int dummy_rvec )
{
	/* printm("DMISS load trap %08lX\n", mregs->spr[S_DMISS]); */
	exception_603( 0x1100, 0 );
	return 0;
}

static int
rvec_dmiss_store_trap( int dummy_rvec )
{
	/* printm("DMISS store trap\n"); */
	exception_603( 0x1200, MOL_BIT(15) );
	return 0;
}

#endif /* EMULATE_603 */

/************************************************************************/
/*	POW 1->0 (DOZE)							*/
/************************************************************************/

static int 
rvec_msr_pow( int dummy_rvec )
{
#if 1
	mregs->msr &= ~MSR_POW;
#endif
	if( (mregs->processor >= 8 || mregs->processor == 3) 
	    && (mregs->spr[S_HID0] & (MOL_BIT(8)|MOL_BIT(9)|MOL_BIT(10))) )
		doze();
	return 0;
}


/************************************************************************/
/*	Emulation (most is done in the kernel)				*/
/************************************************************************/

static int
rvec_priv_inst( int dummy_rvec, ulong inst )
{
	int op, op_ext, b1, b2, b3;

	/* unhandled privileged instruction in supervisor mode */
	/* IMPORTANT: The GPRs are not available here! */

	op = OPCODE_PRIM( inst );
	op_ext = OPCODE_EXT( inst );
	b1 = B1( inst );	/* bit 6-10 */
	b2 = B2( inst );	/* bit 11-15 */
	b3 = B3( inst );	/* bit 16-20 */

	switch( OPCODE(op,op_ext) ) {
	case OPCODE( 31, 370 ):	/* tlbia (opt.) */
		/* not implemented on the 601,603,604,G3 (G4?) */
		break;
	case OPCODE( 31, 470 ):  /* dcbi rA,rB  -- rA=b2 rB=b3 */
		printm("dcbi treated as nop\n");
		mregs->nip += 4;
		return 0;
	default:
		printm("Unknown privileged instruction, opcode %lX\n", inst);
		stop_emulation();
		break;
	}
	mac_exception( 0x700, MOL_BIT(13) );
	return 0;
}

static int
rvec_illegal_inst( int dummy_rvec, ulong inst )
{
	/* IMPORTANT: The GPRs are not available here! */

	if( inst == BREAKPOINT_OPCODE ) {
		int val = is_stop_breakpoint( mregs->nip );
		if( val > 0 )
			stop_emulation();
		if( val ) {
			/* conditional breakpoint */
			return 0;
		}
		/* no breakpoint - take an exception */
	}

	/* printm("ILLEGAL INSTRUCTION %08lX\n", inst ); */
	mac_exception( 0x700, MOL_BIT(12) );
	return 0;
}

static void
fix_thrm_spr( void )
{
	ulong v, t;
	int i;

	if( !(mregs->spr[S_THRM3] & THRM3_E) )
		return;

	/* XXX: Thermal interrupts are unimplemented */
	for( i=S_THRM1 ; i<= S_THRM2 ; i++ ) {
		v = mregs->spr[i];
		if( !(v & THRM1_V) )
			continue;
		v |= THRM1_TIV;
		v &= ~THRM1_TIN;
		t = v & THRM1_THRES(127);
		if( (v & THRM1_TID) && t < THRM1_THRES(24) )
			v |= THRM1_TIN;
		if( !(v & THRM1_TID) && t > THRM1_THRES(24) )
			v |= THRM1_TIN;
		mregs->spr[i] = v;
	}
}

int
_rvec_spr_read( int dummy_rvec, int sprnum, int gprnum )
{
	switch( sprnum ) {
	case S_THRM1:			/* Thermal 1 */
	case S_THRM2:			/* Thermal 2 */
	case S_THRM3:			/* Thermal 3 */
		fix_thrm_spr();
		/* fall through */
	case S_L2CR:
	case S_L3CR:
	case S_MSSCR0:
		break;
	default:
		printm("Read from unimplemented SPR #%d\n", sprnum );
		stop_emulation();
	}
	mregs->gpr[gprnum] = mregs->spr[sprnum];
	mregs->nip += 4;
	return 0;
}


static int
rvec_spr_write( int dummy_rvec, int sprnum, ulong value )
{
	switch( sprnum ) {
	case S_THRM1:			/* Thermal 1 */
	case S_THRM2:			/* Thermal 2 */
	case S_THRM3:			/* Thermal 3 */
		mregs->spr[sprnum] = value;
		fix_thrm_spr();
		break;
	case S_MSSCR0:
		value &= ~MOL_BIT(8);			/* 7400 DL1HWF, L1 data cache hw flush */
		break;
	case S_L2CR:					/* L2 Cache Control Register */
		/* 750 doesn't clear L2I but 7455 does */
		value &= ~(MOL_BIT(10) | MOL_BIT(31));		/* Clear L2I and L2IP */
		break;
	case S_L3CR:					/* L3 Cache Control Register */
		value &= ~(MOL_BIT(20) | MOL_BIT(21));		/* Clear L3HWF and L3I */	
		mregs->spr[sprnum] = value;
		break;
	default:
		printm("Write to unimplemented SPR #%d (%08lX)\n", sprnum, value );
		mregs->spr[sprnum] = value;
		stop_emulation();
	}
	mregs->nip += 4;
	return 0;
}
	
static int
rvec_unusual_prog_excep( int dummy_rvec, ulong opcode, ulong srr1 )
{
	printm("Unusual program exception occured (SRR1 = %08lX)\n", srr1 );
	stop_emulation();
	return 0;
}


/************************************************************************/
/*	MMU Return Vectors						*/
/************************************************************************/

static int
rvec_unexpected_mmu_cond( int rvec, int param1, int param2 )
{
	switch( rvec ) {
	case RVEC_UNUSUAL_DSISR_BITS:	/* dar, dsisr */
		printm("RVEC_UNUSUAL_DSISR_BITS: dar %08X, dsisr %08X\n", param1, param2 );
		break;
	case RVEC_MMU_IO_SEG_ACCESS:
		printm("RVEC_MMU_IO_SEG_ACCESS\n");
		break;
	case RVEC_BAD_NIP: /* nip_phys */
		printm("Instruction Pointer not in ROM/RAM (nip %08lX, nip_phys %08X)\n", mregs->nip, param1 );
		if( !debugger_enabled() )
			quit_emulation();
		stop_emulation();
		break;
	default:
		printm("rvec_unexpected_mmu_cond, unimplemented vector %d\n", rvec );
		break;
	}
	return 0;
}


/************************************************************************/
/*	IO Read/Write							*/
/************************************************************************/

/* returns 1 if GPRs have been modified */
int
_rvec_io_read( int dummy_rvec, ulong mphys_ioaddr, void *usr_data )
{
	ulong ea, cont, inst=mregs->inst_opcode;
	int op, op_ext, rD, rA, rB, d;
	int flag=0, ret=1;

	enum {	byte=1, half=2, word=4, len_mask=7, indexed=8, update=16, 
		zero=32, reverse=64, nop=128, fpinst=256 };

	/* do_io_read() is considered FPU-unsafe */
	shield_fpu( mregs );

	/* break instruction into parts */
	op = OPCODE_PRIM( inst );	/* bit 0-5 */
	op_ext = OPCODE_EXT( inst );
	rD = B1( inst );	/* bit 6-10 */
	rA = B2( inst );	/* bit 11-15 */
	rB = B3( inst );	/* bit 16-20 */
	d = BD( inst );		/* bit 16-31 sign extended */

	switch( op ) {
	case 34: /* lbz rD,d(rA) */
		flag = byte | zero; break;
	case 35: /* lbzu rD,d(rA) */
		flag = byte | zero | update; break;
	case 40: /* lhz rD,d(rA) */
		flag = half | zero; break;
	case 41: /* lhzu rD,d(rA) */
		flag = half | zero | update; break;
	case 42: /* lha rD,d(rA) */
		flag = half; break;
	case 43: /* lhau rD,d(rA) */
		flag = half | update; break;
	case 32: /* lwz rD,d(rA) */
		flag = word | zero; break;
	case 33: /* lwzu, rD,d(rA) */
		flag = word | zero | update; break;
	case 50: /* lfd frD,d(rA) */			/* FPU */
		flag = word | fpinst | zero; break;
	case 51: /* lfdu frD, d(rA) */			/* FPU */
		flag = word | fpinst | update | zero; break;
	}

	if( !flag && op==31 ) {
		switch( op_ext ) {  /* lxxx rD,rA,rB */
		case 87: /* lbzx rD,rA,rB */
			flag = byte | indexed | zero; break;
		case 119: /* lbzux rD,rA,rB */
			flag = byte | indexed | zero | update; break;
		case 279: /* lhzx rD,rA,rB */
			flag = half | indexed | zero; break;
		case 311: /* lhzux rD,rA,rB */
			flag = half | indexed | zero | update; break;
		case 343: /* lhax rD,rA,rB */
			flag = half | indexed; break;
		case 375: /* lhaux rD,rA,rB */
			flag = half | indexed | update; break;
		case 23: /* lwzx rD,rA,rB */
			flag = word | indexed | zero; break;
		case 55: /* lwzux rD,rA,rB */
			flag = word | indexed | zero | update; break;
		case 790: /* lhbrx rS,rA,rB */
			flag = half | indexed | zero | reverse; break;
		case 534: /* lwbrx rS,rA,rB */
			flag = word | indexed | zero | reverse; break;
		case 599: /* lfdx frD,rA,rB */				/* FPU */
			flag = word | indexed | zero | fpinst; break;
		case 631: /* lfdux frD,rA,rB */				/* FPU */
			flag = word | indexed | zero | update | fpinst; break;
		case 86: /* dcbf rA,rB - cache instruction*/
			/* treat as nop if data-translation is off */
			flag = (mregs->msr & MSR_DR) ? 0 : nop; break;
		}
	}

	if( flag & len_mask) {	/* instruction found */
		if( flag & indexed ) { /* lxxx rD,rA,rB */
			ea = mregs->gpr[rB];
			ea += rA ? mregs->gpr[rA] : 0;
		} else { /* lxxx rD,d(rA) */
			ea = rA ? mregs->gpr[rA] : 0;
			ea += d;
		}

		/* ea is the mac untranslated address, */
		/* mphys_ioaddr is the mac-physical address */

		cont = 0;
		do_io_read( usr_data, mphys_ioaddr, (flag & len_mask), &cont );

		if( flag & byte ){
			cont &= 0xff;
		} else if( flag & half ) {
			cont &= 0xffff;
			if( !(flag & zero) )	/* algebraic */
				cont |= (cont & 0x8000)? 0xffff0000 : 0;
			if( flag & reverse )
				cont = bswap_16( cont );
		} else if( flag & reverse)
			cont = ld_le32(&cont);
		if( !(flag & fpinst) )
			mregs->gpr[rD] = cont;
		else {
			/* FPU instruction */
			save_fpu_completely( mregs );
			ret = 0;
			
			mregs->fpr[rD].h = cont;

			/* check for 4K boundary crossings... */
			if( ((mphys_ioaddr+4) & 0xfff) < 4 )
				printm("emulate_load_data_inst: MMU translation might be bad\n");
			do_io_read( usr_data, mphys_ioaddr+4, 4, &cont );
			mregs->fpr[rD].l = cont;

			reload_tophalf_fpu( mregs );
		}
			
		if( (flag & update) && rA && (rA!=rD || (flag & fpinst)) ) {
			ret = 1;
			mregs->gpr[rA] = ea;
		}
	}

	if( flag )
		mregs->nip += 4;
	else {
		printm("Unimplemented load instruction %08lX\n", inst );
		stop_emulation();
	}
	return ret;
}

/* returns 1 if GPRs have been modified */
int
_rvec_io_write( int dummy_rvec, ulong mphys_ioaddr, void *usr_data )
{
	int op, op_ext, rS, rA, rB, d, flag=0, ret=0;
	ulong ea, cont, len, inst=mregs->inst_opcode;

	enum {	byte=1, half=2, word=4, len_mask=7, indexed=8, update=16, 
		reverse=32, nop=64, fpinst=128 };

	/* do_io_write() is considered FPU-unsafe */
	shield_fpu( mregs );

	/* break instruction into parts */
	op = OPCODE_PRIM( inst );
	op_ext = OPCODE_EXT( inst );
	rS = B1( inst );	/* bit 6-10 */
	rA = B2( inst );	/* bit 11-15 */
	rB = B3( inst );	/* bit 16-20 */
	d = BD( inst );		/* bit 16-31 sign extended */

	switch( op ) {
	case 38: /* stb rS,d(rA) */
		flag = byte ; break;
	case 39: /* stbu rS,d(rA) */
		flag = byte | update; break;
	case 44: /* sth rS,d(rA) */
		flag = half ;  break;
	case 45: /* sthu rS,d(rA) */
		flag = half | update; break;
	case 36: /* stw rS,d(rA) */
		flag = word ; break;
	case 37: /* stwud rS,d(rA) */
		flag = word | update; break;
	case 54: /* stfd frS,d(rA) */			/* FPU */
		flag = word | fpinst; break;
	case 55: /* stfdu frS,d(rA) */			/* FPU */
		flag = word | fpinst | update; break;
	}

	if( !flag && op==31 ) {
		switch( op_ext ) {
		case 215: /* stbx rS,rA,rB */
			flag = byte | indexed ; break;
		case 247: /* stbux rS,rA,rB */
			flag = byte | indexed | update; break;
		case 407: /* sthx rS,rA,rB */
			flag = half | indexed ; break;
		case 439: /* sthux rS,rA,rB */
			flag = half | indexed | update; break;
		case 151: /* stwx rS,rA,rB */
			flag = word | indexed ; break;
		case 183: /* stwux rS,rA,rB */
			flag = word | indexed | update; break;
		case 918: /* sthbrx rS,rA,rB */
			flag = half | indexed | reverse; break;
		case 662: /* stwbrx rS,rA,rB */
			flag = word | indexed | reverse; break;
		case 727: /* stfdx frS,rA,rB */
			/* printm("FPU store inst\n"); */
			flag = word | indexed | fpinst; break;
		case 759: /* stfdux frS,rA,rB */
			/* printm("FPU store inst\n"); */
			flag = word | indexed | update | fpinst; break;
		}
	}

	if( flag & len_mask ) {	/* instruction found */
		if( flag & indexed ) { /* stxxx rS,rA,rB */
			ea = mregs->gpr[rB];
			ea += rA ? mregs->gpr[rA] : 0;
		} else { /* stxxx rS,d(rA) */
			ea = rA ? mregs->gpr[rA] : 0;
			ea += d;
		}
		if( !(flag & fpinst ) )
			cont = mregs->gpr[rS];
		else {
			save_fpu_completely( mregs );
			cont = mregs->fpr[rS].h;
		}
		len = flag & len_mask;
		if( flag & byte ) {
			cont &= 0xff;
		} else if( flag & half ) {
			if( !(flag & reverse) )
				cont &= 0xffff;
			else
				cont = bswap_16( cont );
		} else if( flag & reverse )
			cont = ld_le32(&cont);

		/* ea is the mac untranslated address,
		 * mregs->mmu_ioaddr holds the translated address
		 */
		do_io_write( usr_data, mphys_ioaddr, cont, len );
		if( flag & fpinst ) {
			if( ((mphys_ioaddr+4) & 0xfff) < 4 )
				printm("emulate store data inst: Possible MMU translation error\n");
			do_io_write( usr_data, mphys_ioaddr+4, mregs->fpr[rS].l, 4 );
		}
		
		if( (flag & update) && rA  ) {
			mregs->gpr[rA] = ea;
			ret = 1;
		}
	}

	if( flag )
		mregs->nip += 4;
	else {
		printm("Unimplemented store instruction %08lX\n", inst );
		stop_emulation();
	}
	return ret;
}

/************************************************************************/
/*	check interrupts						*/
/************************************************************************/

static int
rvec_check_irqs(int rvec)
{
	hostirq_check_irqs();

	msr_modified();

	return 0;
}

/************************************************************************/
/*	debugger CMDs							*/
/************************************************************************/

static int __dcmd
cmd_loadprog( int argc, char **argv )
{
	int fd;
	if( argc!=2 )
		return 1;

	if( (fd=open( argv[1], O_RDONLY )) < 0 ) {
		perrorm("open");
		return 0;
	}
	if( read( fd, ram.lvbase, ram.size ) < 0 )
		perrorm("read");	
	close( fd );
	return 0;
}

static int __dcmd
cmd_dt( int argc, char **argv )
{
	int i, j = (mregs->debug_trace & 0xff);
	
	if( argc != 1 )
		return 1;

	for(i=0; i<256; i++ ) {
		int val = mregs->dbg_trace_space[(j-i-1)&0xff ];
		printm("%08x ", val );
		if( i%8==7 )
			printm("\n");
	}
	return 0;
}

static int __dcmd
cmd_dtc( int argc, char **argv )
{
	int i;
	if( argc != 1 )
		return 1;
	for( i=0; i<256; i++ )
		mregs->dbg_trace_space[i]=0;
	mregs->debug_trace = 0;
	return 0;
}

static dbg_cmd_t dbg_cmds[] = {
#ifdef CONFIG_DEBUGGER
	{ "loadprog",	cmd_loadprog,	"loadprog file\nLoad macprog to address 0x0\n"	},
	{ "dt", 	cmd_dt,		"dt\nShow kernel debug trace\n"  		},
	{ "dtc", 	cmd_dtc,	"dtc\nClear debug trace\n"			},
#endif
};


/************************************************************************/
/*	initialization / cleanup					*/
/************************************************************************/

static int
save_mregs( void )
{
	return write_session_data( "REGS", 0, (char*)mregs, sizeof(mac_regs_t) );
}

static void
load_mregs( void )
{
	mac_regs_t old = *mregs;
	if( read_session_data("REGS", 0, (char*)mregs, sizeof(mac_regs_t)) )
		session_failure("Could not read registers\n");
	
	/* there are some fields we should not touch... */
	mregs->timer_stamp = old.timer_stamp;
	mregs->flag_bits = old.flag_bits;
	mregs->interrupt = 1;
}

static void
handle_spr_table( spr_range_t *table, int action )
{
	int i, j;
	for( i=0; (j=table[i].first) != -1 ; i++ )
		for( ; j <= table[i].last; j++ )
			_tune_spr( j, action );
}

static void
handle_spr_list( int *list, int action )
{
	int i;
	for( i=0; list[i] != -1; i++ )
		_tune_spr( list[i], action );
}


static void 
init_mregs( void ) 
{
	uint pvr = _get_pvr();
	int i, altivec, rev;
	cpu_entry_t *ci;
	char *pname;
	
	/* do not clear mregs here. Some fields (in particular mregs->interrupt) are in use */

	for( ci=cpuinfo; (ci->pvr & ci->mask) != (pvr & ci->mask) ; ci++ )
		;
	if( !ci->pvr )
		printm("Unknown processor id (%04X).\n", pvr );

	/* altivec available and enabled? */
	altivec = ci->altivec | (get_bool_res("altivec") == 1);
	if( get_bool_res("disable_altivec") == 1 )
		altivec = 0;

	/* determine CPU to be emulated */ 
	pname = get_str_res("processor");
	for( ;; ) {
		if( !pname )
			pname = altivec ? "G4" : "G3";

		for( ci=&cpuinfo[0] ; ci->pvr ; ci++ ) {
			if( !ci->illegal_sprs )
				continue;
			if( !strcasecmp(pname,ci->name) || (ci->alias && !strcasecmp(pname,ci->alias)) )
				break;
		}
		if( !ci->pvr )
			printm("Unknown processor type %s\n", pname );
		else if( ci->altivec && !altivec )
			printm("AltiVec unavailable (or user disabled); reverting to G3 mode\n");
		else
			break;
		pname = NULL;
	}

	/* fill in CPU parameters */
	if( loading_session() )
		load_mregs();
	else {
		mregs->no_altivec = !altivec;
		mregs->processor = ci->pvr >> 16;

		if( (rev=get_numeric_res("processor_rev")) == -1 )
			rev = ci->pvr;
		mregs->spr[S_PVR] = (ci->pvr & ~0xffff) | (rev & 0xffff);
		mregs->spr[S_HID0] = ci->hid0;

		mregs->nip = 0xfff00100;
		mregs->msr = MSR_ME + MSR_IP;
	}

	/* tune SPRs */
	for( i=0; i<1024; i++ )
		_tune_spr( i, kTuneSPR_Privileged );
	handle_spr_list( user_sprs, kTuneSPR_Unprivileged );
	handle_spr_list( rw_sprs, kTuneSPR_ReadWrite );
	handle_spr_list( ro_sprs, kTuneSPR_ReadOnly );
	handle_spr_table( ci->illegal_sprs, kTuneSPR_Illegal );

	printm("Running in PowerPC %s mode, %d MB RAM\n", ci->name, (int)ram.size/(1024*1024) );
}

static void
fix_cpu_node( void )
{
	static struct { int id; char *resname; } ntab[] = {
		{1,	"ppc601_cpu"}, 
		{3,	"ppc603_cpu"}, 
		{4,	"ppc604_cpu"},
		{8,	"ppc750_cpu"}, 
		{0xc,	"ppc7400_cpu"},
		{0x800c,"ppc7410_cpu"},
		{0x8000,"ppc7450_cpu"},
		{0,NULL}
	}, *ci;
	mol_device_node_t *dn, *par;
	int tbase, busf, clockf;
	char *filename;
	
	for( ci=&ntab[0]; ci->id && ci->id != mregs->processor; ci++ )
		;
	assert(ci->resname);

	/* add the correct CPU node to the device tree */
	par = (dn=prom_find_type("cpu"))? dn->parent : prom_find_type("cpus");
	assert(par);
	while( (dn=prom_find_type("cpu")) )
		prom_delete_node( dn );
	if( !(filename=get_str_res(ci->resname)) || !prom_import_node(par, filename) )
		fatal("import of cpu device node '%s' [%s] failed", filename, ci->resname );

	tbase = get_timebase_frequency();
	busf = get_bus_frequency();
	clockf = get_cpu_frequency();

	/* If the bus frequency check returned 0, guess it's 4 * timebase
	 * This is a valid guess for older G4s and lower */
	if (busf == 0) busf = tbase * 4;

	if( !(dn=prom_find_type("cpu")) )
		fatal("CPU node missing");	/* should never occur */
	prom_add_property( dn, "timebase-frequency", (char*)&tbase, sizeof(int) );
	prom_add_property( dn, "bus-frequency", (char*)&busf, sizeof(int) );
	prom_add_property( dn, "clock-frequency", (char*)&clockf, sizeof(int) );

	printm("Timebase: %02d.%02d MHz, Bus: %02d.%02d MHz, Clock: %02d MHz\n",
	       (tbase / 1000000), ((tbase/10000) % 100),
	       (busf / 1000000), ((busf/10000) % 100),
	       clockf / 1000000 );
}

static rvec_entry_t rvecs[] = {
	/* exceptions */
	{ RVEC_DSI_TRAP,	rvec_dsi_trap, 		"Soft DSI Trap"		},
	{ RVEC_ISI_TRAP,	rvec_isi_trap,		"Soft ISI Trap"		},
	{ RVEC_ALIGNMENT_TRAP,	rvec_alignment_trap,	"Alignment Trap"	},
	{ RVEC_ALTIVEC_UNAVAIL_TRAP, rvec_altivec_trap,	"Altivec Unavail Trap"	},
	{ RVEC_ALTIVEC_ASSIST,	rvec_altivec_assist,	"Altivec Assist"	},
	{ RVEC_TRACE_TRAP,	rvec_trace_trap,	"Trace"			},

	/* MMU Vectors */
	{ RVEC_UNUSUAL_DSISR_BITS, rvec_unexpected_mmu_cond, "Unusual DSISR"	},
	{ RVEC_MMU_IO_SEG_ACCESS, rvec_unexpected_mmu_cond, "MMU IO Seg Access"	},
	{ RVEC_BAD_NIP,		rvec_unexpected_mmu_cond, "Bad NIP"		},

	/* emulation Helpers */
	{ RVEC_PRIV_INST,	rvec_priv_inst,		"Priv Instruction"	},
	{ RVEC_ILLEGAL_INST,	rvec_illegal_inst,	"Illegal Instruction"	},
	{ RVEC_SPR_WRITE,	rvec_spr_write,		"SPR Write"		},
	{ RVEC_UNUSUAL_PROGRAM_EXCEP, rvec_unusual_prog_excep, "Unusual Prog Excep" },

	/* doze */
	{ RVEC_MSR_POW,		rvec_msr_pow,		"MSR POW (DOZE)"	},

	/* debugger */
	{ RVEC_BREAK,		rvec_break, 		"Break"			},

	/* check interrupts */
	{ RVEC_CHECK_IRQS,	rvec_check_irqs,	"check interrupts" },

#ifdef EMULATE_603
	/* 603 vectors */
	{ RVEC_DMISS_LOAD_TRAP,	rvec_dmiss_load_trap,	"Soft DMISS Load Trap"	},
	{ RVEC_DMISS_STORE_TRAP, rvec_dmiss_store_trap,	"Soft DMISS Store Trap"	},
	{ RVEC_IMISS_TRAP,	rvec_imiss_trap,	"Soft IMISS Trap"	},
#endif
};

void 
molcpu_init( void ) 
{
	memset( &molcpu, 0, sizeof(molcpu) );

	session_save_proc( save_mregs, NULL, kDynamicChunk );
	init_mregs();
	fix_cpu_node();

	set_rvecs( rvecs, sizeof(rvecs) );
	add_dbg_cmds( dbg_cmds, sizeof(dbg_cmds) );

	molcpu_arch_init();
}

void
molcpu_cleanup( void )
{
	molcpu_arch_cleanup();
}
