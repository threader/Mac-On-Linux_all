/* 
 *   Creation Date: <2002/04/27 22:11:22 samuel>
 *   Time-stamp: <2002/07/11 20:26:26 samuel>
 *   
 *	<checksum_asm.S>
 *	
 *	
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "asmdefs.h"

/* Defined in checksum.c too... */
#define X_BLOCK_SIZE		64


/************************************************************************/
/*	Video Checksum Calculation					*/
/************************************************************************/

mDEFINE(vchecksum, [BPP, BPP_SHIFT], [
	/* r3 = cksumtab, r4 = ctab_add, r5 = height, r6 = fbadd,	
	 * r7 = fbdata, r8=dirtytable, r9=dirtybit, r10=block_width
	 */
GLOBL([vchecksum_]_BPP):
	stwu	r1,-16(r1)			// Create a stack frame
	stw	r31,12(r1)

	srwi	r31,r10,4-_BPP_SHIFT		// loop count = (#pixels * _BPP)/16)
	slwi	r10,r10,_BPP_SHIFT		// r10 = (#pixels * _BPP)

	sub	r3,r3,r4			// r3 = cksumtab - ctab_add ; r4=ctab_add
	mr	r0,r9				// Save dirtybit in r0
	sub	r6,r6,r10			// r6 = adjusted fbadd
	mtctr	r5				// Put height in ctr

1:	li	r5,0				// r5 = checksum
	mr	r11,r31				// r11 = inner loop count
2:
	lwz	r10,0(r7)			// r7 = fbdata
	lwz	r9,8(r7)

	addi	r11,r11,-1			// dec block count
	rlwinm	r9,r9,1,0,31
	add	r5,r5,r10			// CHECKSUM_CALC

	lwz	r12,4(r7)
	lwz	r10,12(r7)

	.if _BPP == 1
	addi	r12,r12,0x5			// CHECKSUM_CALC
	addi	r10,r10,0x7			// CHECKSUM_CALC
	.endif
	
	cmpwi	r11,0				// block clount == 0?
	addi	r7,r7,16			// next block

	rlwinm	r12,r12,2,0,31			// CEHCKSUM_CALC
	rlwinm	r10,r10,3,0,31			// CHECKSUM_CALC

	sub	r5,r5,r9			// CHECKSUM CALC
	add	r12,r12,r10			// CHECKSUM_CALC
	add	r5,r5,r12			// CHECKSUM_CALC
	rlwnm	r5,r5,r11,0,31			// Rotate different chunks

	bne	2b

	lwzx	r9,r3,r4			// Get old checksum
	cmpw	r9,r5				// Checksum modified?
	stwux	r5,r3,r4			// Save checksum and go to next line

	beq	3f				// branch if checksum is unmodified
	
	lwz	r9,0(r8)
	or	r9,r9,r0
	stw	r9,0(r8)
3:
	add	r7,r7,r6			// next line
	addi	r8,r8,4				// Inc dirtytable pointer
	bdnz	1b

	lwz	r31,12(r1)	
	addi	r1,r1,16			// Pop stack frame
	blr	
])

/************************************************************************/
/*	Instantiate							*/
/************************************************************************/

vchecksum(1,0)
vchecksum(2,1)
vchecksum(4,2)

