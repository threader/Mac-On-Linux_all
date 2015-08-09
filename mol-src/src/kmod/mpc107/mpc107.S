/* 
 *   Creation Date: <2003/05/27 13:43:06 samuel>
 *   Time-stamp: <2003/05/28 15:53:21 samuel>
 *   
 *	<mpc107.S>
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

/* The CPU spins here after a forced reset (i.e. a generated machine check) */
KVEC_(0xe0, "MPC107 hold")
mpchold:
1:	b	1b
	b	_start

KVEC(0x100, "Reset")
	ba	0xfff00200

KVEC(0x200, "Machine Check")
	ba	0xfff00200

		
	.text
GLOBL(_start):
	/* disable MMU */
	mfmsr	r3
	rlwinm	r3,r3,0,~(MSR_IR | MSR_DR)
	rlwinm	r3,r3,0,~(MSR_EE)
	mtmsr	r3
	isync

	/* clear bss (and stack) */
	LOADI	r3,__bss_start
	LOADI	r4,_end
	li	r5,0
1:	stw	r5,0(r3)
	addi	r3,r3,4
	cmpw	r3,r4
	blt	1b

	/* initialize stack pointer */
	LOADI	r1,_stack_end
	bl	start
	
//	ba	0xfff00200
	b	mpchold
