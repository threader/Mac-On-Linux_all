/* 
 *   Creation Date: <1999/07/17 13:36:38 samuel>
 *   Time-stamp: <2003/06/02 13:33:13 samuel>
 *   
 *	<gc.h>
 *	
 *	gc/mac-io
 *   
 *   Copyright (C) 1999, 2000, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_GC
#define _H_GC

#include "promif.h"
#include "ioports.h"

typedef struct {
	int			nbars;
	int			offs[5];
	int			size[5];
} gc_bars_t;

/* private to gc.c */
typedef struct gc_range {
	char			*name;
	io_ops_t		*ops;
	void 			*usr;

	int			offs;
	int			size;
	int			flags;

	struct gc_range		*next;
	int			range_id;
	int			is_mapped;
	ulong			base;
} gc_range_t;

#define get_mbase(gc_range) 	((gc_range)->base)

extern int 		is_gc_child( mol_device_node_t *dn, gc_bars_t *ret_regbars );
extern gc_range_t	*add_gc_range( int offs, int size, char *name, int flags,
				       io_ops_t *ops, void *usr );
extern int		gc_is_present( void );


#endif   /* _H_GC */





