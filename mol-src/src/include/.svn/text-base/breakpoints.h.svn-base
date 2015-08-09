/* 
 *   Creation Date: <2001/10/28 22:41:06 samuel>
 *   Time-stamp: <2002/07/14 19:50:47 samuel>
 *   
 *	<breakpoints.h>
 *	
 *	Breakpoints
 *   
 *   Copyright (C) 2001, 2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_BREAKPOINTS
#define _H_BREAKPOINTS

#ifdef CONFIG_DEBUGGER
extern void    		breakpoints_init( void );
extern void 		breakpoints_cleanup( void );
extern int		is_stop_breakpoint( ulong mvptr );
extern int		add_breakpoint( ulong addr, int flags, int data );
extern int 		is_breakpoint( ulong mvptr, char *identifier );
extern void		setup_breakpoints( void );
extern void		restore_breakpoints( void );
#else
static inline void    	breakpoints_init( void ) {}
static inline void 	breakpoints_cleanup( void ) {}
static inline int	is_stop_breakpoint( ulong mvptr ) { return 0; }
static inline int 	is_breakpoint( ulong mvptr, char *identifier ) { return 0; }
static inline void	setup_breakpoints( void ) {}
static inline void	restore_breakpoints( void ) {}
#endif

/* flags used by add_breakpoint */
enum {
	/* 0-15 reserved for the implementation */
	br_dec_flag	= 16,		/* decrementer breakpoint */
	br_m68k		= 32,		/* 68k breakpoint */
	br_rfi_flag	= 64,
	br_transl_ea	= 128,		/* ea should be feeded through the MMU */
	br_phys_ea	= 256		/* ea is a mphys address */
};

#define BREAKPOINT_OPCODE	0x01234567	/* 0 is used by MacOS... */

/* The following address might not be correct for all ROM versions.
 * If not, 68k stepping won't work.
 */
#define MACOS_68K_JUMP_TABLE		0x68080000

#endif   /* _H_BREAKPOINTS */
