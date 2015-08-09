/* 
 *   Creation Date: <2003/05/31 22:13:56 samuel>
 *   Time-stamp: <2003/11/27 12:57:17 samuel>
 *   
 *	<main.c>
 *	
 *	OpenFirmware device tree dumper
 *   
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2
 *   
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>

#define MAX_PROP_SIZE	0x100000

static int depth=0;
static char *propbuf;

static char *drop_properties[] = {		/* properties to drop on export */
	"nvramrc",
	"driver-ist",				/* make sure no MacOS created properties */
	"driver-pointer",			/* properties comes through (BootX...) */
	"driver-ref",
	"driver-descriptor",
	"driver-ptr",
	"did",	/* ? */
	"driver,AAPL,MacOS,PowerPC",
	NULL
};


static char *
get_nodename( void )
{
	static char buf[4096];
	FILE *f = fopen("name", "ro");
	char *ret ;
	
	if( !f )
		return NULL;

	ret = fgets( buf, sizeof(buf), f );
	fclose( f );
	return ret;
}

static void
indent( void )
{
	int i;
	for( i=0; i<depth*4; i++ )
		printf(" ");
}

static void
print_property( const char *prop, int pass )
{
	unsigned char *buf = propbuf;
	int i, s, fd;
	
	if( (fd=open(prop, O_RDONLY)) < 0 ) {
		perror("open");
		exit(1);
	}
	if( (s=read(fd, buf, MAX_PROP_SIZE)) < 0 ) {
		perror("read");
		exit(1);
	}
	close( fd );

	/* drop certain (generated) properties */
	for( i=0; drop_properties[i]; i++ )
		if( !strcmp(drop_properties[i], prop) )
			return;

	/* special tag for the phandle */
	if( !strcmp("linux,phandle", prop) ) {
		if( !pass ) {
			indent(); printf("(mol_phandle 0x%08x )\n", *(int*)buf );
		}
		return;
	}
	if( !pass )
		return;
	
	indent(); printf("(property %-24s ", prop );
	depth++;
	
	/* empty property */
	if( !s ) {
		printf(")\n");
		return;
	}
	
	/* empty string */
	if( s==1 && !buf[0] ) {
		printf("<str>   \"\"\n");
		return;
	}

	/* detect likely strings/string arrays */
	for( i=0; i<s ; i++ ) {
		if( !buf[i] && i && !buf[i-1] )
			break;
		if( buf[i] && !isgraph(buf[i]) && !isspace(buf[i]) )
			break;
	}
	if( i == s && !buf[s-1] ) {
		printf("<str>   ");
		if( s > 50 ) {
			printf("\n");
			indent();
		}
		for( i=0; i<s; i += strlen(&buf[i]) + 1 )
			printf("%s\"%s\"", i ? ", " : "", &buf[i] );
		printf(" )\n" );
		return;
	}
	
	/* int property */
	if( !(s % 4) ) {
		int *data = (int*)buf;
		int perline = 4;

		printf("<word>  ");
		if( !strcmp("reg", prop) ) 
			perline = ( !((s/4) % 5) && !buf[3] && !*(int*)&buf[4] ) ? 5 : 2;
		else if( !strcmp("assigned-addresses", prop) && !(s%(5*4)) )
			perline = 5;
		else if( !strcmp("ranges", prop) )
			perline = (!(s%(6*4))) ? 6 : (!(s%(5*4))) ? 5 : 4;

		for( i=0 ; i<s/4 ; i++ ) {
			if( !(i % perline) && s > 8 ) {
				printf("\n");
				indent();
			}
			printf("0x%08x ", data[i] );
		}
		printf(")\n");
		return;
	}

	/* byte property */
	printf("<byte>  ");
	for( i=0 ; s ; s--, i++ ) {
		if( !(i % 16) && s > 5 ) {
			printf("\n");
			indent();
		}
		printf("0x%02x ", buf[i] );
	}
	printf(")\n");
}

static void
dump_node( const char *nodedir, const char *parentpath )
{
	char *fullpath, *name;
	int savedepth, pass;
	struct dirent *de;
	struct stat st;
	DIR *d;
	
	if( !parentpath ) {
		fullpath = strdup( "/" );
	} else {
		fullpath = malloc( strlen(parentpath) + strlen(nodedir) + 2);
		strcpy( fullpath, parentpath );
		if( strcmp(parentpath, "/") )
			strcat( fullpath, "/" );
		strcat( fullpath, nodedir );
	}
	
	if( !(d=opendir(nodedir)) || chdir(nodedir) < 0 ) {
		fprintf(stderr, "%s is missing!\n", nodedir);
		exit(1);
	}

	name = get_nodename();
	printf("\n");
	indent(); printf("#\n");
	indent(); printf("# *************** %s *************\n", name );
	indent(); printf("# %s\n", fullpath );
	indent(); printf("{\n");
	depth++;

	/* properties */
	for( pass=0; pass<2; pass++ ) {
		while( (de=readdir(d)) ) {
			if( lstat(de->d_name, &st) < 0 ) {
				perror("lstat");
				continue;
			}
			if( !S_ISREG(st.st_mode) )
				continue;
			
			savedepth = depth;
			print_property( de->d_name, pass );
			depth = savedepth;
		}
		rewinddir(d);
	}


	/* children */
	while( (de=readdir(d)) ) {
		if( lstat(de->d_name, &st) < 0 ) {
			perror("lstat");
			continue;
		}
		if( !S_ISDIR(st.st_mode) || !strcmp(".",de->d_name) || !strcmp("..", de->d_name) )
			continue;

		depth++;
		dump_node( de->d_name, fullpath );
		depth--;
	}
	depth--;
	indent(); printf("}\n");

	chdir("../");
	free( fullpath );
}

int
main( int argc, char **argv )
{
	propbuf = malloc( MAX_PROP_SIZE );
	dump_node("/proc/device-tree", NULL );
	return 0;
}
