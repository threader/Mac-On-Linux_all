/* 
 *   Creation Date: <2003/05/26 00:23:37 samuel>
 *   Time-stamp: <2003/05/27 13:49:59 samuel>
 *   
 *	<vector-mpc.h>
 *	
 *	
 *   
 *   Copyright (C) 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_VECTOR_MPC
#define _H_VECTOR_MPC

#define	MOL_SPRG2_MAGIC		0x1779

#define	PERFMON_VECTOR		0x680

/* kernel vector head */
mDEFINE(KVEC, [v, dummy_str], [
	.section .vectors, "x"
	.subsection (_v+1)
	.org	ktrap_[]_v
])
mDEFINE(KVEC_, [v, dummy_str], [
	.section .vectors, "x"
	.subsection _v
	.org	_v
])


mDEFINE(VECTOR_, [v, dummy_str, secondary, not_mol_label], [
	.section .vectors, "x"
	.subsection _v

	/* exception entrypoint */
	.org _v
exception_vector_[]_v:
	mtsprg_a0 r3
	mfcr	  r3
	mtsprg_a1 r1
	mfsprg_a2 r1
	cmpwi	r1,MOL_SPRG2_MAGIC
	bne-	_not_mol_label
	mfsrr1	r1
	rlwinm.	r1,r1,0,MSR_PR
	mfsprg_a3 r1
	bne+	mol_trap_[]_v
	li	r3,_v
	b	_secondary
ktrap_[]_v:
	/* kernel exception handler goes here */

	.text
	balign_32
mol_trap_[]_v:
	nop
	/* MOL exception handler goes here */
])

#define VECTOR(v, dummy_str, secondary) 			\
	VECTOR_(v, dummy_str, secondary, ktrap_##v )


#define TAKE_EXCEPTION( v )					; \
	bl	take_exception					; \
	b	ktrap_##v					;

#define CONTINUE_TRAP( v )					\
	b	ktrap_##v

mDEFINE(PERFMON_VECTOR_RELOCATION, [new_vector], [
	.section .vectors, "x"
	.subsection 0xf00
	.org	0xf00
	ba	_new_vector
])

/************************************************************************/
/*	Physical/virtual conversion					*/
/************************************************************************/

	/* replaced with lis dreg,addr@ha ; addi dreg,dreg,addr@l */
#define LI_PHYS( dreg, addr ) \
	LOADI	dreg, addr

	/* syntax: lwz dreg, phys_addr(reg)
	 * replaced with addis dreg,reg,addr@ha ; lwz dreg,addr@lo(dreg).
	 */
#define LWZ_PHYSADDR_R( dreg, addr, reg )	;\
	addis	dreg,reg,HA(addr)		;\
	lwz	dreg,LO(addr)(dreg)

#define LWZ_PHYS( dreg, addr )	;\
	LWZ_PHYSADDR_R( dreg, addr, 0 );

	/* syntax: tophys rD,rS */
MACRO(tophys, [dreg, sreg], [
	mr	_dreg, _sreg
])
	/* syntax: tovirt rD,rS */
MACRO(tovirt, [dreg, sreg], [
	mr	_dreg, _sreg
])


/************************************************************************/
/*	Segment registers						*/
/************************************************************************/

MACRO(LOAD_SEGMENT_REGS, [base, scr, scr2], [
	mFORLOOP([i],0,7,[
		lwz	_scr,eval(i * 8)(_base)
		lwz	_scr2,eval((i * 8)+4)(_base)
		mtsr	srPREFIX[]eval(i*2),_scr
		mtsr	srPREFIX[]eval(i*2+1),_scr2
	])
])

MACRO(SAVE_SEGMENT_REGS, [base, scr, scr2], [
	mFORLOOP([i],0,7,[
		mfsr	_scr,srPREFIX[]eval(i*2)
		mfsr	_scr2,srPREFIX[]eval(i*2+1)
		stw	_scr,eval(i * 8)(_base)
		stw	_scr2,eval((i * 8) + 4)(_base)
	])
])


/************************************************************************/
/*	BAT register							*/
/************************************************************************/

MACRO(SAVE_DBATS, [varoffs, scr1], [
	mfpvr	_scr1
	srwi	_scr1,_scr1,16
	cmpwi	r3,1
	beq	9f
	mFORLOOP([nn],0,7,[
		mfspr	_scr1, S_DBAT0U + nn
		stw	_scr1,(_varoffs + (4 * nn))(r1)
	])
9:
])
	
MACRO(SAVE_IBATS, [varoffs, scr1], [
	mFORLOOP([nn],0,7,[
		mfspr	_scr1, S_IBAT0U + nn
		stw	_scr1,(_varoffs + (4 * nn))(r1)
	])
])


#endif   /* _H_VECTOR_MPC */
