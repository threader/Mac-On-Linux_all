/* 
 *   Creation Date: <97/07/03 13:44:49 samuel>
 *   Time-stamp: <2003/06/10 22:35:17 samuel>
 *   
 *	<promif.h>
 *	
 *	
 *   
 *   Copyright (C) 1997-2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_PROMIF
#define _H_PROMIF

#include "prom.h"

extern void 	promif_init( void );
extern void 	promif_cleanup( void );

/*
 * To device trees are maintained. The hardware tree
 * describes the hardware of the machine running the software.
 * Functions with the 'hw' suffix accesses this tree.
 *
 * The emulation tree describes the hardware of the machine
 * beeing emulated. It is the emulation tree which normally
 * should be used.
 */

/* The automatically generated node identifier (phandle) starts at PHANDLE_BASE.
 * Lower values are reserved for manual modifications of the oftree file.
 */
#define PHANDLE_BASE	0x1000

extern mol_device_node_t *prom_phandle_to_dn( ulong phandle );

extern mol_device_node_t *prom_find_dev_by_path( const char *path );
extern mol_device_node_t *prom_find_devices( const char *name);
extern mol_device_node_t *prom_find_type( const char *type );
extern mol_device_node_t *prom_get_root( void );
extern mol_device_node_t *prom_get_allnext( void );

extern int		 is_ancestor( mol_device_node_t *par, mol_device_node_t *child );

extern mol_device_node_t *prom_create_node( const char *path  );
extern mol_device_node_t *prom_add_node( mol_device_node_t *par, const char *name );
extern mol_device_node_t *prom_import_node( mol_device_node_t *par, const char *filename );
extern int		 prom_delete_node( mol_device_node_t *dn );

extern unsigned char 	 *prom_get_property(mol_device_node_t *dn, const char *name, int *lenp);
extern int		 prom_get_int_property( mol_device_node_t *dn, const char *name, int *retval );
extern unsigned char 	 *prom_get_prop_by_path( const char *path, int *retlen );
extern char		 *prom_next_property( mol_device_node_t *dn, const char *prev_name );
extern void		 prom_add_property( mol_device_node_t *dn, const char *name, const char *data, int len );

extern int		 prom_irq_lookup( mol_device_node_t *dn, irq_info_t *retinfo );

static inline ulong	prom_dn_to_phandle( mol_device_node_t *dn ) {
				return dn ? (ulong)dn->node : 0;
			}
#endif
