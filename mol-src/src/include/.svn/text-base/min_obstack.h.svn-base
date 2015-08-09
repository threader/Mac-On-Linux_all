/* 
 *   Creation Date: <2001/12/27 20:51:57 samuel>
 *   Time-stamp: <2004/01/17 14:12:14 samuel>
 *   
 *	<obstack.h>
 *	
 *	Minimal obstack implementation
 *   
 *   Copyright (C) 2001, 2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_OBSTACK
#define _H_OBSTACK

#ifdef HAVE_OBSTACK_H
#include <obstack.h>
#else

/*****************************************************
   Minimal obstack implementation
*****************************************************/

struct obstack {
	/* opaque */
	struct obstack_chunk	*first;
	struct obstack_chunk	*last;
	int			loffs;		/* offset to free space in last chunk */	
	char			*grow_obj;
	void 			*(*ch_alloc)(size_t);
	void			(*ch_free)(void*);
};

/* provided by client: obstack_chunk_alloc, obstack_chunk_free */

#define obstack_init( os ) obstack_init_( os, obstack_chunk_alloc, obstack_chunk_free )
extern int obstack_init_( struct obstack *os, void *(*ch_alloc)(size_t), void (*ch_free)(void*) );
extern void *obstack_finish( struct obstack *os );

extern void obstack_free( struct obstack *os, void *object );
extern void obstack_grow( struct obstack *os, const void *data, int size );
extern void obstack_grow0( struct obstack *os, const void *data, int size );
extern void obstack_1grow( struct obstack *os, char c );
extern void *obstack_alloc( struct obstack *os, int size );
extern void *obstack_copy( struct obstack *os, const void *addr, int size );
extern void *obstack_copy0( struct obstack *os, const void *addr, int size );

extern int obstack_object_size( struct obstack *os );

#endif	/* HAVE_OBSTACK */
#endif  /* _H_OBSTACK */
