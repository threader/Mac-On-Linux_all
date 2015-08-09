/* 
 *   Creation Date: <1998/10/17 20:09:22 samuel>
 *   Time-stamp: <2004/02/21 21:49:55 samuel>
 *   
 *	<dumpvars.c>
 *	
 *	Utility to dumps the kernel variables
 *   
 *   Copyright (C) 1998-2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#define DUMPVARS
#include "mol_config.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include "kernel_vars.h"
#include "mac_registers.h"
#include "wrapper.h"

#define VAR(x)		#x, offsetof(struct kernel_vars,x )

typedef struct {
	int		type;
	int		tspec;		/* type specific */
	char 		*field;
	int		offs;
	char		*comment;
} entry;

#define MVAR(x)		0,0,VAR(mmu.x)
#define LABEL(x)	{ 1,0,#x,0,0 }
#define WORDS(x,y)	2,x,VAR(y)

#define BVAR(x)		0,0,VAR(_bp.x)
#define KVAR(x)		0,0,VAR(x)

#define MAC_VAR(x)	0,0,VAR(mregs.x)

static entry list[]={
  LABEL("kernel variables"),

	{KVAR(emuaccel_mphys),		" emuaccel_mphys "},
	{KVAR(emuaccel_size),		" emuaccel_size "},
	{KVAR(emuaccel_page_phys),	" emuaccel_page_phys "},
	{KVAR(emuaccel_page),		" emuaccel_page "},

	{KVAR(break_flags),		" break_flags "},

  LABEL("MMU-variables"),

	{WORDS(16,mmu.unmapped_sr ),	"unmapped sr"},
	{WORDS(16,mmu.user_sr ),	"user sr"}, 
	{WORDS(16,mmu.sv_sr ),		"supervisor sr"}, 
	{WORDS(16,mmu.split_sr ),	"split sr"}, 

	{MVAR(cur_sr_base ),		" cur_sr_base "},
	{MVAR(sr_inst ),		" sr_inst "},
	{MVAR(sr_data ),		" sr_data "},
#if 0  
	{MVAR(segr_unmapped ),		" context of mac, running unmapped "},
	{MVAR(segr_mapped_u ),		" context of mac, running mapped, u-mode "},
	{MVAR(segr_mapped_s ),		" context of mac, running mapped, sv-mode "},

	{MVAR(cur_context ),		" context (segment base) of mac running "},
	{MVAR(inst_context ),		" context (used in mmu split mode) "},
#endif
	{WORDS(2, _bp.ibat_save[0] ), "IBAT0 (hi,lo)"},
	{WORDS(2, _bp.ibat_save[1] ), "IBAT1 (hi,lo)"},
	{WORDS(2, _bp.ibat_save[2] ), "IBAT2 (hi,lo)"},
	{WORDS(2, _bp.ibat_save[3] ), "IBAT3 (hi,lo)"},

	{WORDS(2, _bp.dbat_save[0] ), "DBAT0 (hi,lo)"},
	{WORDS(2, _bp.dbat_save[1] ), "DBAT1 (hi,lo)"},
	{WORDS(2, _bp.dbat_save[2] ), "DBAT2 (hi,lo)"},
	{WORDS(2, _bp.dbat_save[3] ), "DBAT3 (hi,lo)"},

	{MVAR(userspace_ram_base ),	" userspace ram base "},
	{MVAR(ram_size ),		" "},

	{MVAR(bats[0].mbase ),		" ibat0.mbase "},
	{MVAR(bats[0].size ),		" ibat0.size "},
	{MVAR(bats[4].mbase ),		" dbat0.mbase "},
	{MVAR(bats[4].size ),		" dbat0.size "},

	{MVAR(split_dbat0.word[0] ),	" split-dbat0 (upper) "},
	{MVAR(split_dbat0.word[1] ),	" split-dbat0 (lower) "},
	{MVAR(transl_dbat0.word[0] ),	" transl-dbat0 (upper) "},
	{MVAR(transl_dbat0.word[1] ),	" transl-dbat0 (lower) "},

	{MVAR(hash_mbase ),		" hash_mbase (mphys)"},
	{MVAR(hash_base ),		" hash_base (lvptr)"},
	{MVAR(hash_mask ),		" hash_mask"},
	{MVAR(pthash_sr ),		" pthash_sr"},
	{MVAR(pthash_ea_base ),		" pthash_ea_base"},
#ifdef __darwin__
  LABEL("SDR1 related"),
	{MVAR(os_sdr1 ),		" os_sdr1"},
	{MVAR(mol_sdr1 ),		" mol_sdr1"},   
#endif

  LABEL("Private variables to 'base.S'"),

	{BVAR(split_nip_segment),	" split_nip_segment "},
	{BVAR(emulator_stack),		" saved stack (used when going to mac_mode) "},
	{BVAR(dec_stamp),  		" linux DEC = dec_stamp - TBL"},
	{BVAR(int_stamp),  		" next DEC event = int_stamp - TBL"},

  LABEL("Mac-registers"),
	{WORDS(32, mregs.gpr ), "GPR0-GPR31"},
  
	{MAC_VAR(nip),			" Instruction pointer "},
	{MAC_VAR(msr),			" Machine state register (virtual) "},
	{BVAR(_msr),			" MSR - physically used in emulated process "},
	{MAC_VAR(cr),			" Condition register "},

	{WORDS(64, mregs.fpr ),  	" FPR0-FPR31 "},
	{MAC_VAR(fpscr),		" fp. status and control register"},

	{MAC_VAR(link),			" Link register "},
	{MAC_VAR(xer),			" Integer exception register "},
	{MAC_VAR(ctr),			" Count register "},

/*	{WORDS(NUM_SPRS, mregs.spr ), "SPRs"}, */

	{WORDS(16, mregs.segr ), "segment registers"},

	{MAC_VAR(inst_opcode),		" valid only after data/program check exception "},
	{MAC_VAR(dec_stamp),		" xDEC = dec_stamp - TBL "},

	{MAC_VAR(interrupt),		" set if a interrupt is flagged "},
	{MAC_VAR(flag_bits),		" flag_bits (fbXXX) "},

	{MAC_VAR(fpu_state),		" fpu_state"},
	{MAC_VAR(processor),		" processor"},
	{MAC_VAR(altivec_used),		" altivec_used"},
	{MAC_VAR(no_altivec),		" no altivec"},

	{WORDS(NUM_SPRS, _bp.spr_hooks ),  "SPR Hooks"}, 

	{WORDS(10,mregs.debug),		"Debug space" },
	{MAC_VAR(dbg_last_rvec),	"Dbg, last RVEC call" },
	{MAC_VAR(dbg_last_osi),		"Dbg, last osi selector" },
	{0,0,NULL, 0,NULL  }
};


static int mol_fd = -1;

int
mol_ioctl( int cmd, int p1, int p2, int p3 ) 
{
	mol_ioctl_pb_t pb;
	int ret;
	pb.arg1 = p1;
	pb.arg2 = p2;
	pb.arg3 = p3;
	ret = ioctl( mol_fd, cmd, &pb );
#ifdef __darwin__ 
	if( !ret )
		ret = pb.ret;
#endif
	return ret;
}

int 
main( int argc, char **argv )
{
	struct kernel_vars kvars;
	mac_regs_t *mregs;
	int i, j, sess_id;

	sess_id = (argc < 2)? 0 : atoi( argv[1] );
	
	if( getuid() ) {
		fprintf( stderr, "Must be run as root!\n");
		exit(1);
	}
	mol_fd = open("/dev/mol", O_RDWR );
	if( _dbg_copy_kvars(sess_id, &kvars) ) {
		fprintf( stderr, "Failed to get kvars (session %d)\n", sess_id );
		exit(1);
	}
	close( mol_fd );

	mregs = &kvars.mregs;

	for( i=0; list[i].field; i++ ) {
		ulong	*p, val;
		int 	j;

		switch( list[i].type ) {
		case 0:
			val = *(ulong*) ((char*)&kvars+list[i].offs );
			printf("%08lX  %-22s %s\n",val, list[i].field, list[i].comment );
			break;
		case 1:
			printf("\n*** %s ***\n", list[i].field );
			break;
		case 2:
			printf("%s %s\n   ",list[i].field, list[i].comment );
			p = (ulong*)((char*)&kvars + list[i].offs );
			for(j=0; j<list[i].tspec; j++, p++ ){
				printf("%08lX ", *p );
				if( j % 6 == 5 )
					printf("\n   ");
			}
			printf("\n");
		break;
		}
	}

	/* debug trace */
	printf("Debug Trace:\n");
	j = (mregs->debug_trace & 0xff);
       	for(i=0; i<64; i++ ) {
		int val = mregs->dbg_trace_space[(j-i-1)&0xff ];
		printf("%08x ", val );
		if( i%8==7 )
			printf("\n");
	}

	return 0;
}
