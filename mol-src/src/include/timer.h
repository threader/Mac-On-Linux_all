/* 
 *   Creation Date: <1999/02/18 22:36:58 samuel>
 *   Time-stamp: <2004/06/13 00:48:40 samuel>
 *   
 *	<timer.h>
 *	
 *	Timer handling
 *   
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_TIMER
#define _H_TIMER


/************************************************************************/
/*      mticks util							*/
/************************************************************************/

typedef struct {
	int	hi;
	int	lo;
} timestamp_t;

#include "cpu/timer.h"

extern uint	mticks_to_usecs( ullong mticks );
extern ulong	get_timebase_frequency( void );


/************************************************************************/
/*	initialize / mainloop-interface					*/
/************************************************************************/

extern void	timer_init( void );
extern void	timer_cleanup( void );

/* exports for mainloop.c */

extern void	doze( void );
extern void	abort_doze( void );

extern int	__recalc_timer;
#define	recalc_mol_timer()				\
	if( unlikely(__recalc_timer) ) {		\
		mregs->flag_bits |= fb_RecalcDecInt;	\
		__recalc_timer = 0;			\
	}


/************************************************************************/
/*      timer API							*/
/************************************************************************/

/* Note: A zero timer id means error */

typedef void	(timer_func_t)( int id, void *usr, int info );

/* one-shot timers */
//extern int	abs_timer( uint tbu, uint tbl, timer_func_t *proc, void *usr );
extern int	usec_timer( uint usecs, timer_func_t *proc, void *usr );
extern int	tick_timer( uint ticks, timer_func_t *proc, void *usr );
extern void	cancel_timer( int timer_id );

/* periodic timers */
extern int	new_ptimer( timer_func_t *proc, void *usr, int autoresume );
extern void 	free_ptimer( int id );
extern void	set_ptimer( int id, uint uperiod );
extern int	resume_ptimer( int id );
extern void	pause_ptimer( int id );

/* debugger support */
extern void	retune_timers( ullong freeze_mt );
#define FREEZE_TIMERS	{ ullong __freeze_mt = get_mticks_();
#define RETUNE_TIMERS	retune_timers( __freeze_mt ); } do {} while(0)

#endif   /* _H_TIMER */
