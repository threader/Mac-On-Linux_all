/* 
 *   Creation Date: <2001/06/20 23:03:30 samuel>
 *   Time-stamp: <2004/02/24 00:31:13 samuel>
 *   
 *	<performance.S>
 *	
 *	
 *   
 *   Copyright (C) 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
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

#ifdef __darwin__
#define _r0	r0
#define _r3	r3
#define _r4	r4
#define _r5	r5
#define _r13	r13
#define _f0	f0
#define _f3	f3
#define _sr0	sr0
#else
#define _f0	0
#define _f3	3
#define _r0	0
#define _r3	3
#define _r4	4
#define _r5	5
#define	_r13	13
#define _sr0	0
#endif
	
mDEFINE(TEST_, [text, code], [
	li	r30,8
4:	li	r31,4
	mtctr	r31
	b	1f
	
	balign_32
1:	isync
	nop
	nop
	mftb	r29
	
2:	mFORLOOP([i],0,31, _code ; )
	bdnz	2b
	mftb	r28
	cmpwi	r31,4
	LOADI	r31,512
	mtctr	r31
	beq	1b
	.data
3:	.ascii  "_text[]\0"
	balign_4
	.text
	addi	r30,r30,-1
	stmw	r2,16(r1)
	LOADI	r3,3b
	sub	r4,r28,r29
	mr	r5,r30
	bl	EXTERN(perf_print)
	lmw	r2,16(r1)
	cmpwi	r30,0
	bne	4b
])
mDEFINE(TEST,[code],[TEST_(_code,_code)])		

mDEFINE(PRINT,[str],[
	.data
9:	.ascii _str,"\n\0"
	balign_4
	.text
	stmw	r2,16(r1)
	LOADI	r3,9b
	bl	EXTERN(printm)
	lmw	r2,16(r1)
])

	
/************************************************************************/
/*	Test the implementation speed					*/
/************************************************************************/

GLOBL(set_usermode):	/* r3: 0=supervisor, 1=user */
	sc
	blr

GLOBL(test_opcode):	/* r3 = opcode */
	LOADI	r4,1f
	stw	r3,0(r4)
	dcbst	0,r4
	sync
	icbi	0,r4
	sync
	isync
	li	r3,0
1:	nop		/* srr1 is return in r3 if a program trap is taken */
	blr
	
GLOBL(performance):
	// r6-r8 unmodified by PRE/POST	
	mflr	r0
	stwu	r1,-16-256(r1)
	stw	r0,20+256(r1)
	stmw	r2,128+16(r1)
	li	r13,0

	li	r3,(15 << 2) | 3	/* 2 MB, Vu, Vp */
	li	r4,2			/* r/w */
	mtdbatu	0,r3
	mtdbatl	0,r4
	mtibatu	0,r3
	mtibatl	0,r4
		
	PRINT("--------------- USER MODE ------------------")
	TEST([	addi	_r3,_r3,1	]);
	TEST([	nop			]);
	TEST([	li	_r0,0		]);
	TEST([	lwz	_r3,0(0)	]);
	TEST([	lfd	_f3,0(0)	]);
	TEST([	stfd	_f3,0(0)	]);
	TEST([	9: b	9b + 4		]);
	TEST([	mftb	_r3		]);
	TEST([	mfctr	_r3		]);
	TEST([	mtlr	_r3		]);
	TEST([	mfcr	_r3		]);
	TEST([	mfxer	_r3		]);
	TEST([	mfspr	_r3,936		]);
	TEST([	mtcrf	1,_r3		]);
	TEST([	isync			]);
	TEST([	sync			]);
	li	r3,0
	TEST([	dcbz	0,_r3		]);
	TEST_([	lwarx + stwcx		],[
		lwarx	_r3,0,_r13
		stwcx.	_r3,0,_r13
	])
	TEST_([	lwzx + stwx		],[
		lwzx	_r3,0,_r13
		stwx	_r4,0,_r13
	])

	PRINT("------------ PRIVILEGED MODE ---------------")
	TEST([	mfmsr	_r3		]);
	TEST([	mtmsr	_r3		]);
	li	r4,MSR_DR | MSR_IR
	li	r3,0
	TEST_([	mtmsr MMU toggle ],[
		mtmsr _r3
		mtmsr _r4
	]);
	TEST([	tlbie	_r3	 	]);
	TEST([	tlbsync			]);
	TEST([	mfsr	_r3,_sr0	]);
	TEST([	mtsr	_sr0,_r3	]);
	TEST([	mfsrin	_r3,_r13	]);
	TEST([	mtsrin	_r3,_r13	]);
	TEST([	mtsrr1  _r3		]);
	TEST([	mfsrr1  _r3		]);
	TEST([	mfdec	_r3		]);
	TEST([	mtdec	_r3		]);
	li	r3,0
	TEST([	mtdbatu	3,_r3		]);
	TEST([	mfspr _r3,DABR	 	]);

	PRINT("---------------- MISC ----------------------")

	lis	r3,0x10
	mtsdr1	r3
	li	r4,MSR_DR | MSR_IR
	mtmsr	r4
	TEST_([	load PTE		],[
		lwz _r5,0(_r3)
	]);
	lis	r3,0x10
	TEST_([	store PTE (stw)		],[
		stw _r5,0(_r3)
	]);
	lis	r3,0x10	
	TEST_([	store PTE (stwx)	],[
		stwx _r5,0,_r3
	]);

	// test that lwarx/stwcx. works
	lis	r3,0x10	
1:	lwarx	r5,0,r3
	stwcx.	r5,0,r3
	bne-	1b

	// -------------- SPLITMODE -------------
	li	r4,MSR_DR
	mtmsr	r4

	li	r3,0
	TEST_([	splitmode - lwz		],[
		lwz _r5,0(_r3)
	]);	
	li	r3,0
	TEST_([	splitmode - dcbz	],[
		dcbz 0,_r3
	]);
	TEST_([	splitmode - dcbst	],[
		dcbst 0,_r3
	]);	

	// ----------- ZERO page test --------------
	li	r3,0
	li	r4,MSR_DR
	lis	r5,0x1c

	mtmsr	r3
	lwz	r8,0(r5)
	mtmsr	r4
	lwz	r8,0(r5)
	addi	r8,r8,1
	stw	r8,0(r5)
	mtmsr	r3
	lwz	r9,0(r5)
	cmpw	r8,r9
	bnel	zero_page_error
	
	// ----------- splitmode stwcx/lwarx --------------
	li	r3,MSR_DR
	mtmsr	r3
	li	r3,0
1:	lwarx	r5,0,r3
	stwcx.	r5,0,r3
	bne-	1b

	li	r4,0		
	mtmsr	r4
	
	// --------------- EXIT ----------------

	lmw	r2,128+16(r1)
	lwz	r0,256+20(r1)
	addi	r1,r1,256+16
	mtlr	r0
	blr

zero_page_error:
	PRINT("-- WARNING: zero-mapped page handling seems broken --")
	blr
