/* 
 *   Creation Date: <2001/12/31 23:45:39 samuel>
 *   Time-stamp: <2003/08/07 02:33:11 samuel>
 *   
 *	<cache.S>
 *	
 *	Cache flushing
 *   
 *   Copyright (C) 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "asmdefs.h"

#define CACHE_LINE_SIZE		32
#define LG_CACHE_LINE_SIZE	5

/* flush_dcache_range( ulong start, ulong stop) */
GLOBL(flush_dcache_range):
	li	r5,CACHE_LINE_SIZE-1
	andc	r3,r3,r5
	subf	r4,r3,r4
	add	r4,r4,r5
	srwi.	r4,r4,LG_CACHE_LINE_SIZE
	beqlr
	mtctr	r4

1:	dcbst	0,r3
	addi	r3,r3,CACHE_LINE_SIZE
	bdnz	1b
	sync                            /* wait for dcbst's to get to ram */
	blr

/* flush_icache_range( ulong start, ulong stop) */
GLOBL(flush_icache_range):
        li      r5,CACHE_LINE_SIZE-1
        andc    r3,r3,r5
        subf    r4,r3,r4
        add     r4,r4,r5
        srwi.   r4,r4,LG_CACHE_LINE_SIZE
        beqlr
        mtctr   r4
        mr      r6,r3
1:      dcbst   0,r3
        addi    r3,r3,CACHE_LINE_SIZE
        bdnz    1b
        sync                            /* wait for dcbst's to get to ram */
        mtctr   r4
2:      icbi    0,r6
        addi    r6,r6,CACHE_LINE_SIZE
        bdnz    2b
        sync                            /* additional sync needed on g4 */
        isync
        blr

