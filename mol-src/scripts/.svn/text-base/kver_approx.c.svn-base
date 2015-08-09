/* 
 *   Creation Date: <2001/08/06 00:27:12 samuel>
 *   Time-stamp: <2002/07/02 19:04:26 samuel>
 *   
 *	<kver_approx.c>
 *	
 *	
 *   
 *   Copyright (C) 2001, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* version where binary-breaking stuff were added */
static char *separators[] = {
	"2.2.17-pre3",		/* AltiVec introduced */
	"2.2.18-pre18",		/* task_struct change */
	"2.2.19-pre7",		/* task_struct change */
	"2.2.19-pre13",		/* task_struct change */
	"2.3.0", 		/* --- */
	"2.4.0", 		/* --- */
	"2.4.4",		/* slow-motion behavior */
	"2.4.5-pre5",		/* mm->mmap_sem change */
	"2.4.7-pre6",		/* new linux MM */
	"2.4.8-pre2",		/* necessary for sheep_net */
	"2.4.17",		/* --- */	
	"2.5.0", 		/* --- */
};

typedef struct {
	int	version;
	int	smp;
	int	noaltivec;
	char	*vstr;

	int	was_first;
	int	is_separator;
} kvers_t;

kvers_t *vlist;

static int
kvers_compare( kvers_t *a, kvers_t *b )
{
	int ret = a->version - b->version;
	if( !ret && b->is_separator )
		ret = -1;
	return ret;
}

static int
parse_version( char *s, int *vers )
{
	int a,b,c,d,n;
	
	n = sscanf( s, "%d.%d.%d-pre%d", &a, &b, &c, &d );
	if( n<3 ) {
		fprintf( stderr, "Bad kernel version string '%s'\n", s );
		return 1;
	}
	*vers = (a << 24) | (b<<16) | (c<<8);
	*vers |= ( n==4 )? d : 255;
	return 0;
}


int 
main( int argc, char **argv ) 
{
	int i, n, nsep;
	int target=0, approx;
	kvers_t *v;
	
	if( argc <= 2 ) {
		fprintf( stderr, "Usage: kver_approx this_uname uname1 [uname2 ...]\n");
		return 1;
	}
	argc--;
	nsep = sizeof(separators)/sizeof(char*);
	vlist = calloc( argc+nsep, sizeof(kvers_t) );
	
	n=0;
	for( i=0, v=vlist; i<argc; i++ ) {
		v->vstr = argv[i+1];
		if( parse_version( v->vstr, &v->version ) )
			continue;
		v->smp = ( !strstr(v->vstr, "smp") && !strstr(v->vstr,"SMP") )? 0:1;
		v->noaltivec = !strstr(v->vstr, "noav");
		v++;
		n++;
	}
	vlist[0].was_first = 1;

	for( i=0; i<nsep; i++, n++, v++ ) {
		v->vstr = separators[i];
		parse_version( v->vstr, &v->version );
		v->is_separator = 1;
	}

	/* exact match? */
	for(i=1; i<argc; i++ ) {
		v = &vlist[i];
		if( !strcmp(vlist[0].vstr, v->vstr) ) {
			printf( "%s\n", vlist[0].vstr );
			return 0;
		}
	}

	qsort( &vlist[0], n, sizeof(kvers_t), (void*)kvers_compare );

	/* find target */
	for(i=0; i<n; i++ )
		if( vlist[i].was_first )
			target=i;

	/* try a slightly newer kernel version */
	approx = -1;
	for( i=target+1; i<n && approx<0; i++ ) {
		if( vlist[i].is_separator )
			break;
		if( vlist[i].smp != vlist[target].smp )
			continue;
		if( vlist[i].noaltivec != vlist[target].noaltivec )
			continue;
		approx = i;
	}
	for( i=target-1; i>=0 && approx<0 ; i-- )  {
		if( vlist[i].is_separator )
			break;
		if( vlist[i].smp != vlist[target].smp )
			continue;
		if( vlist[i].noaltivec != vlist[target].noaltivec )
			continue;
		approx = i;
	}
	if( approx < 0 )
		return 1;

	printf("%s\n", vlist[approx].vstr );
	return 0;
}
