/* 
 *   Creation Date: <2001/01/31 21:02:05 samuel>
 *   Time-stamp: <2003/08/15 19:43:31 samuel>
 *   
 *	<molcpu.h>
 *	
 *	
 *   
 *   Copyright (C) 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_MOLCPU
#define _H_MOLCPU

#include "wrapper.h"

extern ulong g_session_magic;


/************************************************************************/
/*	architecture interface	 					*/
/************************************************************************/

extern int 	open_session( void );
extern void	close_session( void );
extern void	molcpu_mainloop_prep( void );

extern void	molcpu_arch_init( void );
extern void	molcpu_arch_cleanup( void );
extern void	molcpu_mainloop( void );

extern ulong	get_cpu_frequency( void );
extern ulong	get_bus_frequency( void );


/************************************************************************/
/*	exports from mainloop.c and molcpu.c  				*/
/************************************************************************/

/* init / cleanup */
extern void	mainloop_init( void );
extern void	mainloop_cleanup( void );
extern void	mainloop_start( void );

extern void	molcpu_init( void );
extern void	molcpu_cleanup( void );

/* emulation support, main thread only... */
extern void	irq_exception( void );
#define msr_modified()		mregs->flag_bits |= fb_MsrModified;


/* interrupt_emulation is used to do things from the main thread */
extern void	interrupt_emulation( void );
extern void	set_cpu_irq_mt_( int raise );
extern void	set_cpu_irq_( int raise );

static inline void set_cpu_irq_mt( int raise, int *local ) {
	if( *local != raise ) {
		*local = raise;
		set_cpu_irq_mt_( raise );
	}
}
static inline void set_cpu_irq( int raise, int *local ) {
	if( *local != raise ) {
		*local = raise;
		set_cpu_irq_( raise );
	}
}

/* flow control, mostly for the debugger */
extern void	stop_emulation( void );
extern void	resume_emulation( void );
extern void	quit_emulation( void );
extern void	save_session( void );

/* debugger support */
extern void	set_break_flag( int flag );
extern void	clear_break_flag( int flag );
extern void	make_mregs_dec_valid( void );

/* mainloop_asm exports */
extern struct mac_regs *mregs;
extern void	shield_fpu( struct mac_regs *mregs );
extern void	save_fpu_completely( struct mac_regs *mregs );
extern void	reload_tophalf_fpu( struct mac_regs *mregs );


static inline int is_a_601( void ) {
	return (_get_pvr() >> 16) == 1;
}

#endif   /* _H_CPU_CONTROL */
