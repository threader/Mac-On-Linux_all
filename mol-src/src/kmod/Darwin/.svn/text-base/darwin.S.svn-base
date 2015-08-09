/* 
 *   Creation Date: <2004/02/01 19:22:29 samuel>
 *   Time-stamp: <2004/03/13 18:55:05 samuel>
 *   
 *	<darwin.S>
 *	
 *	
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */


	//////////////////////////////////////////////////////////////
	// call_kernel (call mol kernel routine)
	//	r3	kernel routine
	//	r4..r6	args
	//
	// On stack:		nip, ctr, lr, xer, r0-r12
	// In registers:	r13-r31

call_kernel_save:
	bl	save_middle_regs
call_kernel:
	stw	r3,K_KCALL_ROUTINE(r1)
	stw	r4,K_KCALL_ARG0(r1)
	stw	r5,K_KCALL_ARG1(r1)
	stw	r6,K_KCALL_ARG2(r1)
	li	r3,RVEC_CALL_KERNEL
	b	mac_exit



	/* r3=ea, r4=pte_slot, r5=pte1, r6=pte2, r7=lock, r8=lockval, M: r0 */
RELOC_LOW(xx_store_pte_lowmem)
1:	lwarx	r0,0,r7
	cmpwi	r0,0
	bne-	1b
	stwcx.	r8,0,r7			// take hash lock
	bne-	1b
	// isync			// sync below is sufficient
	stw	r0,0(r4)		// clear old PTE[V] (if we evict something)
	sync				// probably not needed - no other CPU uses this PTE
	stw	r6,4(r4)		// store PTE2
	eieio
	stw	r5,0(r4)		// store PTE1
	tlbie	r3			// flush old pte
	eieio				// order tlbie before tlbsync
	tlbsync				// ensure tlbie finish on all processors
	sync				// ensure tlbsync completed
	stw	r0,0(r7)		// release hash lock
	blr
RELOC_LOW_END(xx_store_pte_lowmem)


	/* r3=ea, r7=lock, r8=lockval, M: r0 */
RELOC_LOW(xx_tlbie_lowmem)
1:	lwarx	r0,0,r7
	cmpwi	r0,0
	bne-	1b
	stwcx.	r8,0,r7			// take hash lock
	bne-	1b
	//isync				// needed if we get rid of the sync
	sync				// make sure any PTE zero-outs have finished
	tlbie	r3			// flush old pte
	eieio				// order tlbie before tlbsync
	tlbsync				// ensure tlbie finish on all processors
	sync				// ensure tlbsync completed
	stw	r0,0(r7)		// release hash lock
	blr
RELOC_LOW_END(xx_tlbie_lowmem)
