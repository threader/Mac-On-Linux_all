/**************************************************************
*   
*   Creation Date: <97/06/18 14:21:41 samuel>
*   Time-stamp: <2001/01/31 23:23:34 samuel>
*   
*	<monitor.h>
*	
*	
*   
*   Copyright (C) 1997, 1999, 2000 Samuel Rydh
*
*   This program is free software; you can redistribute it and/or
*   modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation;
*
**************************************************************/

#ifndef _H_MONITOR
#define _H_MONITOR

#include "debugger.h"

extern void 	monitor_init( void );
extern void 	monitor_cleanup( void );

extern void	install_monitor_cmds( void );

extern void	redraw_inst_win( void );
extern void	refresh_debugger( void );

extern ulong	get_inst_win_addr( void );

extern int	deb_getch( void );

extern int	mon_debugger( void );

extern void	set_nip( ulong new_nip );	/* both 68k / ppc */
extern ulong	get_nip( void );

extern int	is_68k_mode( void );
extern void	set_68k_nip( ulong new_nip );

#endif /* _H_MONITOR */
