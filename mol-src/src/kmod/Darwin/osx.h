/* 
 *   Creation Date: <2004/01/23 12:07:53 samuel>
 *   Time-stamp: <2004/03/13 13:56:48 samuel>
 *   
 *	<osx.h>
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

#ifndef _H_OSX
#define _H_OSX

extern int	dev_register( void );
extern int	dev_unregister( void );

/* support.cpp */
extern char	*map_mregs( kernel_vars_t *kv );
extern void	unmap_mregs( kernel_vars_t *kv );
extern ulong	do_get_phys_page( kernel_vars_t *kv, ulong lvptr );

#endif   /* _H_OSX */
