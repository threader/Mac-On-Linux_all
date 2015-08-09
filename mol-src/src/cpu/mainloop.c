/* 
 *   Creation Date: <2001/01/27 14:38:03 samuel>
 *   Time-stamp: <2004/03/07 21:36:03 samuel>
 *   
 *	<mainloop.c>
 *	
 *	mainloop control
 *   
 *   Copyright (C) 2000, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include "mol_assert.h"
#include "rvec.h"
#include "mac_registers.h"
#include "async.h"
#include "timer.h"
#include "res_manager.h"
#include "session.h"
#include "processor.h"
#include "thread.h"
#include "hostirq.h"

#include "molcpu.h"

#ifdef __darwin__
#include "osi_calls.h"
#endif

#ifdef CONFIG_KVM
#include "kvm/kvm.h"
#endif

/* gRVECtable is also accessed by mainloop.S */
priv_rvec_entry_t gRVECtable[NUM_RVECS];

/* exports to mainloop_asm.S */
extern int mainloop_interrupt( int dummy_rvec );


/************************************************************************/
/*	mainloop control						*/
/************************************************************************/

static int __stop;
enum{ kGo=0, kStop=1, kQuit=-1, kSaveSession=-2 };

/* primarily for the debugger */
void
make_mregs_dec_valid( void )
{
	if( __stop == kGo )
		mregs->spr[S_DEC] = mregs->dec_stamp - get_tbl();
}

void
stop_emulation( void )
{
	if( debugger_attached() && __stop != kStop ) {
		mregs->spr[S_DEC] = mregs->dec_stamp - get_tbl();
		__stop = kStop;
		interrupt_emulation();
	}
}
void
save_session( void )
{
	__stop = kSaveSession;
	interrupt_emulation();
}
void 
resume_emulation( void ) 
{
	__stop = kGo;
}
void
quit_emulation( void )
{
	__stop = kQuit;
	interrupt_emulation();
}

static void
print_guard( void )
{
	/* printm is in principal the only FPU-unsafe function we call. 
	 * We use this hook to automatically shield the FPU when necessary.
	 */
	if( is_main_thread() )
		shield_fpu( mregs );
}

void
mainloop_start( void )
{
	int err;
	_spr_changed();
	struct timespec tv;
	tv.tv_sec = 0;
	tv.tv_nsec = 10000000;

	/* must be called once before the mainloop is entered */
	molcpu_mainloop_prep();

	__stop = (get_bool_res("debug_stop") == 1);

	while( __stop != kQuit ) {
		switch( __stop ) {
		case kGo:
			set_print_guard( print_guard );
			molcpu_mainloop();
			set_print_guard( NULL );
			assert( __stop );
			break;

#ifdef CONFIG_DEBUGGER
		case kStop: {
			FREEZE_TIMERS;
			refresh_debugger();
			while( __stop == kStop ) {
				debugger_nub_poll();
				nanosleep(&tv, NULL);
			}
			RETUNE_TIMERS;
			break;
		}
#endif
		case kSaveSession:
			printm("Saving the session...\n");
			if( !(err = save_session_now()) ) {
				printm("The session was saved successfully\n");
				__stop = kQuit;
			} else if( err == 1 ) {
				static int tries=0;
				__stop = kGo;
				if( ++tries < 200 ) {
					schedule_save_session(1000);
				} else {
					printm("Did not reach a state where the session could be saved\n");
					tries=0;
				}
			} else {
				__stop = kGo;
			}
			break;
		}
	}
}


/************************************************************************/
/*	asynchronous event mechanism					*/
/************************************************************************/

static volatile int	__cpu_irq_lower;
static volatile int	__cpu_irq_raise;

static void
set_cpu_irq_private( int raise )
{
#ifdef CONFIG_KVM
	if( raise ) {
		kvm_trigger_irq();
	} else {
		kvm_untrigger_irq();
	}
#else
	if( raise ) {
		mregs->flag_bits |= fb_IRQPending;
		if( mregs->msr & MSR_EE )
			irq_exception();
	} else
		mregs->flag_bits &= ~fb_IRQPending;
#endif
}

void
set_cpu_irq_mt_( int raise ) 
{
	__cpu_irq_raise = 0;
	__cpu_irq_lower = 0;
	set_cpu_irq_private( raise );
}

void
set_cpu_irq_( int raise )
{
	__cpu_irq_raise = raise;
	__cpu_irq_lower = !raise;

	interrupt_emulation();
}


/* If an asynchronous event occurs (e.g. data is available on a
 * file-descriptor) then mregs->interrupt is set to 1. This flags is
 * checked in mainloop_asm.S before virtualization mode is entered
 * and we come here if it is set.
 * 
 * Note that non-SMP mol can _never_ be in virtualization mode
 * when the flag is set since MOL exits virtualization mode at context
 * switch. Thus interrupt latency is minimal. On SMP, an inter processor
 * interrupt must be sent if MOL is in virtualization mode (the virtualized
 * CPU might do something like (1: b 1b)).
 *
 * Possible asynchronous events:
 *
 *	timer recalc
 *	async I/O
 *	cpu IRQ
 */

void 
interrupt_emulation( void )
{
	assert( mregs );
	mregs->interrupt = 1; 

	/* if we are the main thread, then it is pointless to call abort_doze... */
	if( unlikely(!is_main_thread()) ) {
		abort_doze();

		/* On SMP we might have to send an IPI to the main thread. The
		 * latency in the IPI-case is on average about 8.8us (on a 1.25 GHz
		 * SMP machine) and about 1us otherwise.
		 */
		if( mregs->in_virtual_mode )
			_smp_send_ipi();
	}
}

/* The main thread runs this function as a respons to the call of 
 * interrupt_emulation().
 */
int
mainloop_interrupt( int dummy_rvec )
{
	mregs->interrupt = 0;

	if( async_io_is_pending() ) {
		shield_fpu( mregs );
		do_async_io();
	}
	recalc_mol_timer();

	if (mregs->hostirq_update) {
		mregs->hostirq_update = 0;
		hostirq_update_irqs(&(mregs->active_irqs));
	}

	/* the following is a race free solution... */
	if( __cpu_irq_raise ) {
		__cpu_irq_raise = 0;
		set_cpu_irq_private( 1 );
	}
	if( __cpu_irq_lower ) {
		__cpu_irq_lower = 0;
		set_cpu_irq_private( 0 );
	}
	
	/* debugging stuff */
	if( mregs->kernel_dbg_stop ) {
		printm("kernel_dbg_stop: %08lX\n", mregs->kernel_dbg_stop );
		mregs->kernel_dbg_stop = 0;
		__stop = kStop;
	}
	return __stop;
}


/************************************************************************/
/*	misc return vectors						*/
/************************************************************************/

#ifdef __darwin__
extern int do_call_kernel( void );

int
do_call_kernel( void ) 
{
	_call_kernel();
	return mregs->rvec_vector;
}
#endif

static int
rvec_bad_vector( int rvec )
{
	printm("Uninitialized rvector %d occured (%X)\n", rvec & RVEC_MASK, rvec );
	quit_emulation();
	return 0;
}

static int
rvec_debugger( int dummy_rvec, int num )
{
	printm("RVEC-DEBUGGER <%x>\n", num );
	stop_emulation();
	return 0;
}

static int
rvec_out_of_memory( int dummy_rvec, int err )
{
	printm("Unrecoverable out of memory condition. Exiting\n");
	quit_emulation();
	return 0;
}

static int
rvec_internal_error( int dummy_rvec, int err )
{
	if( err == 0x1700 ) {
		printm("==================================================\n"
		       "A problem related to TAU-interrupts has occured.\n"
		       "This is a hardware problem - please turn off the\n"
		       "kernel config option\n"
		       "    Interrupt driven TAU driver (CONFIG_TAU_INT)\n"
		       "\nor\n"
		       "    Thermap Management Support (CONFIG_TAU)\n"
		       "(a kernel recompilation is necessary...)\n"
		       "==================================================\n");
	}
	printm("RVEC Internal Error %x\n", err );
	quit_emulation();
	return 0;
}


/************************************************************************/
/*	debugger							*/
/************************************************************************/

static int __dcmd
cmd_rvecs( int argc, char **argv )
{
#ifdef COLLECT_RVEC_STATISTICS
	priv_rvec_entry_t *p = gRVECtable;
	int i;	

	for( i=0; i<NUM_RVECS; i++, p++ )
		if( p->rvec != rvec_bad_vector )
			printm("RVEC %02d %-26s: %d\n", i, p->name ? p->name : "----", p->dbg_count );
#else
	printm("Statistics is not collected\n");
#endif
	return 0;
}

static int __dcmd
get_perf_entry( int ind, int get_ticks, char *b, int len )
{
	perf_ctr_t pc;

	*b = 0;
	if( _get_performance_info(ind, &pc) )
		return -1;

      	if( strstr(pc.name, "_ticks") ) {
		ulong t = mticks_to_usecs( (ullong)pc.ctr );
		if( !get_ticks )
			return 1;
		snprintf(b,len,"%-30s: %5ld.%03ld ms", pc.name, t/1000, t%1000 );
	} else {
		if( get_ticks )
			return 1;
		snprintf(b,len,"%-30s: %9d", pc.name, pc.ctr );
	}
	return 0;
}

static void __dcmd
print_one_line( int get_ticks )
{
	char b1[128], b2[128];
	int ret, i=0;
	do {
		while( (ret=get_perf_entry(i++,get_ticks,b1,sizeof(b1))) > 0 )
			;
		if( ret < 0 )
			break;
		while( (ret=get_perf_entry(i++,get_ticks,b2,sizeof(b2))) > 0 )
			;
		printm("  %-47s %-47s\n", b1, b2 );
	} while( ret >= 0 );
}

static int __dcmd
cmd_ks( int argc, char **argv )
{
	int i,j;
	for( j=0; j<=2; j++ ) {
		for( i=0; i<96; i++ )
			printm("-");
		printm("\n");
		if( j<2 )
			print_one_line( j );
	}
	return 0;
}

static int __dcmd
cmd_ksc( int argc, char **argv )
{
#ifdef COLLECT_RVEC_STATISTICS
	int i;	
	for( i=0; i<NUM_RVECS; i++ )
		gRVECtable[i].dbg_count = 0;
#endif
	_clear_performance_info();
	return 0;
}

static dbg_cmd_t dbg_cmds[] = {
#ifdef CONFIG_DEBUGGER
	{ "rvecs",	cmd_rvecs,	"rvecs \nShow RVEC statistics\n"		},
	{ "ks",		cmd_ks,		"ks \nDisplay kernel performance statistics\n"	},
	{ "ksc",	cmd_ksc,	"ks \nClear kernel statistics\n"		},
#endif
};

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static rvec_entry_t rvecs[] = {
	{ RVEC_OUT_OF_MEMORY,	rvec_out_of_memory,	"Out of Memory"		},
	{ RVEC_INTERNAL_ERROR,	rvec_internal_error,	"Internal Error"	},
	{ RVEC_DEBUGGER,	rvec_debugger, 		"Debugger"		},
};

void
set_rvector( uint vnum, void *vector, const char *name )
{
	assert( vnum < NUM_RVECS && !gRVECtable[vnum] );

	gRVECtable[vnum].rvec = vector;
	gRVECtable[vnum].name = name;
}

void
set_rvecs( rvec_entry_t *re, int tablesize )
{
	int i;
	for( i=0; i<tablesize/sizeof(re[0]); i++ )
		set_rvector( re[i].vnum, re[i].vector, re[i].name );
}

void
mainloop_init( void )
{
	int i;

	CLEAR( gRVECtable );
	for( i=0; i<NUM_RVECS; i++ )
		set_rvector( i, rvec_bad_vector, NULL );

	set_rvecs( rvecs, sizeof(rvecs) );
	add_dbg_cmds( dbg_cmds, sizeof(dbg_cmds) );
}

void
mainloop_cleanup( void )
{
	// print_rvec_stats(1);
}
