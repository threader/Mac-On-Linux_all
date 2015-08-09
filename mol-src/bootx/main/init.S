/* 
 *   Creation Date: <2001/06/16 21:30:18 samuel>
 *   Time-stamp: <2002/07/20 02:17:13 samuel>
 *   
 *	<init.S>
 *	
 *	Asm glue for ELF images run inside MOL
 *   
 *   Copyright (C) 2001, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "processor.h"
#include "asmdefs.h"

#ifdef __darwin__
	.section .bss,
#else
	.section .bss
#endif
	balign_32
	.space	1024*256		// 256 K stack
stack:	.space  64

/************************************************************************/
/*	Exception Vectors						*/
/************************************************************************/

#define ILLEGAL_VECTOR( v )	.org v ; bl trap_error ;
	
	.text
GLOBL(__vectors):
	.org 0x0

	.org 0x20
GLOBL(_start):
	LOADI 	r1, stack
	bl	EXTERN(entry)
trap_error:
	mflr	r3
	LOADI	r4,EXTERN(unexpected_excep)
	mtctr	r4
	bctrl
1:	b	1b

	ILLEGAL_VECTOR( 0x100 )
	ILLEGAL_VECTOR( 0x200 )
	ILLEGAL_VECTOR( 0x300 )
	ILLEGAL_VECTOR( 0x400 )
	ILLEGAL_VECTOR( 0x500 )
	ILLEGAL_VECTOR( 0x600 )
	ILLEGAL_VECTOR( 0x700 )

	.org 0x800
	mtsprg0	r3
	mfsrr1	r3
	ori	r3,r3,0x2000
	mtsrr1	r3
	mfsprg0	r3
	rfi

	ILLEGAL_VECTOR( 0x900 )
	ILLEGAL_VECTOR( 0xa00 )
	ILLEGAL_VECTOR( 0xb00 )
	ILLEGAL_VECTOR( 0xc00 )
	ILLEGAL_VECTOR( 0xd00 )
	ILLEGAL_VECTOR( 0xe00 )
	ILLEGAL_VECTOR( 0xf00 )
	ILLEGAL_VECTOR( 0xf20 )
	ILLEGAL_VECTOR( 0x1000 )
	ILLEGAL_VECTOR( 0x1100 )
	ILLEGAL_VECTOR( 0x1200 )
	ILLEGAL_VECTOR( 0x1300 )
	ILLEGAL_VECTOR( 0x1400 )
	ILLEGAL_VECTOR( 0x1500 )
	ILLEGAL_VECTOR( 0x1600 )
	ILLEGAL_VECTOR( 0x1700 )

GLOBL(__vectors_end):
