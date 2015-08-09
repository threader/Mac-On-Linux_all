/* 
 *   Creation Date: <2000/07/25 00:22:18 samuel>
 *   Time-stamp: <2004/02/22 17:27:11 samuel>
 *   
 *	<async.c>
 *	
 *	Asynchronous I/O support
 *   
 *   Copyright (C) 2000, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <signal.h>
#ifdef __linux__
#include <linux/version.h>
#endif
#include "poll_compat.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <sched.h>
#include "async.h"
#include "debugger.h"
#include "thread.h"
#include "timer.h"
#include "molcpu.h"

#define MAX_NUM_HANDLERS	16
#define MAX_NUM_AEVENTS		8

typedef struct {
	int		handler_id;
	async_handler_t	proc;
} handler_t;

typedef struct {
	int		 token;
	aevent_handler_t proc;
} aevent_t;

volatile int	 	__io_is_pending;

static struct {
	handler_t 	handlers[ MAX_NUM_HANDLERS ];
	struct pollfd	ufds[ MAX_NUM_HANDLERS ];
	int 		nhandlers;
	int		npoll;
	int		npending;
	int 		next_handler_id;
	int    		handlers_modified;

	aevent_t	aevents[ MAX_NUM_AEVENTS ];
	int		num_aevents;
	int		next_aevent_token;

	volatile int	cancel_thread;

	int	 	pipefds[2];
	int	 	ackpipe[2];

	int		initstate;
} as;


/* only to be called from the main thread */	
static void
wait_safe( void )
{
	if( as.initstate == 2 && !__io_is_pending ) {
		send_aevent(0);
		while( !__io_is_pending )
			sched_yield();
	}
}


/* WARNING: handler pointers are invalid after this call! */
static void
permute_inactive( void )
{
	struct pollfd tmp_p;
	handler_t tmp_h;
	int i, j;

	for( i=0; i<as.nhandlers; i++ ) {
		if( as.ufds[i].events )
			continue;
		tmp_p = as.ufds[i];
		tmp_h = as.handlers[i];

		for( j=i+1; j<as.nhandlers; j++ ) {
			if( as.ufds[j].events ) {
				as.ufds[i] = as.ufds[j];
				as.handlers[i] = as.handlers[j];
				as.ufds[j] = tmp_p;
				as.handlers[j] = tmp_h;
				break;
			}
		}
	}

	as.npoll = 0;
	for( i=0; i<as.nhandlers && as.ufds[i].events; i++ )
		as.npoll++;

	as.handlers_modified = 1;
}

int
add_async_handler( int fd, int events, async_handler_t proc, int sigio_capable )
{
	handler_t *h;
	int id, flags;

	if( as.nhandlers >= MAX_NUM_HANDLERS || !proc ) {
		printm("async error: %s\n", proc ? "MAX_NUM_HANDLERS exceeded\n" : "NULL proc" );
		return -1;
	}

	wait_safe();

	/* doesn't hurt to make sure it is close-on-exec */
	fcntl( fd, F_SETFD, FD_CLOEXEC );

	/* set O_NONBLOCK */
	if( (flags=fcntl(fd, F_GETFL)) == -1 )
		return -1;
	flags |= O_NONBLOCK;
	if( fcntl(fd, F_SETFL, flags) == -1 )
		return -1;

	/* add to table */
	as.ufds[as.nhandlers].fd = fd;
	as.ufds[as.nhandlers].events = events;
	as.ufds[as.nhandlers].revents = 0;
	h = &as.handlers[as.nhandlers];
	id = h->handler_id = as.next_handler_id++;
	h->proc = proc;
	as.nhandlers++;

	/* h is invalid after this call! */
	permute_inactive();
	return id;
}

void
delete_async_handler( int handler_id )
{
	int i;

	wait_safe();

	for( i=0; i<as.nhandlers; i++ )
		if( as.handlers[i].handler_id == handler_id ) {
			memmove( &as.handlers[i], &as.handlers[i+1],
				 (as.nhandlers-i-1)*sizeof(as.handlers[0]) );
			memmove( &as.ufds[i], &as.ufds[i+1],
				 (as.nhandlers-i-1)*sizeof(as.ufds[0]) );
			break;
		}
	if( i == as.nhandlers ) {
		printm("delete_async_handler: bad id (%d)!\n", handler_id );
		return;
	}
	as.nhandlers--;
	permute_inactive();
}

void
set_async_handler_events( int id, int new_events )
{
	int i;

	wait_safe();

	for( i=0; i<as.nhandlers; i++ )
		if( as.handlers[i].handler_id == id ) {
			as.ufds[i].events = new_events;
			break;
		}

	if( i >= as.nhandlers )
		printm("set_async_handler_events: Handler not found\n");

	permute_inactive();
}

static void
poll_thread_entry( void *dummy )
{
	char ack_char;

	/* Increase priority to reduce latency */
	/* setpriority( PRIO_PROCESS, getpid(), -17 ); */

	while( !as.cancel_thread ) {
		if( (as.npending=poll(as.ufds, as.npoll, -1)) < 0 ) {
			if( errno != EINTR )
				perrorm("poll");
			continue;
		}
		__io_is_pending = 1;
		interrupt_emulation();

		/* wait for ack */
		while( read(as.ackpipe[0], &ack_char, 1) != 1 && errno == EINTR )
			;
	}
	as.cancel_thread = 0;
}

void
__do_async_io( void )
{
	const char ackchar=1;
	int i;

	for( i=0; i<as.npoll && as.npending>0; i++ ) {
		int ev = as.ufds[i].revents & as.ufds[i].events;
		if( ev ) {
			as.npending--;
			as.ufds[i].revents = 0;
			as.handlers_modified = 0;

			(*as.handlers[i].proc)( as.ufds[i].fd, ev );

			/* the handler might change our table */
			if( as.handlers_modified ) {
				as.handlers_modified = 0;
				i=0;
			}
		}
	}
	__io_is_pending = 0;

	/* restart polling thread */
	while( write(as.ackpipe[1], &ackchar, 1) != 1 && errno == EINTR )
		;
}


/************************************************************************/
/*	Asynchronous event mechanism					*/
/************************************************************************/

static void 
aevent_rx( int fd, int dummy_events )
{
	char ev;
	int i;

	if( read(as.pipefds[0], &ev, 1) != 1 )
		return;
	if( !ev )
		return;
	
	for( i=0; i<as.num_aevents; i++ ) {
		if( ev == as.aevents[i].token ) {
			(*as.aevents[i].proc)( (int)ev );
			return;
		}
	}
	printm("Aevent %d unhandled!\n", ev );
}

void
send_aevent( int token )
{
	char ev = token;

	if( TEMP_FAILURE_RETRY( write(as.pipefds[1], &ev, 1) ) != 1 )
		perrorm("send_event");
}

int
add_aevent_handler( aevent_handler_t proc )
{
	aevent_t *aev = &as.aevents[as.num_aevents];

	if( as.num_aevents >= MAX_NUM_AEVENTS ) {
		printm("MAX_NUM_AEVENTS exceeded\n");
		return -1;
	}
	aev->token = as.next_aevent_token++;
	aev->proc = proc;

	if( aev->token > 255 ) {
		printm("run out of aevent tokens!\n");
		return -1;
	}
	as.num_aevents++;
	return aev->token;
}

void
delete_aevent_handler( int token )
{
	aevent_t *aev;
	int i;

	for( aev=as.aevents, i=0; i<as.num_aevents; aev++, i++ ) {
		if( aev->token == token ) {
			memmove( aev, aev+1, (as.num_aevents-i-1)*sizeof(aevent_t) );
			break;
		}
	}
}

void
start_async_thread( void )
{
	as.initstate = 2;
	create_thread( poll_thread_entry, NULL, "async-io thread");	
}

void
async_init( void )
{
	as.next_handler_id = 1;

	if( pipe(as.pipefds) < 0 || pipe(as.ackpipe) < 0 ) {
		perrorm("pipe");
		exit(1);
	}
	fcntl( as.pipefds[0], F_SETFD, FD_CLOEXEC );
	fcntl( as.pipefds[1], F_SETFD, FD_CLOEXEC );
	fcntl( as.ackpipe[0], F_SETFD, FD_CLOEXEC );
	fcntl( as.ackpipe[1], F_SETFD, FD_CLOEXEC );

	as.next_aevent_token = 1;	/* 0 is used by us */

	add_async_handler( as.pipefds[0], POLLIN, aevent_rx, 0 );

	as.initstate = 1;
}

void
async_cleanup( void )
{
	const char dummy_ch = 0;
	struct timespec tv;
	tv.tv_sec = 0;
	tv.tv_nsec = 1000;

	if( !as.initstate )
		return;
	
	/* make sure thread is not spinning in the poll loop */
	if( as.initstate > 1 ) {
		wait_safe();

		as.cancel_thread = 1;
		while ( write( as.ackpipe[1], &dummy_ch, 1 ) != 1 )
			;;

		while( as.cancel_thread )
			nanosleep(&tv, NULL);
	}

	close( as.pipefds[0] );
	close( as.pipefds[1] );
	close( as.ackpipe[0] );
	close( as.ackpipe[1] );

	as.initstate = 0;
}

