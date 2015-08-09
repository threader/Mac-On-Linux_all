/* 
 *   Creation Date: <2001/05/06 22:27:09 samuel>
 *   Time-stamp: <2002/10/27 15:49:56 samuel>
 *   
 *	<pseudofs.c>
 *	
 *	Access to linux-side files
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 */

#include "ci.h"
#include "osi_calls.h"
#include "pseudofs_sh.h"


/************************************************************************/
/*	pseudo filesystem (used to publish files from the linux side)	*/
/************************************************************************/

struct pseudo_fd {
	int		fd;
	int		pos;
};

pseudo_fd_t *
pseudo_open( const char *path )
{
	pseudo_fd_t *p;
	while( *path == '/' )
		path++;
	p = malloc( sizeof(pseudo_fd_t) );
	p->pos = 0;

	if( (p->fd=PseudoFSOpen(path)) < 0 ) {
		free( p );
		return NULL;
	}
	return p;
}

void
pseudo_close( pseudo_fd_t *p )
{
	PseudoFSClose( p->fd );
	free( p );
}

int
pseudo_read( pseudo_fd_t *p, char *buf, int count )
{
	return PseudoFSRead( p->fd, p->pos, buf, count );
}

int
pseudo_lseek( pseudo_fd_t *p, off_t offset, int whence )
{
	int size = PseudoFSGetSize(p->fd);
	
	if( whence == SEEK_SET ) {
		p->pos = offset;
	} else if( whence == SEEK_CUR ) {
		p->pos += offset;
	} else {
		p->pos = size + offset;
	}
	if( p->pos < 0 )
		p->pos = 0;
	if( p->pos > size )
		p->pos = size;
	return p->pos;
}

int
pseudo_load_file( const char *path, char *load_addr, int size )
{
	pseudo_fd_t *fd;
	int s;
	
	if( !(fd=pseudo_open(path)) )
		return -1;
	s = pseudo_read( fd, load_addr, size );
	pseudo_close( fd );
	return s;
}




