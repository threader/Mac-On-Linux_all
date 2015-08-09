/* 
 *   Creation Date: <2003/01/16 00:51:02 samuel>
 *   Time-stamp: <2003/02/19 23:02:45 samuel>
 *   
 *	<x11.h>
 *	
 *	Common X11 code
 *   
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_X11
#define _H_X11

#ifdef CONFIG_X11
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>

typedef struct {
	int		connected;			/* true if X11 connection exists */

	Display		*disp;
	Visual		*vis;
	int		screen;
	Window		rootwin;

	int		byte_order;
	int		is_565;				/* 16-bit 565 mode */
	int		xdepth;
	int		bpp;

	XColor		black;
	XColor		white;
	ulong		black_pixel;
	ulong		white_pixel;

	int		full_screen;
} x11_info_t;

extern x11_info_t	x11;

extern void		x11_set_win_title( Window w, char *title );
extern void		x11_key_event( XKeyEvent *ev );

extern void		display_x_dialog( void );
extern void		remove_x_dialog( void );

#else /* HAVE_X11 */
static inline void	display_x_dialog( void ) {}
static inline void	remove_x_dialog( void ) {}
#endif

#endif   /* _H_X11 */
