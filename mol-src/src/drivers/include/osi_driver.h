/* 
 *   Creation Date: <2000/02/06 23:55:05 samuel>
 *   Time-stamp: <2003/04/28 20:09:20 samuel>
 *   
 *	<osi_driver.h>
 *	
 *	
 *   
 *   Copyright (C) 2000, 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_OSI_DRIVER
#define _H_OSI_DRIVER

typedef struct osi_driver osi_driver_t;
struct pci_dev_info;

extern osi_driver_t	*register_osi_driver( char *drv_name, char *pnode_name, 
					      struct pci_dev_info *pci_config, int *irq );

extern void 		free_osi_driver( osi_driver_t *dr );
extern int		get_driver_pci_addr( osi_driver_t *dr );

extern osi_driver_t 	*get_osi_driver_from_id( ulong osi_id );

extern void		oldworld_route_irq( int new_irq, int *irq, const char *verbose_name );

extern void		set_driver_ok_flag( int drv_flag, int is_ok );
/* in disk/ablk.c */
extern void		mount_driver_volume( void );

enum {
	kOSXEnetDriverOK	= 1,
	kOSXAudioDriverOK	= 2,
};

#endif   /* _H_OSI_DRIVER */
