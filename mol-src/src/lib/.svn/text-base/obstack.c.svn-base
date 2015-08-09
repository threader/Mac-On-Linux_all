/* 
 *   Creation Date: <2001/12/27 21:10:14 samuel>
 *   Time-stamp: <2001/12/29 19:30:41 samuel>
 *   
 *	<obstack.c>
 *	
 *	Obstack compatibility
 *   
 *   Copyright (C) 2001 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "min_obstack.h"
#include "mol_assert.h"
#include <sys/param.h>

#ifdef HAVE_OBSTACK_H
#error "Uhu...? obstack.c should only be compiled if obstack support is missing"
#endif

typedef struct obstack_chunk chunk_t;
typedef struct obstack os_t;

#define MIN_CHUNK_SIZE	4096

struct obstack_chunk {
	chunk_t 	*next;
	int		size;
};

int
obstack_init_( os_t *os, void *(*ch_alloc)(size_t), void (*ch_free)(void*) )
{
	os->first = NULL;
	os->last = NULL;
	os->grow_obj = NULL;
	os->loffs = sizeof(chunk_t);

	os->ch_alloc = ch_alloc;
	os->ch_free = ch_free;
	return 1;
}

static chunk_t *
find_chunk( os_t *os, void *object )
{
	chunk_t *p;
	for( p=os->first; p; p=p->next )
		if( (char*)object >= (char*)&p[1] && (char*)object < (char*)p + p->size )
			return p;
	return NULL;
}

void 
obstack_free( os_t *os, void *object )
{
	chunk_t **pp, *np, *p, *next;

	pp = &os->first;
	os->last = NULL;
	if( object ) {
		p = find_chunk( os, object );
		pp=&p->next;
		os->last=p;
	}
	np = *pp;
	*pp = NULL;

	for( ; np ; np=next ){
		next = np->next;
		os->ch_free( np );
	}
	os->grow_obj = object;

	os->loffs = !os->last ? sizeof(chunk_t) : (int)object - (int)os->last;
}

static void
alloc_chunk( os_t *os, int min_size )
{
	int size = MIN_CHUNK_SIZE;
	chunk_t *p;
	
	min_size += sizeof(chunk_t);
	while( size < min_size )
		size = size << 1;
	
	p = (chunk_t*)os->ch_alloc( size );
	
	p->next = NULL;
	p->size = size;
	if( !os->first ) {
		os->first = os->last = p;
	} else {
		os->last->next = p;
		os->last = p;
	}
	os->loffs = sizeof(chunk_t);
}

static int 
room_avail( os_t *os, int size, char **ptr )
{
	int s;

	if( !os->last || !(s=os->last->size - os->loffs) ) {
		alloc_chunk(os, 0);
		s = os->last->size - os->loffs;
	}
	*ptr = (char*)os->last + os->loffs;
	return MIN( s, size );
}

int 
obstack_object_size( os_t *os )
{
	chunk_t *p;
	int size=0;
	
	if( !(p = find_chunk( os, os->grow_obj )) )
		return 0;

	size = -(os->grow_obj - (char*)p);
	for( ; p->next; p=p->next )
		size += p->size;
	size += os->loffs;

	return size;
}

void *
obstack_finish( os_t *os )
{
	chunk_t *p, *pf;
	char *obj, *d;
	int s, size;

	if( os->grow_obj >= (char*)os->last && os->grow_obj < (char*)os->last + os->loffs ){
		obj = os->grow_obj;
		os->grow_obj = (char*)os->last + os->loffs;
		return obj;
	}
	
	/* Allocate space for new chunk */
	s = obstack_object_size( os );

	size = MIN_CHUNK_SIZE;
	while( size < s+sizeof(chunk_t) )
		size = size << 1;
	p = (chunk_t*)os->ch_alloc( size );
	p->next = NULL;
	p->size = size;

	/* Copy chunk */
	pf = find_chunk( os, os->grow_obj );
	d = (char*)p + sizeof(chunk_t);
	size = pf->size - ((char*)os->grow_obj - (char*)pf);
	memcpy( d, os->grow_obj, size );
	d += size;
	for( pf=pf->next; pf->next; pf=pf->next ) {
		memcpy( d, (char*)pf + sizeof(chunk_t), pf->size );
		d += pf->size;
	}
	memcpy( d, os->last + sizeof(chunk_t), os->loffs - sizeof(chunk_t) );

	/* Free original data */
	obstack_free( os, os->grow_obj );

	/* Add new chunk to list */
	if( !os->last ) {
		os->first = os->last = p;
	} else {
		os->last->next = p;
		os->last = p;
	}
	os->loffs = sizeof(chunk_t) + s;
	os->grow_obj = (char*)p + os->loffs;

	return (char*)p + sizeof(chunk_t);
}

void 
obstack_grow( os_t *os, const void *data, int size )
{
	char *p;
	int s;
	
	while( size ) {
		s = room_avail( os, size, &p );
		memcpy( p, data, s );

		size -= s;
		data += s;
		os->loffs += s;
	}
}

void 
obstack_1grow( os_t *os, char ch )
{
	obstack_grow( os, &ch, 1 );
}

void
obstack_grow0( os_t *os, const void *data, int size )
{
	obstack_grow( os, data, size );
	obstack_1grow( os, 0 );
}

void *
obstack_alloc( os_t *os, int size )
{
	char *p;

	if( !os->last || os->last->size - os->loffs < size )
		alloc_chunk( os, size );
	p = (char*)os->last + os->loffs;
	os->loffs += size;
	os->grow_obj = (char*)os->last + os->loffs;
	return p;
}

void *
obstack_copy( os_t *os, const void *src, int size )
{
	void *dest = obstack_alloc( os, size);
	memcpy( dest, src, size );
	return dest;
}

void *
obstack_copy0( os_t *os, const void *src, int size )
{
	void *dest = obstack_alloc( os, size+1 );
	memcpy( dest, src, size );
	((char*)dest)[size] = 0;
	return dest;
}
