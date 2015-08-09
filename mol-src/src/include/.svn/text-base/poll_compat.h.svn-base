/* 
 *   Creation Date: <2001/12/29 16:32:14 samuel>
 *   Time-stamp: <2001/12/29 16:37:47 samuel>
 *   
 *	<poll.h>
 *	
 *	Provides poll() on platfors with only select() 
 *   
 *   Copyright (C) 2001 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_POLL
#define _H_POLL

#ifdef HAVE_POLL_H
#include <poll.h>
#else

struct pollfd {
	int 	fd;
	short  	events;
	short 	revents;
};

extern int poll( struct pollfd *fds, unsigned int nfsd, int timeout );

#define POLLIN		0x0001
#define POLLPRI		0x0002
#define POLLOUT		0x0004
#define POLLERR		0x0008
#define POLLHUP		0x0010
#define POLLNVAL	0x0020

#endif	 /* HAVE_POLL_H */
#endif   /* _H_POLL */
