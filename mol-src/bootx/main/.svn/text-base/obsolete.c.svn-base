/* 
 *   Creation Date: <2002/08/18 21:24:42 samuel>
 *   Time-stamp: <2003/01/27 02:09:14 samuel>
 *   
 *	<obsolete.c>
 *	
 *	OBSOLETE things (unused)
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *
 */


/* f1-f3 field */
#define FD		1		/* field is dest reg */
#define FS		2		/* field is source reg */
#define FSz		4		/* field is source reg or 0 */
#define Fz		8		/* field must be zero */
#define Fx		16		/* don't care */
#define FSD		(FS|FD)		/* field is both source and dest reg */

/* flags field */
#define REC_ZERO	1		/* require bit31 to be zero */
#define HAS_OE		2		/* has OE bit (affects opcode) */
#define REC_ONE		4		/* require bit31 to be one */
#define IS_BRANCH	8		/* is a branch, sc or trap */
#define SIDE_EFFECTS	16		/* has side effects */



/************************************************************************/
/*	helper functions (determine unused registers)			*/
/************************************************************************/

static inst_t inst_table[] = {
	{ 31, 266,	FD,FS,FS,	HAS_OE,		NULL },		/* add */
	{ 31, 10,	FD,FS,FS,	HAS_OE,		NULL },		/* addc */
	{ 31, 138,	FD,FS,FS,	HAS_OE,		NULL },		/* adde */
	{ 14, -1,	FD,FSz,Fx,	0,		NULL },		/* addi */
	{ 12, -1,	FD,FS,Fx,	0,		NULL },		/* addic */
	{ 13, -1,	FD,FS,Fx,	0,		NULL },		/* addic. */
	{ 15, -1,	FD,FSz,Fx,	0,		NULL },		/* addis */
	{ 31, 234,	FD,FS,Fz,	HAS_OE,		NULL },		/* addme */
	{ 31, 202,	FD,FS,Fz,	HAS_OE,		NULL },		/* addze */
	{ 31, 28,	FS,FD,FS,	0,		NULL },		/* - and */
	{ 31, 60,	FS,FD,FS,	0,		NULL },		/* - andc */
	{ 28, -1,	FS,FD,Fx,	0,		NULL },		/* - andi */
	{ 29, -1,	FS,FD,Fx,	0,		NULL },		/* - andis */
	{ 18, -1,	Fx,Fx,Fx,	IS_BRANCH,	NULL },		/* b, bl, ba, bla */
	{ 16, -1, 	Fx,Fx,Fx,	IS_BRANCH, 	NULL },		/* bc */
	{ 19, 528, 	Fx,Fx,Fx,	IS_BRANCH, 	NULL },		/* bcctr */
	{ 19, 16, 	Fx,Fx,Fx,	IS_BRANCH, 	NULL },		/* bclr */
	{ 31, 0, 	Fx,FS,FS,	REC_ZERO, 	NULL },		/* cmp */
	{ 11, -1, 	Fx,FS,Fx,	0,	 	NULL },		/* cmpi */
	{ 31, 32, 	Fx,FS,FS,	REC_ZERO, 	NULL },		/* cmpl */
	{ 10, -1, 	Fx,FS,Fx,	0, 		NULL },		/* cmpli */
	{ 31, 26, 	FS,FD,Fx,	0, 		NULL },		/* cntlzw */
	{ 19, 257, 	Fx,Fx,Fx,	REC_ZERO, 	NULL },		/* crand */
	{ 19, 129, 	Fx,Fx,Fx,	REC_ZERO, 	NULL },		/* crandc */
	{ 19, 289, 	Fx,Fx,Fx,	REC_ZERO, 	NULL },		/* creqv */
	{ 19, 225, 	Fx,Fx,Fx,	REC_ZERO, 	NULL },		/* crnand */
	{ 19, 33, 	Fx,Fx,Fx,	REC_ZERO, 	NULL },		/* crnor */
	{ 19, 449, 	Fx,Fx,Fx,	REC_ZERO, 	NULL },		/* cror */
	{ 19, 417, 	Fx,Fx,Fx,	REC_ZERO, 	NULL },		/* crorc */
	{ 19, 193, 	Fx,Fx,Fx,	REC_ZERO, 	NULL },		/* crxor */
	{ 31, 758, 	Fz,FS,FS,	REC_ZERO, 	NULL },		/* dcba */
	{ 31, 86, 	Fz,FS,FS,	REC_ZERO, 	NULL },		/* dcbf */
	{ 31, 470, 	Fz,FS,FS,	REC_ZERO, 	NULL },		/* dcbi */
	{ 31, 54, 	Fz,FS,FS,	REC_ZERO, 	NULL },		/* dcbst */
	{ 31, 278, 	Fz,FS,FS,	REC_ZERO, 	NULL },		/* dcbt */
	{ 31, 246, 	Fz,FS,FS,	REC_ZERO, 	NULL },		/* dcbtst */
	{ 31, 1014, 	Fz,FS,FS,	REC_ZERO, 	NULL },		/* dcbz */
	{ 31, 491, 	FD,FS,FS,	HAS_OE, 	NULL },		/* divw */
	{ 31, 459, 	FD,FS,FS,	HAS_OE, 	NULL },		/* divwu */
	{ 31, 310, 	FD,FSz,FS,	REC_ZERO, 	NULL },		/* eciwx */
	{ 31, 438, 	FS,FSz,FS,	REC_ZERO, 	NULL },		/* ecowx */
	{ 31, 854, 	Fz,Fz,Fz,	REC_ZERO, 	NULL },		/* eieio */
	{ 31, 284, 	FS,FD,FS,	0,	 	NULL },		/* eqv */
	{ 31, 954, 	FS,FD,Fz,	0,	 	NULL },		/* extsb */
	{ 31, 922, 	FS,FD,Fz,	0,	 	NULL },		/* extsh */
	/* floating point operations skipped */
	{ 31, 982, 	Fz,FS,FS,	REC_ZERO,	NULL },		/* icbi */
	{ 19, 150, 	Fz,Fz,Fz,	REC_ZERO,	NULL },		/* isync */
	{ 34, -1,	FD,FSz,Fx,	0,		NULL },		/* lbz */
	{ 35, -1,	FD,FSD,Fx,	0,		NULL },		/* lbzu */
	{ 31, 119,	FD,FSD,FS,	REC_ZERO,	NULL },		/* lbzux */
	{ 31, 87,	FD,FSz,FS,	REC_ZERO,	NULL },		/* lbzx */
	{ 50, -1,	FD,FSz,Fx,	0,		NULL },		/* lfd */
	{ 51, -1,	FD,FSD,Fx,	0,		NULL },		/* lfdu */
	{ 31, 631,	FD,FSD,FS,	REC_ZERO,	NULL },		/* lfdux */
	{ 31, 599,	FD,FSz,FS,	REC_ZERO,	NULL },		/* lfdx */
	{ 48, -1,	FD,FSz,Fx,	0,		NULL },		/* lfs */
	{ 49, -1,	FD,FSD,Fx,	0,		NULL },		/* lfsu */
	{ 31, 567,	FD,FSD,FS,	REC_ZERO,	NULL },		/* lfsux */
	{ 31, 535,	FD,FSz,FS,	REC_ZERO,	NULL },		/* lfsx */
	{ 31, 535,	FD,FSz,FS,	REC_ZERO,	NULL },		/* lfsx */
	{ 42, -1,	FD,FSz,Fx,	0,		NULL },		/* lha */
	{ 43, -1,	FD,FSD,Fx,	0,		NULL },		/* lhau */
	{ 31, 375,	FD,FSD,FS,	REC_ZERO,	NULL },		/* lhaux */
	{ 31, 343,	FD,FSz,FS,	REC_ZERO,	NULL },		/* lhax */
	{ 31, 790,	FD,FSz,FS,	REC_ZERO,	NULL },		/* lhbrx */
	{ 40, -1,	FD,FSz,Fx,	0,		NULL },		/* lhz */
	{ 41, -1,	FD,FSD,Fx,	0,		NULL },		/* lhzu */
	{ 31, 311,	FD,FSD,FS,	REC_ZERO,	NULL },		/* lhzux */
	{ 31, 279,	FD,FSz,FS,	REC_ZERO,	NULL },		/* lhzx */
	{ 46, -1,	FD,FSz,Fx,	0,		NULL },		/* lmw */
	{ 31, 597,	FD,FSz,Fx,	REC_ZERO,	NULL },		/* lswi */
	{ 31, 533,	FD,FSz,FS,	REC_ZERO,	NULL },		/* lswx */
	{ 31, 20,	FD,FSz,FS,	REC_ZERO,	NULL },		/* lwarx */
	{ 31, 534,	FD,FSz,FS,	REC_ZERO,	NULL },		/* lwbrx */
	{ 32, -1,	FD,FSz,Fx,	0,		NULL },		/* lwz */
	{ 33, -1,	FD,FSD,Fx,	0,		NULL },		/* lwzu */
	{ 31, 55,	FD,FSD,FS,	REC_ZERO,	NULL },		/* lwzux */
	{ 31, 23,	FD,FSz,FS,	REC_ZERO,	NULL },		/* lwzx */
	{ 19, 0,	Fx,Fx,Fz,	REC_ZERO,	NULL },		/* mcrf */
	{ 63, 64,	Fx,Fx,Fz,	REC_ZERO,	NULL },		/* mcrfs */
	{ 31, 512,	Fx,Fz,Fz,	REC_ZERO,	NULL },		/* mcrxr */
	{ 31, 19,	FD,Fz,Fz,	REC_ZERO,	NULL },		/* mfcr */
	{ 63, 583,	Fx,Fz,Fz,	0,		NULL },		/* mffs */
	{ 31, 83,	FD,Fz,Fz,	REC_ZERO,	NULL },		/* mfmsr */
	{ 31, 339,	FD,Fx,Fx,	REC_ZERO,	NULL },		/* mfspr */
	{ 31, 595,	FD,Fx,Fz,	REC_ZERO,	NULL },		/* mfsr */
	{ 31, 659,	FD,Fz,FS,	REC_ZERO,	NULL },		/* mfsrin */
	{ 31, 371,	FD,Fx,Fx,	REC_ZERO,	NULL },		/* mftb */
	{ 31, 144,	FS,Fx,Fx,	REC_ZERO,	NULL },		/* mtcrf */
	{ 63, 70,	Fx,Fz,Fz,	0,		NULL },		/* mtfsb0 */
	{ 63, 38,	Fx,Fz,Fz,	0,		NULL },		/* mtfsb1 */
	{ 63, 711,	Fx,Fx,Fx,	0,		NULL },		/* mtfsf */
	{ 63, 134,	Fx,Fz,Fx,	0,		NULL },		/* mtfsfi */
	{ 31, 146,	FS,Fz,Fz,	REC_ZERO,	NULL },		/* mtmsr */
	{ 31, 467,	FS,Fx,Fx,	REC_ZERO,	NULL },		/* mtspr */
	{ 31, 210,	FS,Fx,Fz,	REC_ZERO,	NULL },		/* mtsr */
	{ 31, 242,	FS,Fz,FS,	REC_ZERO,	NULL },		/* mtsrin */
	{ 31, 75,	FD,FS,FS,	0,		NULL },		/* mulhw */
	{ 31, 11,	FD,FS,FS,	0,		NULL },		/* mulhwu */
	{ 7, -1,	FD,FS,Fx,	0,		NULL },		/* mulli */
	{ 31, 235,	FD,FS,FS,	HAS_OE,		NULL },		/* mullw */
	{ 31, 476,	FS,FD,FS,	0,		NULL },		/* nand */
	{ 31, 104,	FD,FS,Fz,	HAS_OE,		NULL },		/* neg */
	{ 31, 124,	FS,FD,FS,	0,		NULL },		/* nor */
	{ 31, 444,	FS,FD,FS,	0,		NULL },		/* or */
	{ 31, 412,	FS,FD,FS,	0,		NULL },		/* orc */
	{ 24, -1,	FS,FD,Fx,	0,		NULL },		/* ori */
	{ 25, -1,	FS,FD,Fx,	0,		NULL },		/* oris */
	{ 19, 50,	Fz,Fz,Fz,	REC_ZERO|IS_BRANCH,NULL },	/* rfi */
	{ 20, -1,	FS,FSD,Fx,	0,		NULL },		/* rlwimi */
	{ 21, -1,	FS,FD,Fx,	0,		NULL },		/* rlwinm */
	{ 23, -1,	FS,FD,FS,	0,		NULL },		/* rlwnm */
	{ 17, 1,	Fz,Fz,Fz,	REC_ZERO|IS_BRANCH,NULL },	/* sc */
	{ 31, 24,	FS,FD,FS,	0,		NULL },		/* slw */
	{ 31, 792,	FS,FD,FS,	0,		NULL },		/* sraw */
	{ 31, 824,	FS,FD,Fx,	0,		NULL },		/* srawi */
	{ 31, 536,	FS,FD,FS,	0,		NULL },		/* srw */
	{ 38, -1,	FS,FSz,Fx,	0,		NULL },		/* stb */
	{ 39, -1,	FS,FSD,Fx,	0,		NULL },		/* stbu */
	{ 31, 247,	FS,FSD,FS,	REC_ZERO,	NULL },		/* stbux */
	{ 31, 215,	FS,FSz,FS,	REC_ZERO,	NULL },		/* stbx */
	{ 54, -1,	Fx,FSz,Fx,	0,		NULL },		/* stfd */
	{ 55, -1,	Fx,FSD,Fx,	0,		NULL },		/* stfdu */
	{ 31, 759,	Fx,FSD,FS,	REC_ZERO,	NULL },		/* stfdux */
	{ 31, 727,	Fx,FSz,FS,	REC_ZERO,	NULL },		/* stfdx */
	{ 31, 983,	Fx,FSz,FS,	REC_ZERO,	NULL },		/* stfiwx */
	{ 52, -1,	Fx,FSz,Fx,	0,		NULL },		/* stfs */
	{ 53, -1,	Fx,FSD,Fx,	0,		NULL },		/* stfsu */
	{ 31, 695,	Fx,FSD,FS,	REC_ZERO,	NULL },		/* stfsux */
	{ 31, 663,	Fx,FSz,FS,	REC_ZERO,	NULL },		/* stfsx */
	{ 44, -1,	FS,FSz,Fx,	0,		NULL },		/* sth */
	{ 31, 918,	FS,FSz,FS,	REC_ZERO,	NULL },		/* sthbrx */
	{ 45, -1,	FS,FSD,Fx,	0,		NULL },		/* sthu */
	{ 31, 439,	FS,FSD,FS,	REC_ZERO,	NULL },		/* sthux */
	{ 31, 407,	FS,FSz,FS,	REC_ZERO,	NULL },		/* sthx */
	{ 47, -1,	FS,FSz,Fx,	SIDE_EFFECTS,	NULL },		/* stmw */
	{ 31, 725,	FS,FSz,Fx,	SIDE_EFFECTS|REC_ZERO,NULL },	/* stswi */
	{ 31, 661,	FS,FSz,FS,	SIDE_EFFECTS|REC_ZERO,NULL },	/* stswx */
	{ 36, -1,	FS,FSz,Fx,	0,		NULL },		/* stw */
	{ 31, 662,	FS,FSz,FS,	REC_ZERO,	NULL },		/* stwbrx */
	{ 31, 150,	FS,FSz,FS,	REC_ONE,	NULL },		/* stwcx. */
	{ 37, -1,	FS,FSD,Fx,	0,		NULL },		/* stwu */
	{ 31, 183,	FS,FSD,FS,	REC_ZERO,	NULL },		/* stwux */
	{ 31, 151,	FS,FSz,FS,	REC_ZERO,	NULL },		/* stwx */
	{ 31, 40,	FD,FS,FS,	HAS_OE,		NULL },		/* subf */
	{ 31, 8,	FD,FS,FS,	HAS_OE,		NULL },		/* subfc */
	{ 31, 136,	FD,FS,FS,	HAS_OE,		NULL },		/* subfe */
	{ 8, -1,	FD,FS,Fx,	0,		NULL },		/* subfic */
	{ 31, 232,	FD,FS,Fz,	HAS_OE,		NULL },		/* subfme */
	{ 31, 200,	FD,FS,Fz,	HAS_OE,		NULL },		/* subfze */
	{ 31, 598,	Fz,Fz,Fz,	REC_ZERO,	NULL },		/* sync */
	{ 31, 370,	Fz,Fz,Fz,	REC_ZERO,	NULL },		/* tlbia */
	{ 31, 306,	Fz,Fz,FS,	REC_ZERO,	NULL },		/* tlbie */
	{ 31, 566,	Fz,Fz,Fz,	REC_ZERO,	NULL },		/* tlbsync */
	{ 31, 4,	Fx,FS,FS,	REC_ZERO|IS_BRANCH,NULL },	/* tw */
	{ 3, -1,	Fx,FS,Fx,	IS_BRANCH,	NULL },		/* twi */
	{ 31, 316,	FS,FD,FS,	0,		NULL },		/* xor */
	{ 26, -1,	FS,FD,Fx,	0,		NULL },		/* xori */
	{ 27, -1,	FS,FD,Fx,	0,		NULL },		/* xoris */
};

static ulong
field_to_bit( ulong mask, ulong inst, int ignore_zero )
{
	int i, dr = (inst & mask);
	
	if( ignore_zero && !dr )
		return 0;
	
	for( i=0; i<31 && !(mask & 1); i++ ) {
		mask = mask >> 1 ;
		dr = dr >> 1;
	}
	return MOL_BIT(dr);
}


static int
find_free_reg_( ulong inst, ulong *src_regs, ulong *dest_regs )
{
	ulong prim = OPCODE(inst);
	ulong sec = OPCODE_EXT(inst);
	int i;

	for( i=0; i<sizeof(inst_table)/sizeof(inst_table[0]); i++ ) {
		inst_t *p = &inst_table[i];
		
		// opcode matching
		if( (p->flags & HAS_OE) )
			sec &= ~MOL_BIT(22);
		if( p->prim_opc != prim || (p->sec_opc >=0 && p->sec_opc != sec) )
			continue;
		if( ((p->f1 & Fz) && B1(inst)) || ((p->f2 & Fz) && B2(inst)) || ((p->f3 & Fz) && B3(inst)) )
			continue;
		if( (p->flags & REC_ZERO) && (inst & 1) )
			continue;
		if( (p->flags & REC_ONE) && !(inst & 1) )
			continue;
		if( (p->flags & (IS_BRANCH | SIDE_EFFECTS)) )
			return 1;

		// instruction matched
		if( (p->f1 & FS) )
			*src_regs |= field_to_bit( 0x03e00000, inst, (p->f1 & FSz) );
		if( (p->f2 & FS) )
			*src_regs |= field_to_bit( 0x001f0000, inst, (p->f2 & FSz) );
		if( (p->f3 & FS) )
			*src_regs |= field_to_bit( 0x0000f800, inst, (p->f3 & FSz) );
		
		if( (p->f1 & FD) )
			*dest_regs |= field_to_bit( 0x03e00000, inst, 0 );
		if( (p->f2 & FD) )
			*dest_regs |= field_to_bit( 0x001f0000, inst, 0 );
		if( (p->f3 & FD) )
			*dest_regs |= field_to_bit( 0x0000f800, inst, 0 );
		
		return 0;
	}
	return -1;
}

static int
find_free_reg( ulong *p, int dr )
{
	int i, j;
	ulong dreg=0;
	ulong sreg=MOL_BIT(dr);
	
	for( i=0; i<20; i++, p++ ) {
		if( find_free_reg_( *p, &sreg, &dreg ))
			return -1;
		if( dreg & ~sreg ) {
//			printm("Found register %08lX %08lx\n", dreg, sreg );
			for( j=0; j<32; j++ )
				if( dreg & ~sreg & MOL_BIT(j) )
					return j;
			printm("internal error\n");
			return -1;
		}
	}
	return -1;
}


#if 0
int
encode_mregs_write( ulong *d, encode_info_t *info )
{
//	ulong maddr = (ulong)mregs + info->mregs_offs;
//	int c=0;
	int r, sr=info->r;

	r = find_free_reg( info->inst_ptr + 1, sr );
	if( r < 0 ) {
//		printm("mtmsr: Failed to find free register @ %08x\n", (ulong)info->inst_ptr );
		return 0;
	}
//	printm("mtmsr: Free register %d (sr = %d) @ %08x\n", r, sr, (ulong)info->inst_ptr );

	return 0;
}

int
handle_mtmsr( inst_t *rec, encode_info_t *p )
{
	static int count=0, miss=0;
	int r;
	
	p->mregs_offs = OFFS(msr);
	p->encoder = encode_mregs_write;

	r = find_free_reg( p->inst_ptr + 1, p->r );
	if( r<0 )
		miss++;
	count++;
	printm("[%d/%d] ", count, miss );
	printm("mtmsr @ %08lX  [r%d]\n", (ulong)p->inst_ptr, r );
	return 1;
}
#endif
