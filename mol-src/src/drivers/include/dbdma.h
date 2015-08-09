/* 
 *   Creation Date: <1999/03/01 17:27:56 samuel>
 *   Time-stamp: <2003/06/02 12:37:59 samuel>
 *   
 *	<dbdma.h>
 *	
 *	
 *   
 *   Copyright (C) 1999, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_DBDMA
#define _H_DBDMA

/* all variables should be considered read only */
struct dma_read_pb {
	int	res_count;
	int	req_count;
	char	*buf;
	
	int	key;
	int	is_last;

	/* private variables */
	ulong	identifier;
};

/* all variables should be considered read only */
struct dma_write_pb {
	int	res_count;
	int	req_count;
	
	int	key;
	int	is_last;

	/* private variable */
	ulong	identifier;
};

typedef char	(*dma_sbits_func)( int irq, int write_mask, char write_data );

extern void	allocate_dbdma( int gc_offs, char *name, int irq, 
				dma_sbits_func status_bits_func, long user );
extern void	release_dbdma( int irq );

extern long	dbdma_get_user( int irq );
extern void	dbdma_sbits_changed( int irq );


/* WAIT conditions */
extern int	dma_wait( int irq, int flags, struct timespec *abs_timeout );
extern int	dma_wait_timeout( int irq, int flags, struct timeval *timeout );
extern void	dma_cancel_wait(int irq, int dma_wait_flags, int retval );

/* DMA_READ, DMA_WRITE, DMA_RW is valid for flags value of dma_wait. */
#define	DMA_READ		1
#define	DMA_WRITE		2
#define	DMA_RW			3

#define DMA_ERROR		-1
#define DMA_TIMEOUT		5


/* DEVICE -> DMA chip (dma INPUT) */
extern int	dma_write_req( int irq, struct dma_write_pb *pb );
extern int	dma_write_ack( int irq, char *buf, int len, struct dma_write_pb *pb );

/* DMA chip -> DEVICE (dma OUTPUT) */
extern int	dma_read_req( int irq, struct dma_read_pb *pb );
extern int	dma_read_ack( int irq, int len, struct dma_read_pb *pb );

#endif   /* _H_DBDMA */

