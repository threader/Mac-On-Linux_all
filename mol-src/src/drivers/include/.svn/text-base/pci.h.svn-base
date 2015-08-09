/* 
 *   Creation Date: <1999/03/06 13:39:52 samuel>
 *   Time-stamp: <2003/06/10 22:41:04 samuel>
 *   
 *	<pci.h>
 *	
 *	PCI top-level driver (used by bandit, chaos etc)
 *   
 *   Copyright (C) 1999, 2000, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_PCI
#define _H_PCI

#include "promif.h"

#include <sys/types.h>

typedef void (*pci_map_base_fp)( int regnum, ulong base, int add_map, void *usr );

typedef struct pci_dev_info {
	short		vendor_id;
	short		device_id;
	unsigned char	revision;	
	unsigned char	class_prog;
	short		device_class;
} pci_dev_info_t;

typedef struct pci_dev_hooks {
	/* config space read/write hooks. the char * argument points to data to be read/written
	 * and may be overwritten by the hook.
	 */
	void (*read_config)(void *, int, char *);
	void (*write_config)(void *, int, char *);
} pci_dev_hooks_t;

typedef int	pci_addr_t;	/* 00 domain bus devfn */

#define		PCIADDR_FROM_BUS_DEVFN( domain, bus, devfn )	(((domain)<<16) | ((bus) << 8) | (devfn))

extern void 	write_pci_config( pci_addr_t addr, int offs, ulong val, int len );
extern ulong 	read_pci_config( pci_addr_t addr, int offs, int len );

extern int 	add_pci_device( pci_addr_t addr, pci_dev_info_t *dev_info, void *usr );
extern pci_addr_t add_pci_device_from_dn( mol_device_node_t *dn, void *usr );
extern void 	set_pci_dev_usr_info( pci_addr_t addr, void *usr );
extern void	set_pci_dev_hooks(pci_addr_t addr, pci_dev_hooks_t *h);

extern int	use_pci_rom( pci_addr_t addr, char *filename );
extern int 	set_pci_base( pci_addr_t addr, int ind, ulong bmask, pci_map_base_fp proc );

extern pci_addr_t pci_device_loc( mol_device_node_t *dev );

extern void 	pci_assign_addresses( void );

/* pci-bridges.h */
extern int	get_pci_domain( mol_device_node_t *dev );

#endif   /* _H_PCI */
