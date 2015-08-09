/* 
 *   Creation Date: <1999/02/18 22:36:07 samuel>
 *   Time-stamp: <2004/06/12 21:36:35 samuel>
 *   
 *	<timer.c>
 *	
 *	Time and timer handling
 *   
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <signal.h>

#include "timer.h"
#include "thread.h"
#include "mac_registers.h"
#include "molcpu.h"
#include "debugger.h"
#include "promif.h"
#include "res_manager.h"
#include "rvec.h"
#include "cpu/atomic.h"
#include "os_interface.h"

#define TF_PERIODIC		1
#define TF_OVERFLOW		2
#define TF_AUTORESUME		4

#define MAX_NUM_TIMERS		16
#define TIMER_MAX		0x7fffffff

#define DBG_DISABLE_DOZE	0

typedef struct timer_entry {
	uint			tbu;
	uint			tbl;
	int			flags;
	uint			period;

	int			id;
	struct timer_entry 	*next;
	void			*usr;
	timer_func_t 		*handler;
} mtimer_t;

static struct {
	mtimer_t		slots[MAX_NUM_TIMERS];

	mtimer_t		*head;
	mtimer_t		*inactive;
	mtimer_t		*free;

	mtimer_t		*overflow;

	uint			tb_freq;
	uint			ticks_per_usec_num;
	uint			ticks_per_usec_den;

	atomic_t		dozing;
	int			abortpipe[2];
} ts;

int				__recalc_timer;

static pthread_mutex_t 		timer_lock_mutex = PTHREAD_MUTEX_INITIALIZER;

#define LOCK			pthread_mutex_lock( &timer_lock_mutex )
#define UNLOCK			pthread_mutex_unlock( &timer_lock_mutex )

static inline ullong usecs_to_mticks_( ulong usecs ) {
	return (ullong)ts.ticks_per_usec_num * usecs / ts.ticks_per_usec_den;
}

static inline ulong mticks_to_usecs_( ullong mt ) {
	return (mt * ts.ticks_per_usec_den) / ts.ticks_per_usec_num;
}


/************************************************************************/
/*	internal timer functions					*/
/************************************************************************/

static mtimer_t *
dequeue_timer( mtimer_t **list, ulong id ) 
{
	for( ; *list; list=&(**list).next ){
		if( (**list).id == id ) {
			mtimer_t *t = *list;
			*list = t->next;
			return t;
		}
	}
	return NULL;
}

static void
insert_timer( mtimer_t *t )
{
	mtimer_t **tc;
	ullong mark = ((ullong)t->tbu << 32) | t->tbl;
	ullong omark = ((ullong)ts.overflow->tbu <<32) | ts.overflow->tbl;

	if( (llong)(mark - omark) > 0 ) {
		t->next = ts.overflow->next;
		ts.overflow->next = t;
		t->flags |= TF_OVERFLOW;
		return;
	}

	for( tc=&ts.head; *tc && (int)((**tc).tbl - t->tbl) < 0 ; tc=&(*tc)->next )
		;
	t->next = *tc;
	*tc = t;

	if( t == ts.head ) {
		mregs->timer_stamp = t->tbl;
		if( likely(is_main_thread()) )
			mregs->flag_bits |= fb_RecalcDecInt;
		else {
			__recalc_timer = 1;
			interrupt_emulation();
		}
	}
}

static mtimer_t *
new_timer( timer_func_t *handler, void *usr, int flags )
{
	mtimer_t *t;

	if( !(t=ts.free) ) {
		printm("No free timers!\n");
		return NULL;
	}
	ts.free = t->next;

	t->flags = flags;
	t->id += 0x100;
	t->period = 0;
	t->usr = usr;
	t->handler = handler;
	t->next = NULL;
	return t;
}


/************************************************************************/
/*	external timer API						*/
/************************************************************************/

static int
abs_timer( uint tbu, uint tbl, timer_func_t *handler, void *usr )
{
	mtimer_t *t;
	int id=0;

	LOCK;
	if( (t=new_timer(handler, usr, 0)) ) {
		id = t->id;
		t->tbu = tbu;
		t->tbl = tbl;
		insert_timer( t );
	}
	UNLOCK;
	return id;
}

int
tick_timer( uint ticks, timer_func_t *proc, void *usr )
{
	ullong mark = ticks + get_mticks_();
	return abs_timer( mark >> 32, mark, proc, usr );
}

int
usec_timer( uint usecs, timer_func_t *proc, void *usr )
{
	ullong mark = usecs_to_mticks_( usecs ) + get_mticks_();
	return abs_timer( mark >> 32, mark, proc, usr );
}

void
cancel_timer( int id )
{
	mtimer_t *t;

	LOCK;
	if( !(t=dequeue_timer(&ts.head, id)) )
		printm("cancel_timer: no such timer\n");
	else {
		t->next = ts.free;
		ts.free = t;
	}
	UNLOCK;
}


/************************************************************************/
/*      periodical timer API						*/
/************************************************************************/

int
new_ptimer( timer_func_t *handler, void *usr, int auto_resume )
{
	mtimer_t *t;
	int id=0;
	
	LOCK;
	if( (t=new_timer(handler, usr, TF_PERIODIC | (auto_resume? TF_AUTORESUME:0))) ) {
		id = t->id;
		t->next = ts.inactive;
		ts.inactive = t;
	}
	UNLOCK;
	return id;
}

static void
set_ptimer_ticks( mtimer_t *t, int ticks )
{
	ullong mark = get_mticks_() + ticks;
	
	t->tbu = mark >> 32;
	t->tbl = mark;
	t->period = ticks;
	insert_timer( t );
}

void
set_ptimer( int id, uint uperiod )
{
	mtimer_t *t;

	if( uperiod <= 10 ) {
		printm("uperiod too small\n");
		uperiod = 2000;
	}

	LOCK;
	if( !(t=dequeue_timer(&ts.inactive, id)) && !(t=dequeue_timer(&ts.head, id)) )
		printm("bogus timer\n");
	else {
		ullong mark = usecs_to_mticks_( uperiod );
		t->period = mark;
		mark += get_mticks_();

		t->tbu = mark >> 32;
		t->tbl = mark;
		insert_timer( t );
	}
	UNLOCK;
}

static int
recalc_ptimer( mtimer_t *t )
{
	llong mark = (((ullong)t->tbu << 32) | t->tbl);
	llong now = get_mticks_();
	int lost = -1;

	while( now - mark >= 0 ) {
		lost++;
		mark += t->period;
	}
	t->tbu = mark >> 32;
	t->tbl = mark;

	if( lost < 0 ) {
		printm("Warning: unsynchronized timebase detected\n");
		lost = 0;
	}
	return lost;
}

int
resume_ptimer( int id )
{
	mtimer_t *t;
	int lost=0;

	LOCK;
	if( !(t=dequeue_timer(&ts.inactive, id)) )
		printm("resume_ptimer: bogus timer\n");
	else {
		lost = recalc_ptimer( t );
		insert_timer( t );
	}
	UNLOCK;
	return lost;
}

void
pause_ptimer( int id )
{
	mtimer_t *t;

	LOCK;
	if( !(t=dequeue_timer(&ts.head, id)) )
		printm("pause_ptimer: not found\n");
	else {
		t->next = ts.inactive;
		ts.inactive = t;
	}
	UNLOCK;
}

void
free_ptimer( int id )
{
	mtimer_t *t;

	LOCK;
	if( !(t=dequeue_timer(&ts.head, id)) && !(t=dequeue_timer(&ts.inactive, id)) )
		printm("free_ptimer: no such timer\n");
	else {
		t->next = ts.free;
		ts.free = t;
	}
	UNLOCK;
}


/************************************************************************/
/*	debugger support						*/
/************************************************************************/

void
retune_timers( ullong freeze_mticks )
{
	ullong add = get_mticks_() - freeze_mticks;
	mtimer_t *t;

	/* timers were frozen at mticks, recalculate... */
	LOCK;
	for( t=ts.head; t; t=t->next ) {
		ullong mark = (((ullong)t->tbu << 32) | t->tbl) + add;
		t->tbu = mark >> 32;
		t->tbl = mark;
	}
	mregs->dec_stamp += add;
	UNLOCK;
}




/************************************************************************/
/*	timer and doze stuff						*/
/************************************************************************/

static int
rvec_timer( int dummy )
{
	timer_func_t *handler;
	mtimer_t *t;
	void *usr;
	int id, info;

	LOCK;
	while( (t=ts.head) ) {
		info = get_tbl() - t->tbl;

		if( info < 0 || (t->flags & TF_OVERFLOW) )
			break;

		/* printm("Latency: %d\n", info ); */

		handler= t->handler;
		usr = t->usr;
		id = t->id;
		ts.head = t->next;

		if( t->flags & TF_PERIODIC ) {
			if( t->flags & TF_AUTORESUME ) {
				info = recalc_ptimer( t );
				insert_timer( t );
			} else {
				t->next = ts.inactive;
				ts.inactive = t;
			}
		} else {
			t->next = ts.free;
			ts.free = t;
		}
		UNLOCK;
		/* info is either latency or lost ticks */
		(*handler)( id, usr, info );
		LOCK;
	}
	mregs->timer_stamp = t->tbl;
	__recalc_timer = 0;
	UNLOCK;

	/* flag a kernel recalculation of mol-DEC */
	mregs->flag_bits |= fb_RecalcDecInt;
	return 0;
}

void
doze( void )
{
	uint tbl = get_tbl();
	int t, ticks;
	static int cnt;
	
	LOCK;
	ticks = mregs->dec_stamp - tbl;
	t = ts.head->tbl - tbl;
	if( t < ticks )
		ticks = t;
	UNLOCK;

	if( ticks > 0 ) {
		atomic_dec( &ts.dozing );
		if( atomic_read(&ts.dozing) == -1 ) {
			if( (cnt++ & 0xff) == 0xff ) {
				_idle_reclaim_memory();
			} else {
				struct timeval tv;
				fd_set readfs;
				char ch;

				FD_ZERO( &readfs );
				FD_SET( ts.abortpipe[0], &readfs );
				tv.tv_sec = 0;
				tv.tv_usec = mticks_to_usecs_( ticks );
				if( select(ts.abortpipe[0]+1, &readfs, NULL, NULL, &tv) > 0 )
					read( ts.abortpipe[0], &ch, 1 );
			}
		}
		atomic_set( &ts.dozing, DBG_DISABLE_DOZE );
	}
}

void
abort_doze( void )
{
	char ch;
	if( !atomic_inc_and_test_(&ts.dozing) )
		write( ts.abortpipe[1], &ch, 1 );
}

static void
overflow_handler( int id, void *dummy, int lost_ticks )
{
	mtimer_t *t, **tp, *chain = NULL;
	
	LOCK;
	for( tp=&ts.head; (t=*tp) ; ) {
		if( !(t->flags & TF_OVERFLOW) ) {
			tp = &t->next;
		} else {
			*tp = t->next;
			t->next = chain;
			chain = t;
		}
	}
	while( (t=chain) ) {
		t->flags &= ~TF_OVERFLOW;
		chain = t->next;
		insert_timer( t );
	}
	UNLOCK;
}


/************************************************************************/
/*	misc								*/
/************************************************************************/

ulong
get_timebase_frequency( void )
{
	return ts.tb_freq;
}

uint
mticks_to_usecs( ullong mticks )
{
	return mticks_to_usecs_( mticks );
}

static int
osip_mticks_to_usecs( int sel, int *params )
{
	return mticks_to_usecs_( params[0] );
}

static int
osip_usecs_to_mticks( int sel, int *params )
{
	return usecs_to_mticks_( params[0] );
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static ulong
calibrate_timebase( void )
{
	struct timeval tv, tv2;
	ulong a,b, val;
	int i;
	
	printm( "*******************************************************\n"
		"* Measuring the timebase frequency.\n"
		"* This will take about 5 seconds\n"
		"*******************************************************\n");

	/* this calibration is not perfect (but quite good) */
	val = -1;
	for( i=0; i<5; i++ ) {
		gettimeofday( &tv, NULL );
		a = get_tbl();
		for( ;; ) {
			gettimeofday( &tv2, NULL );
			b = get_tbl();
			if( tv2.tv_sec - tv.tv_sec == 1 && tv2.tv_usec - tv.tv_usec > 0 )
				break;
			if( tv2.tv_sec - tv.tv_sec > 1 )
				break;
		}
		if( val > b-a )
			val = b-a;
		printm("    %08lX\n", b-a);
	}
	printm( "*******************************************************\n"
		"* Adding 'timebase_frequency: 0x%08lX' to\n"
		"* /etc/mol/session.map will prevent this delay\n"
		"*******************************************************\n", val );

	for( i=0; i<3; i++ )
		sleep(1);
	return val;
}

void
timer_init( void )
{
	uint val;
	int i;

	if( (val=get_numeric_res("timebase_frequency")) != -1 ) {
		printm("Using timebase frequency %d.%03d MHz [from config file]\n", 
		       val/1000000, (val/1000) % 1000 );
	} else if( !(val=_get_tb_frequency()) ) {
		val = calibrate_timebase();
	}
	ts.tb_freq = val;

	if( DBG_DISABLE_DOZE )
		printm("************ DOZING disabled **************\n");
	
	for( i=1; i<MAX_NUM_TIMERS; i++ )
		ts.slots[i-1].next = &ts.slots[i];
	for( i=0; i<MAX_NUM_TIMERS; i++ )
		ts.slots[i].id = i+1;
	ts.free = &ts.slots[0];

	mregs->timer_stamp = get_tbl() + TIMER_MAX;

	pipe( ts.abortpipe );
	fcntl( ts.abortpipe[0], F_SETFD, FD_CLOEXEC );
	fcntl( ts.abortpipe[1], F_SETFD, FD_CLOEXEC );

	ts.ticks_per_usec_num = ts.tb_freq;
	ts.ticks_per_usec_den = 1000000;

	new_ptimer( overflow_handler, NULL, 1 );
	ts.overflow = ts.inactive;
	set_ptimer_ticks( ts.overflow, TIMER_MAX );

	set_rvector( RVEC_TIMER, rvec_timer, "Timer" );	
	os_interface_add_proc( OSI_MTICKS_TO_USECS, osip_mticks_to_usecs );
	os_interface_add_proc( OSI_USECS_TO_MTICKS, osip_usecs_to_mticks );
}

void
timer_cleanup( void )
{
	ts.head = NULL;
}
