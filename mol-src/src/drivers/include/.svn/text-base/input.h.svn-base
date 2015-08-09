/* 
 *   Creation Date: <2002/05/20 11:52:32 samuel>
 *   Time-stamp: <2003/02/16 23:49:09 samuel>
 *   
 *	<input.h>
 *	
 *	Mouse
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_INPUT
#define _H_INPUT

extern ullong	get_mouse_moved_stamp( void );

extern void	mouse_move_to( int x, int y, int width, int height );
extern void	mouse_move( int dx, int dy );
extern void	mouse_but_event( int butevent );
extern void	mouse_event( int dx, int dy, int cur_buts );
extern void	mouse_hwsw_curs_switch( void );

extern int	mouse_activate( int activate );

enum { 
	/* WARNING: hardcoded in console.c and xvideo.c */
	kButton1=1, kButton2=2, kButton3=4, kButtonMask = 7
};


#endif   /* _H_INPUT */
