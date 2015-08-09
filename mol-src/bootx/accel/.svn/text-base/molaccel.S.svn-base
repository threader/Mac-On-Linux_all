/*
 * Mol adaption, (C) 2002, 2003 Samuel Rydh, <samuel@ibrium.se>
 */
/*
 * Copyright (c) 2000, 2002, 2004 Apple Computer, Inc. All rights reserved.
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

#include "mol_config.h"
#include "processor.h"
#include "asmdefs.h"
#include "osi.h"

/*
 * OS X uses MMU splitmode in certain situations. Unfortunately, data
 * access from MMU splitmode is quite expensive in MOL. To speed up
 * things, the most commonly used functions must be replaced with 
 * non-splitmode equivalents.
 *
 * Most routines in this file are unmodified, except that the MSR_IR 
 * bit is cleared as well as MSR_DR (we rely on a 1-1 MMU mapping).
 * 
 * Absolute addressing is prohibited since this code is relocated...
 */
	

#define AFUNC(s,func)				\
	.data 17 ; func##_str:			;\
	.ascii	s,"\0" ; balign_4 		;\
	.data 19 ; .long func##_str, func	;\
	.text ; GLOBL(func)

	.data 18
GLOBL(accel_table__start):
	.data 20
GLOBL(accel_table__end):	
	

/************************************************************************/

	.text	
GLOBL(accel_start):	
		
	// write_hook( addr, inst )
AFUNC("write_hook", write_hook):

	mfmsr	r6
	rlwinm	r7,r6,0,28,25			// Clear MSR_DR,MSR_IR (26,27)
	mtmsr	r7				// this might be an implcit branch...	

	stw	r4,0(r3)
	dcbst	0,r3
	sync
	icbi	0,r3
	sync
	isync
		
	mtmsr	r6
	blr


/************************************************************************/
/*	hw_cpv								*/
/************************************************************************/

#define mbvrswap	16
	
	// hw_cpv( struct mapping *mp )
AFUNC("hw_cpv", hw_cpv):
	rlwinm. r4,r3,0,0,19		// Round back to the mapping block allocation control block
	mfmsr   r10			// Get the current MSR
	beq-	hcpvret			// Skip if we are passed a 0...
	andi.   r9,r10,0x7fcf		// Turn off NSR_EE, MSR_IR and MSR_DR
	mtmsr   r9			// Disable DR and EE
	
	lwz	r4,mbvrswap(r4)		// Get the conversion value
	mtmsr   r10			// Interrupts and DR back on
	xor	r3,r3,r4			// Convert to physical

hcpvret:
	rlwinm	r3,r3,0,0,26		// Clean out any flags
	blr

	
/************************************************************************/
/*	misc
/************************************************************************/

#include "bcopy.S"			// bcopy_phys etc.
#include "cache.S"			// sync_cache	
#include "pmap.S"			// pmap_copy_page etc.

	
/************************************************************************/
/*	10.3
/************************************************************************/

AFUNC("ml_set_physical", ml_set_physical ):
	li	r0,0
	oris	r0,r0,(MSR_VEC >> 16)
	mfmsr	r11
//	ori	r0,r0,MSR_FP | MSR_DR 
	ori	r0,r0,MSR_FP | MSR_DR | MSR_IR
	andc	r2,r11,r0
	mtmsr	r2
	blr

//AFUNC("ml_restore", ml_restore ):
//	mtmsr	r11
//	blr
	
GLOBL(accel_end):
