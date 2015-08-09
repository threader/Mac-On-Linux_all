/* 
 *   Creation Date: <2000/10/29 01:55:01 samuel>
 *   Time-stamp: <2000/10/29 01:57:01 samuel>
 *   
 *	<dbdma_hw.h>
 *	
 *	Definitions for the Apple Descriptor-Based DMA controller
 *	in Power Macintosh computers. Shamelessly stoled from 
 *	<asm/dbdma.h>
 *
 *	Copyright (C) 1996 Paul Mackerras.
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_DBDMA_HW
#define _H_DBDMA_HW


/* Bits in control and status registers */
#define RUN	0x8000
#define PAUSE	0x4000
#define FLUSH	0x2000
#define WAKE	0x1000
#define DEAD	0x0800
#define ACTIVE	0x0400
#define BT	0x0100
#define DEVSTAT	0x00ff

/*
 * DBDMA command structure.  These fields are all little-endian!
 */
struct dbdma_cmd {
    unsigned short req_count;	/* requested byte transfer count */
    unsigned short command;	/* command word (has bit-fields) */
    unsigned int   phy_addr;	/* physical data address */
    unsigned int   cmd_dep;	/* command-dependent field */
    unsigned short res_count;	/* residual count after completion */
    unsigned short xfer_status;	/* transfer status */
};

/* DBDMA command values in command field */
#define OUTPUT_MORE	0	/* transfer memory data to stream */
#define OUTPUT_LAST	0x1000	/* ditto followed by end marker */
#define INPUT_MORE	0x2000	/* transfer stream data to memory */
#define INPUT_LAST	0x3000	/* ditto, expect end marker */
#define STORE_WORD	0x4000	/* write word (4 bytes) to device reg */
#define LOAD_WORD	0x5000	/* read word (4 bytes) from device reg */
#define DBDMA_NOP	0x6000	/* do nothing */
#define DBDMA_STOP	0x7000	/* suspend processing */

/* Key values in command field */
#define KEY_STREAM0	0	/* usual data stream */
#define KEY_STREAM1	0x100	/* control/status stream */
#define KEY_STREAM2	0x200	/* device-dependent stream */
#define KEY_STREAM3	0x300	/* device-dependent stream */
#define KEY_REGS	0x500	/* device register space */
#define KEY_SYSTEM	0x600	/* system memory-mapped space */
#define KEY_DEVICE	0x700	/* device memory-mapped space */

/* Interrupt control values in command field */
#define INTR_NEVER	0	/* don't interrupt */
#define INTR_IFSET	0x10	/* intr if condition bit is 1 */
#define INTR_IFCLR	0x20	/* intr if condition bit is 0 */
#define INTR_ALWAYS	0x30	/* always interrupt */

/* Branch control values in command field */
#define BR_NEVER	0	/* don't branch */
#define BR_IFSET	0x4	/* branch if condition bit is 1 */
#define BR_IFCLR	0x8	/* branch if condition bit is 0 */
#define BR_ALWAYS	0xc	/* always branch */

/* Wait control values in command field */
#define WAIT_NEVER	0	/* don't wait */
#define WAIT_IFSET	1	/* wait if condition bit is 1 */
#define WAIT_IFCLR	2	/* wait if condition bit is 0 */
#define WAIT_ALWAYS	3	/* always wait */


#endif   /* _H_DBDMA_HW */
