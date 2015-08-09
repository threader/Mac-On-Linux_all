/* 
 *   Creation Date: <2001/06/16 21:30:18 samuel>
 *   Time-stamp: <2004/02/24 00:40:53 samuel>
 *   
 *	<init.S>
 *	
 *	Asm glue for ELF images run inside MOL
 *   
 *   Copyright (C) 2001, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "processor.h"
#include "asmdefs.h"
#include "osi.h"

#define	STACK_SIZE	(1024*256)
.comm	stackspace,STACK_SIZE


/************************************************************************/
/*	Exception Vectors						*/
/************************************************************************/

#define VECTOR( v, dummy_str )		.org __vectors + v ; vector__##v
#define ILLEGAL_VECTOR( v, dummy )	.org __vectors + v ; bl trap_error ;

	.text
__vectors:
ILLEGAL_VECTOR( 0x100, "SystemReset" )
ILLEGAL_VECTOR( 0x200, "MachineCheck" )
ILLEGAL_VECTOR( 0x300, "DSI" )
ILLEGAL_VECTOR( 0x400, "ISI" )
ILLEGAL_VECTOR( 0x500, "IRQ" )
ILLEGAL_VECTOR( 0x600, "Alignment" )

VECTOR( 0x700, "Program" ):	/* skip over - used to probe illegal instructions */
	mfsrr0	r3
	addi	r3,r3,4
	mtsrr0	r3
	mfsrr1	r3		/* modify r3 */
	rfi

VECTOR( 0x800, "FPU Unavailable" ):
	mtsprg0	r3
	mfsrr1	r3
	ori	r3,r3,0x2000
	mtsrr1	r3
	mfsprg0	r3
	rfi

ILLEGAL_VECTOR( 0x900, "DEC" )
ILLEGAL_VECTOR( 0xa00, "Reserved" )
ILLEGAL_VECTOR( 0xb00, "Reserved" )

// r3=1 -> supervisor mode, r3=0 -> usermode
VECTOR( 0xc00, "Syscall" ):
	mtsprg0	r4
	mtsprg1	r5
	mfsrr1	r4
	li	r5,MSR_PR
	andc	r4,r4,r5
	cmpwi	r3,0
	beq	1f
	ori	r4,r4,MSR_PR	
1:	mtsrr1	r4
	mfsprg0	r4
	mfsprg1	r5
	rfi

ILLEGAL_VECTOR( 0xd00, "Trace" )
ILLEGAL_VECTOR( 0xe00, "FPU assist" )
ILLEGAL_VECTOR( 0xf00, "Performance monitor" )
ILLEGAL_VECTOR( 0xf20, "AltiVec" )
ILLEGAL_VECTOR( 0x1000, "Instruction TLB Miss - 603" )
ILLEGAL_VECTOR( 0x1100, "DataLoadTLBMiss - 603" )
ILLEGAL_VECTOR( 0x1200, "DataStoreTLBMiss - 603" )
ILLEGAL_VECTOR( 0x1300, "Instruction Breakpoint" )
ILLEGAL_VECTOR( 0x1400, "System Management Interrupt" )
ILLEGAL_VECTOR( 0x1500, "reserved" )
ILLEGAL_VECTOR( 0x1600, "AltiVec Assist" )
ILLEGAL_VECTOR( 0x1700, "Thermal Management Interrupt" )


trap_error:
	mflr	r3
	LOADI	r4,EXTERN(unexpected_exception)
	mtctr	r4
	bctrl
1:	b	1b
	
__vectors_end:

/************************************************************************/
/*	Entrypoint, Asm globals						*/
/************************************************************************/

GLOBL(_start):
	LOADI	r1,stackspace
	LOADI	r3,(STACK_SIZE-4096)
	add	r1,r1,r3
	rlwinm	r1,r1,0,~0xf
	li	r3,0
	mtmsr	r3

	// copy exception vectors
	LOADI	r3,__vectors
	LOADI	r5,(__vectors_end+16)
	li	r4,0
	sub	r5,r5,r3
	rlwinm	r5,r5,0,0,28
1:	lwz	r6,0(r3)
	lwz	r7,4(r3)
	lwz	r8,8(r3)
	lwz	r9,12(r3)
	stw	r6,0(r4)
	stw	r7,4(r4)
	stw	r8,8(r4)
	stw	r9,12(r4)
	dcbst	0,r4
	sync
	icbi	0,r4
	sync
	addi	r5,r5,-16
	addi	r3,r3,16
	addi	r4,r4,16
	cmpwi	r5,0
	bgt	1b
	isync

	bl	EXTERN(entry)

	li	r3,OSI_EXIT
	b	osi_call
	
GLOBL(osi_call):
	mr	r5,r3
	mr	r6,r4
	mr	r7,r5
	mr	r8,r6
	mr	r9,r7
	LOADI	r3,OSI_SC_MAGIC_R3
	LOADI	r4,OSI_SC_MAGIC_R4
	sc
	blr
