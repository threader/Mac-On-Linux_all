/* 
 *   Creation Date: <2000/07/25 00:23:51 samuel>
 *   Time-stamp: <2004/02/22 17:06:07 samuel>
 *   
 *	<async.h>
 *	
 *	Support of asynchronous I/O
 *   
 *   Copyright (C) 2000, 2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_ASYNC
#define _H_ASYNC

extern void 	async_init( void );
extern void	start_async_thread( void );
extern void 	async_cleanup( void );

typedef void	(*async_handler_t)( int fd, int events );
typedef void	(*aevent_handler_t)( int token );

/* Only the main-thread may call the add/delete/set routines */ 
extern int 	add_async_handler( int fd, int events, async_handler_t proc, int sigio_capable );
extern void 	delete_async_handler( int handler_id );

/* Note: POLLHUP and POLLERR can not be masked */
extern void 	set_async_handler_events( int handler_id, int new_events );

extern int	add_aevent_handler( aevent_handler_t proc );
extern void	delete_aevent_handler( int token );

/* May be called by any thread */
extern void	send_aevent( int token );


extern volatile int __io_is_pending;

extern void 	__do_async_io( void );

static inline int async_io_is_pending( void ) {
	return __io_is_pending;
}

static inline void do_async_io( void ) {
	if( __io_is_pending )
		__do_async_io();
}


#endif   /* _H_ASYNC */

