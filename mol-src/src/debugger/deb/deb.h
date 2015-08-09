/* 
 *   Creation Date: <2000/09/16 01:57:04 samuel>
 *   Time-stamp: <2003/06/12 13:58:18 samuel>
 *   
 *	<deb.h>
 *	
 *	
 *   
 *   Copyright (C) 2000, 2001, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_DEB
#define _H_DEB


enum { kInstTrans=0, kDataTrans=1 };

extern ulong 	readc_ea( ulong ea, int context, int use_dtrans );
extern void 	clear_cache( void );
extern void	poll_socket( void );
extern void	set_ppc_mode( int in_ppc_mode );
extern void	refetch_mregs( void );
extern void 	send_mregs_to_mol( void );

extern int	do_remote_dbg_cmd( int argc, char **argv );

extern int	get_inst_context( void );
extern int	get_data_context( void );

extern void	dbg_add_breakpoint( ulong addr, int flags, int data );
extern int	dbg_is_breakpoint( ulong mvptr, char *identifier );

#endif   /* _H_DEB */
