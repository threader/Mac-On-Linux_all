/*
 * Mol adaption, (C) 2002, 2003, 2004 Samuel Rydh, <samuel@ibrium.se>
 *
 * Copyright (c) 2000, 2002 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

	
/************************************************************************/
/*	sync_cache
/************************************************************************/

#define CACHE_LINE_SIZE		32
#define CACHE_LINE_POW2		5
	
	// [10.2-10.3] sync_cache( vm_offs_t pa, unsigned count )
AFUNC("sync_cache", sync_cache):
	mfmsr	r6
	rlwinm	r7,r6,0,28,25			// Clear MSR_DR,MSR_IR (26,27)
	mtmsr	r7				// this might be an implcit branch...	

	/* Check to see if the address is aligned. */
	add	r8,r3,r4
	andi.   r8,r8,(CACHE_LINE_SIZE-1)
	beq-    .L_sync_check
	addi    r4,r4,CACHE_LINE_SIZE
	li	 r7,(CACHE_LINE_SIZE-1)		// Align buffer & count - avoid overflow problems
	andc    r4,r4,r7
	andc    r3,r3,r7
	
.L_sync_check:
	cmpwi   r4,	CACHE_LINE_SIZE
	ble	.L_sync_one_line
	
	/* Make ctr hold count of how many times we should loop */
	addi    r8,r4,(CACHE_LINE_SIZE-1)
	srwi    r8,r8,CACHE_LINE_POW2
	mtctr   r8

	/* loop to flush the data cache */
.L_sync_data_loop:
	subic   r4,r4,CACHE_LINE_SIZE
	dcbf    r3,r4
	bdnz    .L_sync_data_loop
	
	sync
	mtctr   r8

	/* loop to invalidate the instruction cache */
.L_sync_inval_loop:
	icbi    r3,r4
	addic   r4,r4,CACHE_LINE_SIZE
	bdnz    .L_sync_inval_loop

.L_sync_cache_done:
	sync					// Finish physical writes
	mtmsr   r6				// Restore original translations */
	blr

.L_sync_one_line:
	dcbf    0,r3
	sync
	icbi    0,r3
	b	.L_sync_cache_done

