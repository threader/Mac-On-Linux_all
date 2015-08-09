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

#define PPC_PGBYTES		4096
#define MSR_EE_BIT		16	



//---------------------------------------------------------------------------
// pmap_copy_page
//---------------------------------------------------------------------------
	
#define FM_SIZE		64

	// [10.3] void pmap_copy_page( ppnum_t src, ppnum_t dst )
AFUNC("pmap_copy_page_103", pmap_copy_page_103 ):
	slwi	r3,r3,12
	slwi	r4,r4,12
	// fallthrough

	// [10.2] void pmap_copy_page( vm_offs_t src, vm_offs_t dst )
AFUNC("pmap_copy_page", pmap_copy_page ):
#if 0
	mfpvr	r9					// Get the PVR
	rlwinm	r9,r9,16,16,31				// Isolate the PPC processor
	cmplwi	r9,PROCESSOR_VERSION_Max		// Do we have Altivec?
	beq+	wegotaltivec				// Yeah...
#endif
	mfmsr	r9					// Get the MSR
	stwu	r1,-(FM_SIZE+32)(r1)			// Make a frame for us
	rlwinm	r7,r9,0,MSR_EE_BIT+1,MSR_EE_BIT-1	// Disable interruptions
	ori	r7,r7,MSR_FP				// Turn on the FPU
	mtmsr	r7					// Disable rupts and enable FPU

	stfd	fr0,FM_SIZE+0(r1)			// Save an FP register
//	rlwinm	r7,r7,0,MSR_DR_BIT+1,MSR_DR_BIT-1	// Clear the DDAT bit
	rlwinm	r7,r7,0,28,25				// Clear MSR_DR,MSR_IR (26,27)
	stfd	fr1,FM_SIZE+8(r1)			// Save an FP register
	addi	r6,r3,PPC_PGBYTES			// Point to the start of the next page
	stfd	fr2,FM_SIZE+16(r1)			// Save an FP register
	mr	r8,r4					// Save the destination
	stfd	fr3,FM_SIZE+24(r1)			// Save an FP register
	
	mtmsr	r7					// Set the new MSR
	isync						// Ensure data translations are off

	dcbt	0, r3					// Start in first input line
	li	r5,	CACHE_LINE_SIZE			// Get the line size

.L_pmap_copy_page_loop:
	dcbz	0, r4					// Allocate a line for the output
	lfd	fr0, 0(r3)				// Get first 8
	lfd	fr1, 8(r3)				// Get second 8
	lfd	fr2, 16(r3)				// Get third 8
	stfd	fr0, 0(r4)				// Put first 8
	dcbt	r5, r3					// Start next line coming in
	lfd	fr3, 24(r3)				// Get fourth 8
	stfd	fr1, 8(r4)				// Put second 8
	addi	r3,r3,CACHE_LINE_SIZE			// Point to the next line in
	stfd	fr2, 16(r4)				// Put third 8
	cmplw	cr0,r3,r6				// See if we're finished yet
	stfd	fr3, 24(r4)				// Put fourth 8
	dcbst	0,r4					// Force it out
	addi	r4,r4,CACHE_LINE_SIZE			// Point to the next line out
	blt+	.L_pmap_copy_page_loop			// Copy the whole page
			
	sync						// Make sure they're all done
	li	r4,PPC_PGBYTES-CACHE_LINE_SIZE		// Point to the end of the page

invalinst:	
	subic.	r5,r4,CACHE_LINE_SIZE			// Point to the next one
	icbi	r4, r8					// Trash the i-cache
	subi	r4,r5,CACHE_LINE_SIZE			// Point to the next one
	icbi	r5, r8					// Trash the i-cache
	bgt+	invalinst				// Keep going until we do the page...

//	rlwimi	r7,r9,0,MSR_DR_BIT,MSR_DR_BIT		// Set DDAT if on
	rlwimi	r7,r9,0,26,27				// Restore MSR_DR, MSR_IR
	sync						// Make sure all invalidates done
			
	mtmsr	r7					// Set DDAT correctly
	isync		
			
	lfd	fr0,FM_SIZE+0(r1)			// Restore an FP register
	lfd	fr1,FM_SIZE+8(r1)			// Restore an FP register
	lfd	fr2,FM_SIZE+16(r1)			// Restore an FP register
	lfd	fr3,FM_SIZE+24(r1)			// Restore an FP register
	
	lwz	r1,0(r1)				// Pop up the stack
	
	mtmsr	r9					// Turn off FPU now and maybe rupts back on
	blr


//---------------------------------------------------------------------------
// pmap_zero_page
//---------------------------------------------------------------------------

	// [10.3] void pmap_zero_page( ppnum_t pa )
AFUNC("pmap_zero_page_103", pmap_zero_page_103 ):
	slwi	r3,r3,12				// convert pagenum to addr
	// fallthrough
			
	// [10.2] void pmap_zero_page( vm_offs_t pa )
AFUNC("pmap_zero_page", pmap_zero_page):
	
	mfmsr	r6					// Get the MSR
//	rlwinm	r7,r6,0,MSR_DR_BIT+1,MSR_DR_BIT-1	// Turn off DR
	rlwinm	r7,r6,0,28,25				// Clear MSR_DR,MSR_IR (26,27)
	rlwinm	r7,r7,0,MSR_EE_BIT+1,MSR_EE_BIT-1	// Clear MSR_EE
	li	r4,PPC_PGBYTES-CACHE_LINE_SIZE		// Point to the end of the page
	mtmsr	r7					// Set MSR to DR off

.L_phys_zero_loop:	
	subic.	r5,r4,CACHE_LINE_SIZE			// Point to the next one
	dcbz	r4, r3					// Clear the whole thing to 0s
	subi	r4,r5,CACHE_LINE_SIZE			// Point to the next one
	dcbz	r5, r3					// Clear the next to zeros
	bgt+	.L_phys_zero_loop			// Keep going until we do the page...

	sync						// Make sure they're all done
	li		r4,PPC_PGBYTES-CACHE_LINE_SIZE	// Point to the end of the page

.L_inst_inval_loop:	
	subic.	r5,r4,CACHE_LINE_SIZE			// Point to the next one
	icbi	r4, r3					// Clear the whole thing to 0s
	subi	r4,r5,CACHE_LINE_SIZE			// Point to the next one
	icbi	r5, r3					// Clear the next to zeros
	bgt+	.L_inst_inval_loop			// Keep going until we do the page...

	sync						// Make sure they're all done

	mtmsr	r6					// Restore original translations
	blr
