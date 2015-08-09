/* 
 *   Creation Date: <1999/03/05 16:14:42 samuel>
 *   Time-stamp: <2003/06/10 23:11:38 samuel>
 *   
 *	<pci-brdiges.c>
 *	
 *	PCI host bridges (bandit, chaos and grackle)
 *   
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "promif.h"
#include "ioports.h"
#include "debugger.h"
#include "pci.h"
#include "debugger.h"
#include "extralib.h"
#include "byteorder.h"
#include "driver_mgr.h"

/* #define DEBUG_ADDR		0x0068	*/


/* this hold for bandit, chaos and uni-north */
#define MACRISC_CFG_ADDR_OFFS	0x800000
#define MACRISC_CFG_DATA_OFFS	0xc00000

/* This is for Grackle (MPC106) in map B (access address is masked by 0xffffc)
 * Motorola suggests using FEC00CF8 and FEE00CFE.
 */
#define GRACKLE_CFG_ADDR	0xFEC00000
#define GRACKLE_CFG_DATA	0xFEE00000

#define MACRISC_DEV_FN		(11<<3)
#define GRACKLE_DEV_FN		0

/* Chaos bridge:
 *   It appears as if OF reads data from offset c00004 whenever 
 *   the config address ends at 0x4 or 0xc, probably due to a 64-bit
 *   bus interface or something (the linux kernel doesn't do this).
 */

typedef struct pci_bridge {
	char 	*bridge_name;
	ulong	phandle;		/* bridge phandle */
	ulong	mbase;			/* register base (0xF8000000) */

	int	domain;			/* we might have independent buses */
	
	int	first_bus;
	int	last_bus;

	ulong	addr_mbase;		/* config register port */
	ulong	data_mbase;		/* config data port */

	int	bridge_type;		/* chaos or bandit */
	void	*type_data;		/* bridge specific data goes here */

	pci_addr_t (*interpret_addr)( struct pci_bridge *b, ulong pciaddr, int *offs );

	struct pci_bridge  *next;

	/* emulated hardware registers */
	ulong	addr;			/* address register */
} pci_bridge_t;

static pci_bridge_t *all_bridges;

enum {
	bandit_bridge, chaos_bridge, grackle_bridge, uninorth_bridge
};

int
get_pci_domain( mol_device_node_t *the_dn ) 
{
	pci_bridge_t *b = all_bridges;
	mol_device_node_t *dn;
	
	for( ; b ; b=b->next )
		for( dn=the_dn; dn ; dn=dn->parent )
			if( prom_dn_to_phandle(dn) == b->phandle )
				return b->domain;
	printm("get_pci_domain failed\n");
	return -1;
}

/* bandit, chaos and uni-north.
 *
 * CFA0: [21 bits, device select (dev 11-31)] [3 bits function] [8 bits offset ~0x3]
 * CFA1: [zeros?] [8 bits bus] [8 bits devfn] [8 bits offset ~0x3]
 */
static pci_addr_t
macrisc_interpret_addr( pci_bridge_t *b, ulong pciaddr_le, int *offs )
{
	ulong tbit, pciaddr_be;
	int val, hit=0;
	int dev_fn, bus;
	
	pciaddr_be = ld_le32( &pciaddr_le );
	
	*offs = (pciaddr_be & ~3) & 0xff;
	if( !(pciaddr_be & 1) ) {
		/* CFA0 */
		dev_fn = (pciaddr_be >> 8) & 7;
		bus = b->first_bus;

		for( val=11, tbit=0x800; tbit; tbit=tbit<<1, val++ ) {
			if( pciaddr_be & tbit ) {
				hit++;
				dev_fn |= val<<3;
			}
		}
		if( hit != 1 )
			return -1;
	} else {
		/* CFA1 */
		dev_fn = (pciaddr_be >> 8) & 0xff;
		bus = (pciaddr_be >> 16) & 0xff;
	}
	if( bus < b->first_bus || bus > b->last_bus )
		return -1;

	return PCIADDR_FROM_BUS_DEVFN( b->domain, bus, dev_fn );
}

/* Grackle: LE:  (offset.B&~0x3) dev_fn.B bus.B 0x80  */
static pci_addr_t
grackle_interpret_addr( pci_bridge_t *b, ulong pciaddr_le, int *offs)
{
	int bus, dev_fn;
	
	*offs = pciaddr_le >> 24;
	dev_fn = (pciaddr_le >> 16) & 0xff;
	bus = (pciaddr_le >> 8) & 0xff;

	if( bus < b->first_bus || bus > b->last_bus )
		return -1;

	/* possibly an error should be returned if the enable bit is NOT set */
	return PCIADDR_FROM_BUS_DEVFN( b->domain, bus, dev_fn );
}


/************************************************************************/
/*	Bridges								*/
/************************************************************************/

static pci_dev_info_t bandit_pci_config = {
	/* vendor, device ID, revision, prog, class */
	0x106b, 0x0001, 0x03, 0x0/*?*/, 0x0600
};
static pci_dev_info_t grackle_pci_config = {
	/* vendor, device ID, revision, prog, class */
	0x1057, 0x0002, 0x03, 0x0/*?*/, 0x0600
};
static pci_dev_info_t uninorth_pci_config[3] = {
	/* vendor, device ID, revision, prog, class */
	{ 0x106b, 0x0034, 0x0, 0x0, 0x0600 },		/* AGP */
	{ 0x106b, 0x0035, 0x0, 0x0, 0x0600 },		/* slots, key-largo */
	{ 0x106b, 0x0036, 0x0, 0x0, 0x0600 }		/* firewire / gmac */
};

static void
init_bandit_bridge( mol_device_node_t *dn, struct pci_bridge *b )
{
	pci_addr_t addr = PCIADDR_FROM_BUS_DEVFN( b->domain, b->first_bus, MACRISC_DEV_FN );
	add_pci_device( addr, &bandit_pci_config, NULL );
}

static void
init_chaos_bridge( mol_device_node_t *dn, struct pci_bridge *b )
{
}

static void
init_uninorth_bridge( mol_device_node_t *dn, struct pci_bridge *b )
{
	pci_addr_t addr;
	uint ind = ((b->addr_mbase)-0xf0000000)/0x2000000;

	if( ind >= 3 ) {
		printm("bad bridge\n");
		return;
	}
	/* distinguish between host bridges by assigning them different domains */
	b->domain = ind;

	addr = PCIADDR_FROM_BUS_DEVFN( b->domain, b->first_bus, MACRISC_DEV_FN );	
	add_pci_device( addr, &uninorth_pci_config[ind], NULL );
}

static void
init_grackle_bridge( mol_device_node_t *dn, struct pci_bridge *b )
{
	pci_addr_t addr = PCIADDR_FROM_BUS_DEVFN( b->domain, b->first_bus, GRACKLE_DEV_FN );
	add_pci_device( addr, &grackle_pci_config, NULL );
}


/************************************************************************/
/*	I / O								*/
/************************************************************************/

static ulong 
reg_io_read( ulong addr, int len, void *u_bridge )
{
	pci_bridge_t *bridge = (pci_bridge_t*)u_bridge;
	int offs;

	/* address port */
	if( (addr & ~0x3) == bridge->addr_mbase ) {
		ulong buf[2];
		buf[0] = bridge->addr;
		return read_mem( (char*)buf+(addr&3), len );
	}
	/* data port */
	if( (addr & ~0x7) == bridge->data_mbase ) {
		pci_addr_t pci_addr = bridge->interpret_addr( bridge, bridge->addr, &offs );
		ulong ret = (addr < 0) ? 0 : read_pci_config( pci_addr, offs+(addr&3), len );
#if defined(DEBUG_ADDR) 
		if( addr == DEBUG_ADDR )
			printm("PCI, read [%d] (%04x)+%02X: %08lX\n", len, pci_addr, offs, ret );
#endif
		return ret;
	}
	return 0;
}

static void 
reg_io_write( ulong addr, ulong data, int len, void *u_bridge )
{
	pci_bridge_t *bridge = (pci_bridge_t*)u_bridge;
	int offs;
	
	/* Address port */
	if( (addr & ~0x3) == bridge->addr_mbase ) {
		ulong buf[2];
		buf[0] = bridge->addr;
		write_mem( (char*)buf+(addr&3), data, len );
		bridge->addr = buf[0];
		return;
	}

	/* Data port - note that Chaos uses a 64-bit bus interface (or?),
	 * placing the data in the upper word for xxx4/xxxc accesses 
	 */
	if( (addr & ~0x7) == bridge->data_mbase ) {
		pci_addr_t pci_addr = bridge->interpret_addr( bridge, bridge->addr, &offs );
		if( pci_addr != -1 )
			write_pci_config( pci_addr, offs+(addr&3), data, len );
#if defined(DEBUG_ADDR) 
		if( addr == DEBUG_ADDR )
			printm("PCI, write [%d] (%04x)+%02X: %08lX\n", len, pci_addr, offs, ret );
#endif
	}
}

static void 
reg_io_print( int isread, ulong addr, ulong data, int len, void *u_bridge )
{
	pci_bridge_t *bridge = (pci_bridge_t*)u_bridge;
	pci_addr_t pci_addr;
	ulong d;
	int offs;
	
	if( (addr & ~0x3) == bridge->addr_mbase ) {
#if 0
		pci_addr = bridge->interpret_addr( bridge, bridge->addr, &offs );
		printm("Bridge '%s' %s %08lX ; (%04x+%02x)\n", bridge->bridge_name,
		       isread ? "read: " : "write:", ld_le32(&data), pci_addr, offs );
#endif
		return;
	}

	if( (addr & ~0x7) == bridge->data_mbase ) {
		pci_addr = bridge->interpret_addr( bridge, bridge->addr, &offs );
		addr &= ~0x4;

		d = (len == 1)? data : (len == 2)? bswap_16(data) : bswap_32(data);

		printm("Bridge '%s-%d' %s (%04x+%02lX): %08lX\n", bridge->bridge_name,
		       len, isread? "read " : "write", pci_addr, offs+(addr&3), d);
		return;
	}

	printm("bridge ???? %08lX: (len=%d) %s %08lX\n", addr, len,
	       isread ? "read: " : "write:", data );
	/* stop_emulation(); */
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static io_ops_t ops = {
	.read	= reg_io_read,
	.write	= reg_io_write, 
	.print	= reg_io_print
};

static void
init_bridges( char *device_name )
{
	mol_device_node_t *dn = prom_find_devices( device_name );
	int first_bus, last_bus, *regp, *busr, rl;
	pci_bridge_t *bridge;
	char namebuf[40];

	for( ; dn ; dn=dn->next ) {
		if( !(busr=(int*)prom_get_property(dn, "bus-range", &rl)) || rl != sizeof(int[2]) ) {
			printm("%s: doesn't look lika a bridge\n", device_name );
			continue;
		}
		if( !(regp=(int*)prom_get_property(dn, "reg", &rl)) || rl != sizeof(int[2]) ) {
			printm("%s: Expecting 1 addrs\n", device_name );
			continue;
		}
		first_bus = busr[0] & 0xff;
		last_bus = busr[1] & 0xff;

		bridge = malloc( sizeof(*bridge) );
		memset( bridge, 0, sizeof(*bridge) );

		bridge->bridge_name = device_name;
		bridge->mbase = *regp;
		bridge->first_bus = first_bus;
		bridge->last_bus = last_bus;
		bridge->phandle = prom_dn_to_phandle( dn );

		/* assume bandit, chaos or uni-north */
		bridge->addr_mbase = bridge->mbase + MACRISC_CFG_ADDR_OFFS;
		bridge->data_mbase = bridge->mbase + MACRISC_CFG_DATA_OFFS;
		bridge->interpret_addr = macrisc_interpret_addr;
		bridge->bridge_type = !strcmp(device_name, "chaos") ? chaos_bridge : bandit_bridge;
		bridge->domain = 0;

		/* the grackle bridge (MPC106) is different */
		if( !strcmp(device_name, "pci") ) {
			char *s = (char *) prom_get_property(dn, "compatible", NULL);
			if( s && !strcmp(s, "uni-north") ) {
				bridge->bridge_type = uninorth_bridge;
			} else {
				bridge->addr_mbase = GRACKLE_CFG_ADDR;
				bridge->data_mbase = GRACKLE_CFG_DATA;
				bridge->interpret_addr = grackle_interpret_addr;
				bridge->bridge_type = grackle_bridge;
			}
		}
		
		/* the minimum IO-area size is 16 bytes, we need only 4 (8 for chaos) bytes though */
		snprintf( namebuf, sizeof(namebuf), "%s-%x-%x-addr", device_name, first_bus, last_bus );
		add_io_range( bridge->addr_mbase, 0x10, namebuf, 0, &ops, (void*)bridge );
		snprintf( namebuf, sizeof(namebuf), "%s-%x-%x-data", device_name, first_bus, last_bus );
		add_io_range( bridge->data_mbase, 0x10, namebuf, 0, &ops, (void*)bridge );

		bridge->next = all_bridges;
		all_bridges = bridge;
		
		switch( bridge->bridge_type ) {
		case chaos_bridge:
			init_chaos_bridge( dn, bridge );
			break;
		case bandit_bridge:
			init_bandit_bridge( dn, bridge );
			break;
		case uninorth_bridge:
			init_uninorth_bridge( dn, bridge );
			break;
		case grackle_bridge:
			init_grackle_bridge( dn, bridge );
			break;
		}
		/* printm("PCI-bridge '%s' (bus %d..%d) installed at 0x%08lx\n", 
		   device_name, first_bus, last_bus, bridge->mbase ); */
	}
}

static int
pci_bridges_init( void )
{
	all_bridges = NULL;
	init_bridges( "bandit" );
	init_bridges( "chaos" );
	init_bridges( "pci" );
	return 1;
}

static void
pci_bridges_cleanup( void )
{
	pci_bridge_t *br;
	
	while( (br=all_bridges) ) {
		all_bridges = br->next;
		if( br->type_data )
			free( br->type_data );
		free( br );
	}
}

driver_interface_t pci_bridges_driver = {
	"pci-bridges", pci_bridges_init, pci_bridges_cleanup
};
