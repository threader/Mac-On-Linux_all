/* 
 *   Creation Date: <2000/02/06 22:56:55 samuel>
 *   Time-stamp: <2003/06/02 14:22:18 samuel>
 *   
 *	<osi_driver.c>
 *	
 *	OSI driver 
 *	Handles the Mac-side driver interface.
 *   
 *   Copyright (C) 2000, 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "promif.h"
#include "booter.h"
#include "res_manager.h"
#include "debugger.h"
#include "pci.h"
#include "verbose.h"
#include "os_interface.h"
#include "osi_driver.h"
#include "session.h"
#include "driver_mgr.h"
#include "pic.h"

static struct osi_driver {
	pci_addr_t	pci_addr;
	struct 		osi_driver *next;
} *osi_driver_root;

pci_addr_t
get_driver_pci_addr( struct osi_driver *dr )
{
	return !dr ? -1 : dr->pci_addr;
}

osi_driver_t *
get_osi_driver_from_id( ulong osi_id )
{
	/* osi_id == first word of reg_property */
	pci_addr_t addr = (osi_id >> 8) & 0xffff;
	osi_driver_t *dr;

	for( dr=osi_driver_root; dr; dr=dr->next )
		if( dr->pci_addr == addr )
			break;
	return dr;
}


/************************************************************************/
/*	IRQ utils							*/
/************************************************************************/

static int
lookup_irq( mol_device_node_t *dn )
{
	irq_info_t irqinfo;
	
	/* oldworld IRQs are routed at runtime */
	if( is_oldworld_boot() )
		return -1;

	if( !dn || prom_irq_lookup(dn, &irqinfo) != 1 ) {
		printm("Failed to lookup IRQ\n");
		return -1;
	}
	return irqinfo.irq[0];
}

void
oldworld_route_irq( int new_irq, int *irq, const char *verbose_name )
{
	if( !is_oldworld_boot() || *irq > 0 )
		return;
	*irq = new_irq;
	register_irq( irq );
	printm("Routing %s irq to %d\n", verbose_name, *irq );
}


/************************************************************************/
/*	add newworld driver to device tree				*/
/************************************************************************/

static int
prom_add_nwdriver( mol_device_node_t *dn, char *filename )
{
	int fd, size;
	char *ptr;
	
	if( !dn )
		return 1;
	fd = open( filename, O_RDONLY );
	if( fd < 0 ) {
		perrorm("Could not open driver '%s'", filename );
		return 1;
	}

	size = lseek( fd, 0, SEEK_END);
	if( !(ptr=malloc(size)) ) {
		printm("Out of memrory\n");
		close(fd );
		return 1;
	}
	lseek( fd, 0, SEEK_SET );
	if(read( fd, ptr, size ) == size) 
		prom_add_property( dn, "driver,AAPL,MacOS,PowerPC", ptr, size );

	free( ptr );
	close( fd );

	return 0;
}


/************************************************************************/
/*	oldworld PCI allocation						*/
/************************************************************************/

#ifdef OLDWORLD_SUPPORT
typedef struct {
	int	allocated;
	int	pci_addr;		/* 00 00 bus devfn */
} pci_desc_t;

/* mouse, video, blk */
static pci_desc_t slots[] = {
	{0, (13<<3) | 0},		/* (0:70) slot B1, fn 0 */
	{0, (14<<3) | 0},		/* (0:78) slot C1, fn 0 */
	{0, (15<<3) | 0},		/* (0:68) slot A1, fn 0 */

	{0, (13<<3) | 1},		/* (0:69) slot A1, fn 1 */
	{0, (14<<3) | 1},		/* (0:71) slot B1, fn 1 */
	{0, (15<<3) | 1},		/* (0:79) slot C1, fn 1 */

	{0, (13<<3) | 2},		/* (0:6a) slot A1, fn 2 */
	{0, (14<<3) | 2},		/* (0:72) slot B1, fn 2 */
	{0, (15<<3) | 2},		/* (0:7a) slot C1, fn 2 */

	{0, (13<<3) | 3},		/* (0:6b) slot A1, fn 3 */
	{0, (14<<3) | 3},		/* (0:73) slot B1, fn 3 */
	{0, (15<<3) | 3},		/* (0:7b) slot C1, fn 3 */
	{0, 0}
};

static pci_addr_t
alloc_ow_pci_slot( int need_irq )
{
	int i;

	for( i=0; slots[i].pci_addr; i++ ){
		if( slots[i].allocated )
			continue;
		slots[i].allocated = 1;
		return slots[i].pci_addr;
	}
	printm("----> No free PCI-slots availalble!\n");
	return -1;
}
#endif

/************************************************************************/
/*	register / free osi driver					*/
/************************************************************************/

struct osi_driver*
register_osi_driver( char *drv_name, char *pnode_name, pci_dev_info_t *pci_config, int *irq )
{
	mol_device_node_t *dn;
	osi_driver_t dt, *ret_dt;
	char resname[32], *name=NULL;

	memset( &dt, 0, sizeof(dt) );

	if( drv_name ) {
		strcpy( resname, "osi_" );
		strncat3( resname, drv_name, "_driver", sizeof(resname) );
		name = get_filename_res( resname );
		if( !name && is_classic_boot() ) {
			printm("---> Resource '%s' is missing\n", resname );
		}
	}

#ifdef OLDWORLD_SUPPORT
	/* irq is determined at runtime in the oldworld case */
	if( pci_config && is_oldworld_boot() )
		if( (dt.pci_addr=alloc_ow_pci_slot(1)) < 0 )
			return NULL;
#endif
	if( !is_oldworld_boot() ) {
		if( !(dn=prom_find_devices(pnode_name)) )
			return NULL;

		if( name && is_macos_boot() )
			prom_add_nwdriver( dn, name );

		if( pci_config )
			dt.pci_addr = pci_device_loc( dn );
	}

	if( irq ) {
		*irq = lookup_irq( prom_find_devices(pnode_name) );
		if( !is_oldworld_boot() && register_irq(irq) )
			return NULL;
	}

	/* add PCI card to bus */
	if( pci_config && (is_classic_boot() || is_osx_boot()) ) {
		VPRINT("Adding pci card at (%d:%02X)\n", dt.pci_bus, dt.pci_devfn );
		if( add_pci_device( dt.pci_addr, pci_config, NULL )) {
			printm("Unexpected error: Failed adding pci card!\n");
			return NULL;
		}
	}
	
	/* add oldworld ROM */
	if( is_oldworld_boot() && name && pci_config )
		use_pci_rom( dt.pci_addr, name );

	/* add to linked list */
	ret_dt = (osi_driver_t*)malloc( sizeof(dt) );
	*ret_dt = dt;
	ret_dt->next = osi_driver_root;
	osi_driver_root = ret_dt;

	return ret_dt;
}

void
free_osi_driver( osi_driver_t *d )
{
	osi_driver_t **pp;
	
	for( pp=&osi_driver_root; *pp != d ; pp=&(**pp).next )
		;
	if( *pp ) {
		*pp = (**pp).next;
		free( d );
	} else
		printm("free_osi_driver: error\n");
}

static uint drv_ok_flags;

static int
osip_mount_drv_vol( int sel, int *params )
{
	uint mask = is_osx_boot() ? kOSXEnetDriverOK | kOSXAudioDriverOK : 0;
	
	if( (mask & drv_ok_flags) != mask )
		mount_driver_volume();
	return 0;
}

void
set_driver_ok_flag( int drv_flag, int is_ok )
{
	if( !is_ok )
		mount_driver_volume();
	drv_ok_flags |= drv_flag;
}

static int
osi_init( void )
{
	os_interface_add_proc( OSI_CMOUNT_DRV_VOL, osip_mount_drv_vol );
	return 1;
}

static void
osi_cleanup( void )
{
	while( osi_driver_root )
		free_osi_driver( osi_driver_root );
}

driver_interface_t osi_driver = {
    "osi-driver", osi_init, osi_cleanup
};
