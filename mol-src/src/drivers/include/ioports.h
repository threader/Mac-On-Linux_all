/* 
 *   Creation Date: <97/06/29 23:34:47 samuel>
 *   Time-stamp: <2004/02/07 18:33:44 samuel>
 *   
 *	<ioports.h>
 *	
 *	Memory mapped I/O emulation
 *   
 *   Copyright (C) 1997-2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _IOPORTS_H
#define _IOPORTS_H

typedef ulong 	(*io_read_fp)( ulong addr, int len, void *usr );
typedef void 	(*io_write_fp)( ulong addr, ulong data, int len, void *usr );
typedef void 	(*io_print_fp)( int isread, ulong addr, ulong data, int len, void *usr );

typedef struct io_ops {
	io_read_fp	read;
	io_write_fp	write;
	io_print_fp	print;
} io_ops_t;

#define IO_STOP		1
#define IO_VERBOSE	4


enum{ /* flags parameter in add_io_range */
	io_stop=1, io_dolast=2, io_print=4
};

extern int 	add_io_range( ulong mbase, size_t size, char *name, int flags,
			      io_ops_t *ops, void *usr_data );
extern int	remove_io_range( int iorange_id );

/* util functions */
extern ulong 	read_mem( char *ptr, int len );
extern void 	write_mem( char *ptr, ulong data, int len );


#define eieio()	\
	({ __asm__ __volatile("eieio" :: ); })

#endif

