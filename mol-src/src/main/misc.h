/* 
 *   Creation Date: <2004/01/23 14:18:45 samuel>
 *   Time-stamp: <2004/01/24 22:30:17 samuel>
 *   
 *	<misc.h>
 *	
 *	
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_MISC
#define _H_MISC

/* arch/misc.c */
#ifdef __linux__
extern void	signal_handler( int sig_num, siginfo_t *sinfo,
				struct ucontext *puc, ulong rt_sf );
#else
extern void	signal_handler( int sig_num );
#endif
extern void	load_mods( void );


/* main.c */
extern int	common_signal_handler( int sig_num );


#endif   /* _H_MISC */

