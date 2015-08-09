/* 
 *   Creation Date: <2000/09/16 12:29:36 samuel>
 *   Time-stamp: <2003/08/26 15:05:09 samuel>
 *   
 *	<dgram.c>
 *	
 *	MOL datagram utility functions
 *	(and other shared code)
 *   
 *   Copyright (C) 2000, 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include <stddef.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "dgram.h"
#include "wrapper.h"
#include "res_manager.h"

struct dgram_receiver {
	int	sock;
	int	recv_count;
	char	*packet;
	ulong	pheader[2];
};


/************************************************************************/
/*	Dgram receive 							*/
/************************************************************************/

dgram_receiver_t *
create_dgram_receiver( int sock )
{
	dgram_receiver_t *dr = calloc( 1, sizeof(dgram_receiver_t) );
	if( dr )
		dr->sock = sock;
	return dr;
}

void
free_dgram_receiver( dgram_receiver_t *dr )
{
	free( dr );
}

mol_dgram_t *
receive_dgram( dgram_receiver_t *dr, int *err ) 
{
	mol_dgram_t *dg;
	int len;

	if( err )
		*err=0;
	
	/* Reading packet header? */
	while( dr->recv_count < sizeof(dr->pheader) ) {
		if( (len = read( dr->sock, &dr->pheader, sizeof(dr->pheader)-dr->recv_count )) < 0 ) {
			if( errno == EAGAIN )
				continue;
			perrorm("receive_dgram, read");
			goto bail;
		}
		dr->recv_count += len;
		if( dr->recv_count < sizeof(dr->pheader) )
			return NULL;
	}
	
	if( dr->pheader[0] != MOL_DBG_PACKET_MAGIC ) {
		printm("Invalid debugger packet magic\n");
		goto bail;
	}
	len = dr->pheader[1];	/* total size of packet (including the header) */

	if( !dr->packet )
		dr->packet = malloc( MAX(len, MAX_DBG_PKG_SIZE) );

	while( dr->recv_count < len ) {
		char dummybuf[32], *buf;
		int s = len - dr->recv_count;

		/* discard too big packets */
		buf = dr->packet? dr->packet + dr->recv_count : dummybuf;

		if( !buf || len > MAX_DBG_PKG_SIZE ){
			buf = dummybuf;
			s = MAX( sizeof(dummybuf), s );
		}
		
		if( (s=read( dr->sock, buf, s )) < 0 ) {
			if( errno == EAGAIN )
				continue;
			perrorm("receive_dgram[2], read");
			goto bail;
		}
		dr->recv_count += s;
	}


	if( dr->recv_count != len )
		return NULL;

	/* Dgram received */
	if( (dg = (mol_dgram_t*)dr->packet) ) {
		dg->magic = MOL_DBG_PACKET_MAGIC;
		dg->total_len = len;
		dg->next = NULL;
		dg->data = NULL;
		dg->data_size = len - sizeof(mol_dgram_t);
		if( dg->data_size )
			dg->data = (char*)dg + sizeof(mol_dgram_t);
	}
	dr->recv_count=0;
	dr->packet = NULL;
	return dg;

 bail:
	if( err )
		*err = 1;
	return NULL;
}


/************************************************************************/
/*	Dgram transmit 							*/
/************************************************************************/

int
send_dgram_buf4( int fd, int what, const char *buf, int size, int p0, int p1, int p2, int p3 )
{
	mol_dgram_t dg;
	int s, j;
	
	memset( &dg, 0, sizeof(dg) );
	dg.p0 = p0;
	dg.p1 = p1;
	dg.p2 = p2;
	dg.p3 = p3;
	dg.what = what;
	dg.data_size = size;
	dg.magic = MOL_DBG_PACKET_MAGIC;
	dg.total_len = sizeof(dg) + size;

	if( !buf && size ){
		printm("send_dgram_buf3: internal error\n");
		return 1;
	}
	
	for( j=0; j<2; j++ ) {
		const char *p = (!j)? (char*)&dg : buf;
		s = (!j)? sizeof(dg) : size;

		if( !p )
			break;
		while( s > 0 ) {
			int len;
			if( (len=write(fd, p, s)) < 0 ) {
				if( errno != EAGAIN && errno != EINTR ) {
					/* better not use printm here... */
					fprintf(stderr, "send_dgram: %s\n", strerror(errno));
					return -1;
				}
				continue;
			}
			s -= len;
		}
	}
	return 0;
}


/************************************************************************/
/*	Misc. shared code						*/
/************************************************************************/

char *
molsocket_name( void )
{
	char buf[200];
	snprintf( buf, 200, "%s-%d", MOL_SOCKET_NAME, g_session_id );
	return strdup( buf );
}

