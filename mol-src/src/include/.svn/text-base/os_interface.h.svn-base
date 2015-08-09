/* 
 *   Creation Date: <2003/04/04 16:24:12 samuel>
 *   Time-stamp: <2003/04/04 16:25:59 samuel>
 *   
 *	<os_interface.h>
 *	
 *	
 *   
 *   Copyright (C) 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License v2.
 */

#ifndef _H_OS_INTERFACE
#define _H_OS_INTERFACE

#include "osi.h"

extern void	os_interface_init( void );
extern void	os_interface_cleanup( void );

typedef int	(*osi_proc)( int selector, int *params );

typedef struct {
	int		selector;
	osi_proc	proc;
} osi_iface_t;

extern int	register_osi_iface( const osi_iface_t *iface, int size );
extern void	unregister_osi_iface( const osi_iface_t *iface, int size );

extern int	os_interface_add_proc( int selector, osi_proc proc );
extern void	os_interface_remove_proc( int sel );

#endif   /* _H_OS_INTERFACE */
