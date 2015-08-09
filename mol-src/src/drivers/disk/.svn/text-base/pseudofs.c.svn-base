/* 
 *   Creation Date: <2002/10/26 19:47:27 samuel>
 *   Time-stamp: <2003/10/22 12:28:10 samuel>
 *   
 *	<pseudofs.c>
 *	
 *	Used to publish files from the linux side
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/stat.h>
#include "pseudofs.h"
#include "os_interface.h"
#include "driver_mgr.h"
#include "memory.h"
#include "res_manager.h"

typedef struct pfile {
	char		*linux_name;
	char		*pseudo_name;
	struct pfile	*next;
} pfile_t;

static pfile_t *proot;


static pfile_t *
find_pseudofile( const char *name )
{
	pfile_t *p;
	for( p=proot; p && strcmp(p->pseudo_name,name); p=p->next )
		;
	return p;
}

int
publish_pseudofile( const char *linux_fname, const char *pseudofile, int should_exist )
{
	pfile_t *p;
	int fd;
	
	/* NOTE: is called before pseudofs_init... */

	if( !linux_fname || !pseudofile )
		return 1;
	if( find_pseudofile(pseudofile) ) {
		printm("pseudofile %s already exists\n", pseudofile );
		return 1;
	}

	if( (fd=open(linux_fname, O_RDONLY)) < 0 ) {
		if( !should_exist && errno == ENOENT )
			return 1;
		perrorm("-----> %s", linux_fname );
		return 1;
	}
	close( fd );
	
	if( !(p=malloc(sizeof(pfile_t))) )
		return 1;
	p->linux_name = strdup(linux_fname);
	p->pseudo_name = strdup(pseudofile);

	p->next = proot;
	proot = p;
	return 0;
}

static int
osip_pseudo_fs( int sel, int *params )
{
	char *s;
	pfile_t *p;
	int fd, ret, ind;
	struct stat st;
	
	switch( params[0] ) {
	case kPseudoFSOpen: /* name */
		if( mphys_to_lvptr(params[1], &s) )
			return -1;
		for( ind=0, p=proot; p && strcmp(p->pseudo_name,s); p=p->next )
			ind++;
		if( !p )
			return -1;
		return ind;

	case kPseudoFSClose: /* fd (index) */
		return 0;

	case kPseudoFSRead: /* fd (index), offs, buf, count */
		for( p=proot; p && params[1]--; p=p->next )
			;
		if( !p || mphys_to_lvptr(params[3], &s) )
			return -1;
		if( (fd=open(p->linux_name, O_RDONLY)) < 0 ) {
			perrorm("pseudofs_open %s", p->linux_name );
			return -1;
		}
		lseek( fd, params[2], SEEK_SET );
		if( (ret=read(fd, s, params[4])) < 0 )
			perrorm("pseudofs_read");
		close( fd );
		return ret;

	case kPseudoFSGetSize: /* fd (index) */
		for( p=proot; p && params[1]--; p=p->next )
			;
		if( !p )
			return -1;
		if( lstat( p->linux_name, &st ) < 0 ) {
			perrorm("pseduofs, lstat");
			return -1;
		}
		return st.st_size;

	case kPseudoFSIndex2Name: /* index, buf, size */
		for( p=proot; p && params[1]--; p=p->next )
			;
		if( !p || mphys_to_lvptr(params[2], &s) )
			return -1;
		strncpy0( s, p->linux_name, params[3] );
		break;

	default:
		return -1;
	}
	return 0;
}

static int
pseudofs_init( void )
{
	char buf[16], buf2[128], nbuf[16], *lname, *s;
	int i,j,index;
	
	for( j=0; j<3; j++ ) {
		sprintf( buf, "%spseudofile%s", (j==2 ? "@" : ""), (j==1 ? "_" : "") );
		for( i=0; (s=get_str_res_ind(buf, i, 0)) ; i++ ) {
			if( !(lname=get_filename_res_ind(buf,i,1)) ) {
				printm("---> malformed pseudofile resource\n");
				continue;
			}
			for( index=0 ; !(j==2 && index); index++) {
				strncpy0( buf2, s, sizeof(buf2) );
				nbuf[0] = 0;
				if( index )
					sprintf( nbuf, "-%d", index );
				strncat0( buf2, nbuf, sizeof(buf2) );
				if( !find_pseudofile(buf2) ) {
					publish_pseudofile( lname, buf2, 1 );
					break;
				}
			}
		}
	}
	os_interface_add_proc( OSI_PSEUDO_FS, osip_pseudo_fs );
	return 1;
}

static void
pseudofs_cleanup( void )
{
	pfile_t *p;

	while( (p=proot) ) {
		proot = p->next;
		free( p->linux_name );
		free( p->pseudo_name );
		free( p );
	}
}

driver_interface_t pseudofs_driver = {
	"pseudofs", pseudofs_init, pseudofs_cleanup
};




