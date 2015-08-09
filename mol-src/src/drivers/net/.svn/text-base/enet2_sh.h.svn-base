/* 
 *   Creation Date: <2002/05/23 23:14:01 samuel>
 *   Time-stamp: <2003/02/23 13:06:10 samuel>
 *   
 *	<enet2_sh.h>
 *	
 *	
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_ENET2_SH
#define _H_ENET2_SH

/* This number is used to prompt the user to upgrade the driver */
#define OSX_ENET_DRIVER_VERSION		1
/* This number is used to prevent old drivers from beeing loaded */
#define OSI_ENET2_API_VERSION		1


/* control request commands */
#define kEnet2CheckAPI			0
#define kEnet2Start			1
#define kEnet2Stop			2
#define kEnet2Reset			3
#define kEnet2OSXDriverVersion		4

/* ring setup */ 
#define kEnet2SetupRXRing		1
#define kEnet2SetupTXRing		2
#define kEnet2SetupRXOverflowRing	3
#define kEnet2SetupTXOverflowRing	4

/* ring flags */
#define kEnet2SplitBufferFlag		1	/* overflow buffer used */

/* ring element (all rings must have a 2^n size) */
typedef struct {
	int	buf;			/* mphys of buffer */
	int	bsize;			/* size of buffer */
	int	psize;			/* set/cleared when the buffer is done */
	int	flags;			/* buffer flags (set by driver) */
} enet2_ring_t;

/* RX buffers: Driver sets buf, bsize, user1 and clears psize
   psize is set by MOL when a buffer is received */

/* TX buffers: Driver sets buf, bsize, user1 and psize. MOL clears
   psize when buffer has been transmitted */

/* IMPORTANT: all rx buffer must have two bytes head room (_not_ included
 * in bsize). I.e, buf-2 must be within the rx-buffer. The reason for
 * this is the packet padding performed by for instance the tun driver.
 */

#endif   /* _H_ENET2_SH */
