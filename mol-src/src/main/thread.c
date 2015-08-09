/* 
 *   Creation Date: <1999-02-01 04:41:19 samuel>
 *   Time-stamp: <2004/01/24 21:34:55 samuel>
 *   
 *	<thread.c>
 *	
 *	Thread manager
 *   
 *   Copyright (C) 1999, 2000, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <signal.h>
#include <pthread.h>
#include <sched.h>

#include "debugger.h"
#include "thread.h"
#include "mol_assert.h"

/* GLOBALS */
pthread_t __main_th = 0;

struct pool_thread {
	pthread_t		id;
	const char		*name;

	void			(*func)(void*);
	void			*data;

	volatile int		active;

	struct pool_thread	*next;
	struct pool_thread	*all_next;
};

static struct {
	pthread_mutex_t		pool_mutex;

	struct pool_thread	*all;
	struct pool_thread	*start;
	struct pool_thread	*free;

	int			startpipe[2];
	int 			initialized;
} tm;

#define LOCK 			pthread_mutex_lock( &tm.pool_mutex );
#define UNLOCK			pthread_mutex_unlock( &tm.pool_mutex );


static void
set_thread_sigmask( void )
{
	sigset_t set;

	/* Best to block SIGINT & SIGPROF */
        sigemptyset( &set );
        sigaddset( &set, SIGINT );
        sigaddset( &set, SIGPROF );
	sigaddset( &set, SIGIO );
	
	pthread_sigmask( SIG_BLOCK, &set, NULL );
}

static void
thread_exit_func( void *_th )
{
	struct pool_thread *th = _th;
	th->active = 0;
}

static void *
thread_entry( void *p )
{
	pthread_t id = pthread_self();
	struct pool_thread *th;
	char ch;	
	int r;

	set_thread_sigmask();

	for( ;; ) {
		if( (r=read( tm.startpipe[0], &ch, 1 )) != 1 || !ch ) {
			if( r == 0 || (r == -1 && errno == EINTR) )
				continue;
			break;
		}
		LOCK;
		th = tm.start;
		tm.start = th->next;
		th->id = id;
		th->active = 1;
		pthread_cleanup_push( thread_exit_func, th );
		UNLOCK;

		(*th->func)( th->data );
		
		LOCK;
		pthread_cleanup_pop( 1 /*execute*/ );
		th->next = tm.free;
		tm.free = th;
		UNLOCK;
	}
	return NULL;
}

void
create_thread( void (*func)(void*), void *data, const char *name )
{
	struct pool_thread *th;
	pthread_t dummy_id;
	const char ch = 1;
	
	assert( tm.initialized );

	LOCK;
	if( !tm.free ) {
		th = calloc( 1, sizeof(*th) );

		th->all_next = tm.all;
		tm.all = th;
		
		if( pthread_create( &dummy_id, NULL, (void*)thread_entry, (void*)th ) ) {
			UNLOCK;
			fatal("Thread creation failed\n");
		}
	} else {
		th = tm.free;
		tm.free = th->next;
	}
	/* start thread */
	th->data = data;
	th->func = func;
	th->name = name;
	th->next = tm.start;
	tm.start = th;
	UNLOCK;

	write( tm.startpipe[1], &ch, 1 );
}

const char *
get_thread_name( void )
{
	pthread_t self = pthread_self();
	struct pool_thread *th;

	for( th=tm.all; th; th=th->all_next )
		if( th->id == self )
			return th->name ? th->name : "<noname thread>";

	if( self == __main_th )
		return "main-thread";

	return "<unregistered>";
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

void 
threadpool_init( void )
{
	pthread_mutex_init( &tm.pool_mutex, NULL );

	__main_th = pthread_self();
	pipe( tm.startpipe );
	fcntl( tm.startpipe[0], F_SETFD, FD_CLOEXEC );
	fcntl( tm.startpipe[1], F_SETFD, FD_CLOEXEC );

	tm.initialized = 1;
}

void
threadpool_cleanup( void )
{
	struct pool_thread *th, *next;
	const char ch=0;
	int i;
	
	if( !tm.initialized )
		return;

	printm("Terminating threads...\n");
	for( i=0; i<64; i++ ) {
		write( tm.startpipe[1], &ch, 1 );
		sched_yield();
	}
	
	for( th=tm.all; th; th=next ) {
		if( th->active ) {
			printm("thread '%s' is active!\n", th->name ? th->name : "<noname>");
			/* use the big hammer... */
			pthread_kill( th->id, SIGKILL );
		}
		pthread_join( th->id, NULL );

		next = th->all_next;
		free( th );
	}
	close( tm.startpipe[0] );
	close( tm.startpipe[1] );
	tm.initialized = 0;
}


