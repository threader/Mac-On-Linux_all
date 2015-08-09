/* 
 *   Creation Date: <2000/10/28 17:38:39 samuel>
 *   Time-stamp: <2004/01/14 21:42:54 samuel>
 *   
 *	<molrcget.c>
 *	
 *	Stand alone interface to the molrc config file
 *   
 *   Copyright (C) 2000, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "res_manager.h"
#include "debugger.h"
#include "version.h"
#include <stdarg.h>

#ifdef __darwin__
#define do_optreset()	do { optreset = 1; } while(0)
#else
#define do_optreset()	do {} while(0)
#endif


int in_security_mode = 1;

static void
print_non_null( char *s )
{
	if( s )
		printf("%s\n", s );
}

int
main( int argc, char **argv  )
{
	int ret=0, n, i, field=-1, index=0, worked=0;
	char *s, *res = "_missing_";

	if( !getuid() )
		in_security_mode = 0;

	for( n=1; n<argc && strcmp("--",argv[n-1]) ; n++ )
		;
	if( n == argc )
		res_manager_init( 0, 1, argv );
	else
		res_manager_init( 0, argc-n+1, &argv[n-1] );

	for( i=1; i<n ; i++ )
		if( argv[i][0] != '-' )
			res = argv[i];

	optind = 1;
	do_optreset();

	for( ;; ) {
		int c;
		if( (c=getopt( n, argv, "AFetvVdpPbnlLahf:i:" )) == -1 )
			break;
		worked++;
		switch(c) {
		case 0:
			break;
		case 'F':
			print_non_null( get_filename_res(res) );
			break;
		case 'b':
			ret = !(get_bool_res(res) == 1);
			break;
		case 'n':
			printf("%ld\n", get_numeric_res(res) );
			break;
		case 'f':
			worked--;
			if( optarg )
				sscanf(optarg, "%d", &field );
			break;
		case 'i':
			worked--;
			if( optarg )
				sscanf(optarg, "%d", &index );
			break;
		case 'a': case 'l':
			/* ignore, this is the default action */
			worked--;
			break;
		case 'A':
			for( i=0; get_str_res_ind(res, i, 0); i++ ) {
				int j;
				for( j=0; (s=get_str_res_ind( res, i, j )); j++ )
					printf("%s ", s );
				printf("\n");
			}
			break;
		case 'p':
		case 'P':
			printf( "%s\n", get_libdir()); 
			break;
		case 'd':
			printf( "%s\n", get_datadir());
			break;
		case 'v':
			printf( "%s\n", get_vardir() );
			break;
		case 'e':
			printf( "%s\n", get_molrc_name() );
			break;
		case 'L':
			printf( "%s\n", get_lockfile());
			break;
		case '?':
		case 'h':
			fprintf(stderr, "Usage: molrcget [-t] [-devpL] [-bnaA] "
				"[-i index] [-f field] [RESOURCE [-- MOLOPTIONS]]\n");
			exit(1);
			break;
		case 'V':
			printf("%s", MOL_VERSION_STR );
			return 0;
		case 't':
			/* test that the molrc files exist */
			return 0;
		default:
			fprintf(stderr, "?? getopt returned unexpected value %d\n", c );
			break;
		}
	}
	if( !worked && field < 0 ) {
		for(i=0; (s=get_str_res_ind(res, index, i)) ;i++ )
			printf( "%s ", s );
		printf("\n");
	} else if( !worked ) {
		s = get_str_res_ind( res, index, field );
		printf("%s\n", s );
	}
	res_manager_cleanup();
	return ret;
}
