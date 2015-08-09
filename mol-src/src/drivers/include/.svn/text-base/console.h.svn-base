/* 
 *   Creation Date: <97/06/29 23:33:42 samuel>
 *   Time-stamp: <2004/02/07 18:29:33 samuel>
 *   
 *	<console.h>
 *	
 *	Initialization of screen and keyboard
 *   
 *   Copyright (C) 1997, 1999, 2000, 2003, 2004 Samuel Rydh
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_CONSOLE
#define _H_CONSOLE

extern void (*console_switch_hook)(int vt, int activate);

extern int	console_init( void );
extern void	console_cleanup( void );

extern void	console_set_gfx_mode( int flag );

extern int	console_to_front( void );

/* fbdev.c */
#ifdef CONFIG_FBDEV
extern void	fbdev_vt_switch( int activate );
#else
static inline void fbdev_vt_switch( int activate ) {}
#endif

#endif   /* _H_CONSOLE */
