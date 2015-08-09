/**************************************************************
*   
*   Creation Date: <97/06/26 21:19:54 samuel>
*   Time-stamp: <2000/09/17 16:13:21 samuel>
*   
*	<cmdline.h>
*	
*	Maintains the debugger commandline
*   
*   Copyright (C) 1997, 1999, 2000 Samuel Rydh
*
*   This program is free software; you can redistribute it and/or
*   modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation;
*
**************************************************************/

#ifndef _CMDLINE_H
#define _CMDLINE_H

#include "debugger.h"

extern void	cmdline_init( void );
extern void	cmdline_cleanup( void );

extern void	cmdline_draw( void );
extern void	cmdline_do_key( int key );
extern void	cmdline_setcursor( void );
extern int 	yn_question( char *promt, int defanswer );

extern void	add_remote_cmd( char *cmdname, char *help, int numargs );

#endif
