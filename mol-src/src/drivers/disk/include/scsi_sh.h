/* 
 *   Creation Date: <2003/07/09 12:55:41 samuel>
 *   Time-stamp: <2003/07/30 00:32:08 samuel>
 *   
 *	<scsi_sh.h>
 *	
 *	SCSI interface
 *   
 *   Copyright (C) 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_SCSI_SH
#define _H_SCSI_SH

/* scatter and gather list element. The [0] element in a list is used to link
 * to another sg list.
 */
typedef struct {
	int		base;
	int		size;
} sg_t;

typedef struct sglist {
	int		n_el;			/* #el in this list */
	int		next_sglist;		/* next sg list (phys addr) */
	struct sglist	*drv_next;		/* driver private field */
	sg_t		vec[0];
} sglist_t;

typedef struct {
	int		client_addr;		/* client virtual address of this request */
	int		next_completed;		/* client address of next completed */

	unsigned char	cdb[16];

	unsigned char	cdb_len;
	unsigned char	target;
	unsigned char	lun;
	unsigned char	is_write;		/* host to device */

	unsigned char	scsi_status;		/* SCSI status */
	unsigned char	adapter_status;		/* adapter status */
	unsigned char	filler[2];

	/* returned sense */
	int		ret_sense_len;		/* returned sense len */
	sg_t		sense[2];		/* sense sg bufs (both must be filled in!) */
	
	/* data buffer */
	unsigned int	size;			/* buffer size (all might not be in the sglist) */
	unsigned int	act_size;		/* transfered size */
	unsigned int	n_sg;			/* #sg bufs (total) */
	sglist_t	sglist;			/* scatter and gather buffers (must be last!) */
} scsi_req_t;

enum {
	kAdapterStatusNoError	= 0,
	kAdapterStatusNoTarget
};

#define SCSI_CTRL_INIT		1
#define SCSI_CTRL_EXIT		2

#define SCSI_ACK_IRQFLAG	1		/* always set */
#define SCSI_ACK_REQADDR_MASK	~3

#endif   /* _H_SCSI_SH */


