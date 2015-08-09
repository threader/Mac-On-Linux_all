/* 
 *   Creation Date: <2003/05/21 17:31:50 samuel>
 *   Time-stamp: <2004/01/09 16:33:32 samuel>
 *   
 *	<asfilter.c>
 *	
 *	replaces ';' with newlines. The line numbering
 *	(for error messages) is also fixed.
 *   
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include <stdio.h>
#include <string.h>
#include <ctype.h>

int
main( int argc, char **argv )
{
	char buf[4096], name[1024], *p;
	int v, mline, line;

	line = 1;
	mline = 1;
	strcpy( name, "stdin\n" );
	for( ;; ) {
		if( !fgets(buf, sizeof(buf), stdin) )
			return 0;
		if( strlen(buf) > 4090 ) {
			fprintf(stderr, "string too long");
			return 1;
		}
		/* perform ';' -> '\n' substitution */
		if( buf[0] != '#' ) {
			char *pp = buf;
			for( p=pp; *p ; p++ )
				if( *p == ';' ) {
					*p = 0;
					printf( "%s\n", pp );
					if( line > 0 )
						printf( "# %d %s", line, name );
					pp = p+1;
				}
			printf( "%s", pp );
			line++;
			mline++;
			continue;
		}
		if( !strncmp(buf+1, "line", 4) ) {
			if( sscanf(buf+5, "%d", &v) == 1 ) {
				line += v - mline;
				mline = v;
				if( line > 0 )
					printf("# %d %s", line, name );
			}
			continue;
		}
		if( buf[1] == ' ' ) {
			printf( "%s", buf );
			line++;
			mline++;
			
			if( sscanf(buf+2, "%d", &line) != 1 )
				continue;
			for( p=buf+2 ; isspace(*p) || isdigit(*p); p++ )
				;
			strncpy( name, p, sizeof(name) );
			buf[ sizeof(buf)-1 ] = 0;
		}
	}
}
