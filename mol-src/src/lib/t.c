/* 
 *   Creation Date: <2001/12/29 00:03:33 samuel>
 *   Time-stamp: <2002/04/30 19:27:30 samuel>
 *   
 *	<t.c>
 *	
 *	OBSTACK TEST
 *   
 *   Copyright (C) 2001, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "min_obstack.h"

struct obstack os;

#define obstack_chunk_alloc	malloc
#define obstack_chunk_free	free

int 
main( int argc, char **argv )
{
	char *s = "Hello world";
	char *s2 = "**Helsssssssssssssssssssssssssssslo world**";
	char *p, *p2;
	
	obstack_init( &os );
	p = (char*)obstack_copy0( &os, s, strlen(s) );
	p2 = (char*)obstack_copy0( &os, s2, strlen(s2) );
	//printf("%s - %s %08lX %08lX\n", p, p2, p, p2 );
	obstack_free( &os, p );
	p = (char*)obstack_copy0( &os, s, strlen(s) );
	p = (char*)obstack_copy0( &os, s, strlen(s) );
	p2 = (char*)obstack_copy0( &os, s2, strlen(s2) );	
	//printf("%s - %s %08lX %08lX\n", p, p2, p, p2 );

	obstack_grow( &os, s, strlen(s) );
	obstack_grow( &os, s, strlen(s) );
	obstack_grow( &os, s, strlen(s) );
	obstack_grow( &os, s, strlen(s) );
	obstack_grow( &os, s, strlen(s) );
	obstack_grow( &os, s, strlen(s) );
	obstack_grow0( &os, s, strlen(s) );
	p = obstack_finish( &os );
	obstack_grow0( &os, s, strlen(s) );
	p = obstack_finish( &os );
	printf("XXXXXX %s XXXXXX\n", p );
	
	obstack_free( &os, NULL );
	return 0;
}

