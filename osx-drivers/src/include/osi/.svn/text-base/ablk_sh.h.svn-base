/* 
 *   Creation Date: <2002/05/19 13:53:40 samuel>
 *   Time-stamp: <2003/02/22 22:55:04 samuel>
 *   
 *	<ablk_sh.h>
 *	
 *	Asynchronous block driver interface
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_ABLK_SH
#define _H_ABLK_SH

/* OSI_ABlkCntrl selectors */
#define kABlkReset		1
#define kABlkStop		2
#define kABlkStart		3
#define kABlkRouteIRQ		4		/* oldworld support */
#define kABlkGetEvents		5

/* descriptor flags */
#define	ABLK_READ_REQ		1		/* param field contains start sector */
#define ABLK_WRITE_REQ		2		/* param field contains start sector */
#define ABLK_RAISE_IRQ		4
#define ABLK_SG_BUF		8		/* this is a scatter & gather buf */
#define ABLK_FIRST_CNTRL_REQ	256
#define ABLK_CNTRL_REQ_MASK	~(ABLK_FIRST_CNTRL_REQ - 1)
#define ABLK_NOP_REQ		(1 << 8)	/* noop, used to abort operations */
#define ABLK_EJECT_REQ		(2 << 8)	/* eject media */
#define ABLK_LOCKDOOR_REQ	(3 << 8)	/* prevent user eject */

/* request descriptors */
typedef struct {
	int		flags;			/* r/w flag, irq notification */
	int		proceed;		/* set when next element is valid */
	int		unit;			/* unit number */
	unsigned int	param;			/* meaning depends upon the flags field */
} ablk_req_head_t;

/* scatter and gater buf */
typedef struct {
	int		flags;			/* ABLK_SG_BUF is set */
	int		proceed;
	unsigned long	buf;			/* mphys of buf */
	unsigned long	count;			/* #bytes in buf */
} ablk_sg_t;


/* info.flags */
#define ABLK_INFO_READ_ONLY		1
#define	ABLK_INFO_REMOVABLE		2
#define ABLK_INFO_MEDIA_PRESENT		4
#define ABLK_BOOT_HINT			8
#define ABLK_INFO_CDROM			16
#define ABLK_INFO_DRV_DISK		128

/* returned in r4-r5, OSI_ABLK_DISK_INFO */
typedef struct ablk_disk_info {
	int	nblks;			/* #512 byte blocks */
	int	flags;
	int	res1;
	int	res2;
} ablk_disk_info_t;

/* 
 * A ring is used for driver -> MOL communication.
 *
 *	...	
 *	[el1]	<---- MOL req_head pointer
 *	[el2]	
 *	...
 *
 * The driver sets el1.proceed when el2 is valid. When the
 * processing of el2 has started, then req_head pointer is
 * advanced and el1.proceed and el1.flags are cleared. The
 * el1 slot may then be reused.
 *
 * When kAblkStart is called, element 0 should be treated as busy.
 * The driver may set el0.flags to ABLK_NOP_REQ in order to
 * simplify slot allocation.
 *
 * The driver can check for completion though the request
 * count returned by ablk_irq_ack. Each descriptor is
 * included in this count. When el1 has been processed,
 * the count is 1.
 *
 */

/* bits returned in the event field of OSI_ABlkAckIRQ */
#define ABLK_EVENT_DISK_INSERTED	1

#endif   /* _H_ABLK_SH */

