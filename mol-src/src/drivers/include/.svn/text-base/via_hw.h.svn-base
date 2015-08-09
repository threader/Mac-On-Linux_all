/* 
 *   Creation Date: <1999/07/18 01:48:16 samuel>
 *   Time-stamp: <1999/07/18 01:48:51 samuel>
 *   
 *	<via_hw.h>
 *	
 *   Copyright (C) 1999 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_VIA_HW
#define _H_VIA_HW

#define	VIA_REG_COUNT			16
#define	VIA_REG_LEN			0x200

enum {
	B,
	A,
	DIRB,
	DIRA,
	T1CL,
	T1CH,
	T1LL,
	T1LH,
	T2CL,
	T2CH,
	SR,
	ACR,
	PCR,
	IFR,
	IER,
	ANH
};

enum {
	/* data B bits */
	TREQ		= 0x08,
	TACK		= 0x10,
	TIP		= 0x20,

	/* auxillary control bits */
	SR_CTRL		= 0x1c,
	SR_EXT		= 0x0c,
	SR_OUT		= 0x10,
	T1MODE		= 0xc0,
	T1MODE_CONT	= 0x40,

	/* interrupt flag bits */
	IER_SET		= 0x80,
	IER_CLR		= 0,
	SR_INT		= 0x04,
	T1_INT		= 0x40,
	T2_INT		= 0x20,
	SR_DATA_INT	= 0x08,
	SR_CLOCK_INT	= 0x10,
	ONESEC_INT	= 0x01,
	IRQ_LINE	= 0x80,
};


#endif   /* _H_VIA_HW */
