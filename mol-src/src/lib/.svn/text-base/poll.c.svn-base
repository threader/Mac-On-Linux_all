/* 
 *   Creation Date: <2001/12/29 16:25:15 samuel>
 *   Time-stamp: <2004/02/07 17:14:17 samuel>
 *   
 *	<poll.c>
 *	
 *	implementation of poll() using select()
 *   
 *   Copyright (C) 2001, 2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/param.h>
#include "poll_compat.h"

#ifdef HAVE_POLL_H
#error "This file should only be compiled if poll() is missing"
#endif

int
poll( struct pollfd *fds, unsigned int nfds, int timeout )
{
	fd_set read_fds, write_fds, except_fds;
	struct timeval timeout_tv;
	int ret, i, n=0;

	timeout_tv.tv_usec = timeout * 1000;
	timeout_tv.tv_sec = 0;

	FD_ZERO( &read_fds );
	FD_ZERO( &write_fds );
	FD_ZERO( &except_fds );

	/* XXX: Is the POLLHUP <-> except_fds matching correct? */
	for( i=0; i<nfds; i++ ){
		n = MAX( fds[i].fd, n );
		fds[i].revents = 0;

		if( (fds[i].events & (POLLIN | POLLPRI)) )
			FD_SET( fds[i].fd, &read_fds );
		if( (fds[i].events & POLLOUT) )
			FD_SET( fds[i].fd, &write_fds );
		if( (fds[i].events & POLLHUP) )
			FD_SET( fds[i].fd, &except_fds );
	}
	if( (n=select(n+1, &read_fds, &write_fds, &except_fds, (timeout<0) ? NULL : &timeout_tv)) < 0 )
		return -1;

	ret = 0;
	for( i=0; n > 0 && i<nfds; i++ ) {
		int found=0;
		if( FD_ISSET(fds[i].fd, &read_fds) ) {
			fds[i].revents |= POLLIN;
			found=1;
			n--;
		}
		if( FD_ISSET(fds[i].fd, &write_fds) ) {
			fds[i].revents |= POLLOUT;
			found=1;
			n--;
		}
		if( FD_ISSET(fds[i].fd, &except_fds) ) {
			fds[i].revents |= POLLHUP;
			found=1;
			n--;
		}
		ret += found;
	}
	return ret;
}

