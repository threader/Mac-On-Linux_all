/* 
 *   Creation Date: <2002/01/14 23:29:41 samuel>
 *   Time-stamp: <2002/01/17 20:27:29 samuel>
 *   
 *	<testfile.S>
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

// otool -v -r testfile.o

	.text
GLOBL(testfunc):
#if 0
	lwz	r3,HA(-0x20+_globalsym)(r4)
	li	r4,LO(1f)
	li	r4,HI(1f)
	lwz	r3,HA(1f)(r4)
	lwz	r3,HA(lab1)(r4)
	nop
	nop
	ba	1f
	nop
	blr
1:	
	blr	
	nop

	.long	lab1
	.long	lab1
	.long	lab2
lab1:	
	addi	r3,r3,1
lab2:
	addi	r3,r3,1
	addi	r3,r3,1
	addi	r3,r3,1
#endif

