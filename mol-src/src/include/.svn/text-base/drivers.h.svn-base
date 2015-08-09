/* 
 *   Creation Date: <2000/12/31 04:29:36 samuel>
 *   Time-stamp: <2004/02/07 18:31:27 samuel>
 *   
 *	<io.h>
 *	
 *	driver interface
 *   
 *   Copyright (C) 2000, 2001, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_DRIVERS
#define _H_DRIVERS

/* from driver_mgr.c */
extern int	driver_mgr_init( void );
extern void 	driver_mgr_cleanup( void );

/* from ioports.c */
extern void 	do_io_read( void *_ior, ulong mphys, int len, ulong *retvalue );
extern void 	do_io_write( void *_ior, ulong mphys, ulong data, int len );

/* from console.c */
extern void	console_make_safe( void );


#endif   /* _H_DRIVERS */
