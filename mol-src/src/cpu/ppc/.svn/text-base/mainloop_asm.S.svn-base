/* 
 *   Creation Date: <2001/01/26 21:21:53 samuel>
 *   Time-stamp: <2004/02/28 21:22:48 samuel>
 *   
 *	<mainloop.S>
 *	
 *	Main loop
 *   
 *   Copyright (C) 2000, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "rvec.h"
#include "processor.h"
	
#include "mregs-offs.h"
#include "asmdefs.h"
#include "mac_registers.h"

//#define DEBUG

	
	////////////////////////////////////////////////////////////////////////
	//
	//	This file depends upon the register conventions of the platform.
	//	Linux/PPC uses the System V ABI:
	//
	//	Altered by function calls:
	//		r0,r3-r12,(r2 ?)
	//		fr0-fr13
	//		cr[0,1,5,6,7]
	//		ctr
	//		xer
	//		certain bits of fpscr
	//
	//	Not altered by function calls:	
	//		r13-r31
	//		cr[2,3,4]
	//		fr14-fr31
	//		v0-v19, vrsave		(darwin, what about Linux?)
	//		certain bits of fpscr
	//
	//	The AltiVec implementation makes the assumption that *NO*
	//	altivec registers (including VRSAVE) is touched except by us.
	//
	////////////////////////////////////////////////////////////////////////


/************************************************************************/
/*	MACROS								*/
/************************************************************************/

mDEFINE(SETVEC, [name, rvec, func, scr, scr2], [
	bl	1f
	.ascii	"_name\0"
	balign_4
1:
	LOADI	_scr,EXTERN(gRVECtable)
	LOADI	_scr2,_func
	stw	_scr2,((_rvec & RVEC_MASK) * RVEC_ESIZE)(_scr)
	mflr	_scr2
	stw	_scr2,((_rvec & RVEC_MASK) * RVEC_ESIZE + 8)(_scr)
])

mDEFINE(DBG_SAVE_FPU, [], [
#ifdef DEBUG
	mr	r9,r3
	lwz	r3,STACK_MREGS(r1)	
	bl	save_fpu_completely
	mr	r3,r9
#endif
])


GLOBL( register_asm_rvecs ):
	mflr	r5
	SETVEC(	[NOP -],		RVEC_NOP,		loop,			/**/ r3,r4 )
	SETVEC(	[Exit -],		RVEC_EXIT,		mainloop_out,		/**/ r3,r4 )
	SETVEC(	[EnableFPU -],		RVEC_ENABLE_FPU,	rvec_enable_fpu,	/**/ r3,r4 )
	SETVEC(	[EnableAltivec -],	RVEC_ENABLE_ALTIVEC,	rvec_enable_altivec,	/**/ r3,r4 )
	SETVEC( [SPR Read],		RVEC_SPR_READ,		rvec_spr_read,		/**/ r3,r4 )
	SETVEC( [IO Read],		RVEC_IO_READ,		rvec_io_read,		/**/ r3,r4 )
	SETVEC( [IO Write],		RVEC_IO_WRITE,		rvec_io_write,		/**/ r3,r4 )
#ifdef __darwin__
	SETVEC( [Call Kernel],		RVEC_CALL_KERNEL,	rvec_call_kernel,	/**/ r3,r4 )
#endif
	mtlr	r5
	blr


/************************************************************************/
/*	mainloop							*/
/************************************************************************/

	///////////////////////////////////////////////////////////////
	// molcpu_mainloop_prep( void )
	// molcpu_mainloop( void )
	//
	// The following registers hold mac-values:
	//
	//	r13-r31
	//	fr0-fr12	(if fpu_state == FPU_DIRTY)
	//	fr14-fr31
	//	v0-v31
	//
	// In particular, fr13 and fpscr *never* hold mac values.
	// (We can't touch fpscr since that might trigger a FPU exception)
	
.set STACK_SPACE,		432
.set STACK_LR,			STACK_SPACE+STACK_LR_OFFSET
.set STACK_LINKAGE_AREA,	0		// 12 bytes (8 for linux, 12 for darwin)
.set STACK_CR,			12		// 4 bytes
.set STACK_DUMMY_FP,		16		// 8 bytes
.set STACK_MREGS,		32		// 4 bytes
.set STACK_SESSION_MAGIC,	36		// 4 bytes
.set STACK_SESSION_ID,		40		// 4 bytes
.set STACK_FPRS,		48		// 256 bytes
.set STACK_GPRS,		304		// 128 bytes

GLOBL( molcpu_mainloop_prep ):
	LOADI	r8,MOL_INITIALIZE_FLAG
	b	1f					// mainloop is entered
GLOBL( molcpu_mainloop ):
	li	r8,0
1:	
	LOADVAR r12,EXTERN(mregs)
	
	// push stackframe
	stwu	r1,-STACK_SPACE(r1)
	mflr	r11
	stw	r11,STACK_LR(r1)
	mfcr	r11					// CR (cr1-cr7 needs to be saved?)
	stw	r11,STACK_CR(r1)
	mffs	fr3
	stfd	fr3,xEMULATOR_FPSCR-4(r12)		// Save emulator fpscr
	STORE_GPR_RANGE R11,R31,STACK_GPRS,r1		// don't touch r13-r31 (and f14-f31)
	STORE_GPR_RANGE R2,R2,STACK_GPRS,r1		// probably not necessary
	STORE_FPR_RANGE FR14,FR31,STACK_FPRS,r1		// don't touch f14-f31
	lwz	r11,xALTIVEC_USED(r12)			// only touch the AltiVec unit if we know
	cmpwi	r11,0					// we have one.
	beq	1f
	xVEC_LOAD	r12, /**/ r11
1:	xLOAD_FULL_FPU	r12				// loads everything (except fr13 and fpscr)
	xGPR_LOAD_RANGE	13,31,r12			// r4 = mbase

	LOADVAR r5,EXTERN(g_session_magic)
	LOADVAR r7,EXTERN(g_session_id)
	
	stw	r12,STACK_MREGS(r1)
	stw	r5,STACK_SESSION_MAGIC(r1)
	LOADI	r6,kernel_call_return			// if we are initializing
	stw	r7,STACK_SESSION_ID(r1)
	cmpwi	r8,0
	beq	loop					// jump unless we are initializing
	li	r4,MOL_ENTRY_R4_MAGIC
	or	r7,r7,r8				// set the initialization flag	
	b	switch_magic				// we want to avoid mol_interrupt
	
	balign_32
loop:	// loop (r6 is used if we initialize)
	lwz	r12,STACK_MREGS(r1)
	li	r4,MOL_ENTRY_R4_MAGIC
	lwz	r7,xINTERRUPT(r12)
	lwz	r5,STACK_SESSION_MAGIC(r1)
	cmpwi	r7,0
	stw	r4,xIN_VIRTUAL_MODE(r12)
	bne-	mol_interrupt
	lwz	r7,STACK_SESSION_ID(r1)
switch_magic:
	MOL_KERNEL_ENTRY_MAGIC				// only r1, r2 and are preserved
do_rvec_jump:						// return point, r3=rvec#, r4+=arguments
	DBG_SAVE_FPU
	lwz	r12,STACK_MREGS(r1)
	cmpwi	r3,RVEC_NOP				// optimize the common case
	li	r10,0
	beq+	loop
	LOADI	r9,EXTERN(gRVECtable)			// Jump to return vector
	stw	r10,xIN_VIRTUAL_MODE(r12)
	rlwinm	r10,r3, RVEC_ESIZE_LOG2, 31-(NRVECS_LOG2-1)-RVEC_ESIZE_LOG2, 31-RVEC_ESIZE_LOG2
	lwzx	r0,r9,r10
	mtctr	r0
#ifdef COLLECT_RVEC_STATISTICS
	addi	r10,r10,4
	lwzx	r11,r9,r10
	addi	r0,r11,1
	stwx	r0,r9,r10
#endif
#ifdef DEBUG
	stw	r3,xDBG_LAST_RVEC(r12)
#endif
	bctrl
	b	loop

mol_interrupt:
	bl	EXTERN(mainloop_interrupt)
	cmpwi	r3,0
	beq+	loop

	/* fallthrough to mainloop_out (also RVEC entrypoint) */
mainloop_out:
	lwz	r3,STACK_MREGS(r1)
	bl	save_altivec				// save_altivec( mac_regs )

	lwz	r3,STACK_MREGS(r1)
	bl	save_fpu_completely			// save_fpu_completely( mac_regs )

	lwz	r3,STACK_MREGS(r1)
	xGPR_SAVE_RANGE	13,31,r3			// save xGPR13-xGPR31
	
	lwz	r0,STACK_LR(r1)
	LOAD_GPR_RANGE R11,R31,STACK_GPRS,r1
	LOAD_FPR_RANGE FR14,FR31,STACK_FPRS,r1
	lwz	r11,STACK_CR(r1)
	mtcr	r11
	mtlr	r0
	addi	r1,r1,STACK_SPACE
	blr

	/////////////////////////////////////////////////////////////////
	// kernel_call_return
	//
	//	r3	RVEC number
	//
	// If our low-level asm kernel code calls C-kernel code directly,
	// then the return point is set to kernel_call_return rather than 
	// do_rvec_jump. This allows the C-kernel code to pass parameters 
	// to rvec functions by storing them in xRVEC_PARAM.

#ifdef __darwin__
rvec_call_kernel:
	bl	EXTERN(do_call_kernel)		// ioctl wrapper, returns rvec number
#endif

kernel_call_return:
	lwz	r12,STACK_MREGS(r1)
	lwz	r4,xRVEC_PARAM0(r12)
	lwz	r5,xRVEC_PARAM1(r12)
	lwz	r6,xRVEC_PARAM2(r12)
	b	do_rvec_jump


/************************************************************************/
/*	RVECs								*/
/************************************************************************/

save_top_gpr:
	xGPR_SAVE_RANGE	13,31,r12		// r12=mregs, save xGPR13-xGPR3
	blr
load_top_gpr:
	xGPR_LOAD_RANGE 13,31,r12
	blr	
				
rvec_enable_fpu:
	stfd	fr0,STACK_DUMMY_FP(r1)		// enable the fpu
	b	loop

rvec_enable_altivec:
	li	r3,1
	stw	r3,xALTIVEC_USED(r12)		// Useful for AltiVec detection
	.long	0x10000484			// vor	v0,v0,v0
	b	loop

rvec_spr_read:
	bl	save_top_gpr			// r13-r31, r12=mregs
	bl	EXTERN(_rvec_spr_read)
	lwz	r12,STACK_MREGS(r1)
	bl	load_top_gpr			// r13-r31, r12=mregs
	b	loop

rvec_io_read:
	bl	save_top_gpr
	bl	EXTERN(_rvec_io_read)
	cmpwi	r3,0
	lwz	r12,STACK_MREGS(r1)
	bnel	load_top_gpr
	b	loop
	
rvec_io_write:	
	bl	save_top_gpr
	bl	EXTERN(_rvec_io_write)
	cmpwi	r3,0
	lwz	r12,STACK_MREGS(r1)
	bnel	load_top_gpr
	b	loop


/************************************************************************/
/*	function calls (for now, only used internally)			*/
/************************************************************************/

	// save_altivec( mac_regs_t *mregs )
save_altivec:
	lwz	r9,xALTIVEC_USED(r3)
	cmpwi	r9,0
	beqlr
	xVEC_SAVE r3, /**/ r9
	blr


/************************************************************************/
/*	external API							*/
/************************************************************************/

	// void reload_tophalf_fpu( mac_registers_t *mregs )
	//
	// Must be called if a floating point register in the range
	// fr14-fr31 is modified from an rvec. The only case
	// ought to be rvec_io_read...

GLOBL(reload_tophalf_fpu):
	xLOAD_TOPHALF_FPU r3				// we only need to reload the top half of the fpu.
	blr


	// void save_fpu_completely( mac_registers_t *mregs )
	//
	// makes sure the complete FPU has been stored in mregs.
	// State transition: FPU_xxx -> FPU_SAVED
	
GLOBL(save_fpu_completely):
	lwz	r10,xFPU_STATE(r3)
	li	r11,FPU_STATE_SAVED
	cmpwi	r10,FPU_STATE_SAVED			// if the FPU is already saved, do nothing
	beqlr+
	stw	r11,xFPU_STATE(r3)
	xSAVE_TOPHALF_FPU r3				// save top half of the FPU
	cmpwi	r10,FPU_STATE_DIRTY			// and the lower half of FPU if its dirty
	bnelr
	xSAVE_LOW_FPU r3				// save all of the FPU
	blr


	// void shield_fpu( mac_registers_t *mregs )
	//
	// Save FPU registers that might be altered by function calls.
	// State transition: FPU_DIRTY -> FP_HALF_SAVED
	
GLOBL(shield_fpu):
	cmpwi	r3,0
	beqlr
	lwz	r9,xFPU_STATE(r3)
	li	r11,FPU_STATE_HALF_SAVED
	cmpwi	r9,FPU_STATE_DIRTY			// only save if we have touched the fpu
	bnelr+
	xSAVE_LOW_FPU	r3
	stw	r11,xFPU_STATE(r3)
	blr
		
	
#ifdef DEBUG
#warning "**************** DEBUG ENABLED IN mainloop_asm.S ***************"
#endif
