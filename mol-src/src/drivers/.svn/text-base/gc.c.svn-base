/* 
 *   Creation Date: <1999/03/06 18:35:25 samuel>
 *   Time-stamp: <2003/06/02 13:33:09 samuel>
 *   
 *	<gc.c>
 *	
 *	Grand Central (GC)
 *
 *	This file adds the PCI device - the chips in the GC
 *	are added elsewhere.
 *
 *   Copyright (C) 1999, 2000, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include "verbose.h"
#include "promif.h"
#include "ioports.h"
#include "debugger.h"
#include "pci.h"
#include "gc.h"
#include "driver_mgr.h"
#include "booter.h"

/* REG_COUNT needs to be a multiple of 4 in order not to
 * trigger add_io_range alignment error
 */
#define REG_COUNT	4
#define REG_OFFSET	0x30

static mol_device_node_t	*gc_node;
static gc_range_t		*gc_regs_range, *first_range;
static ulong			registers[REG_COUNT];


int
is_gc_child( mol_device_node_t *dn, gc_bars_t *gcbars )
{
	int i, s, *rp;
	if( is_ancestor(gc_node, dn) ) {
		if( gcbars ) {
			memset( gcbars, 0, sizeof(*gcbars) );
			rp = (int*)prom_get_property(dn, "reg", &s);
			s /= 2*sizeof(int);
			gcbars->nbars = s;
			for( i=0; i<s && i<sizeof(gcbars->offs)/sizeof(gcbars->offs[0]); i++ ) {
				gcbars->offs[i] = *rp++;
				gcbars->size[i] = *rp++;
			}
		}
		return 1;
	}
	return 0;
}

gc_range_t *
add_gc_range( int offs, int size, char *name, int flags, io_ops_t *ops, void *usr )
{
	gc_range_t *r = malloc( sizeof(*r) );
	memset( r, 0, sizeof(*r) );
	
	r->next = first_range;
	first_range = r;
	
	r->offs = offs;
	r->size = size;
	r->name = name;
	r->flags = flags;
	r->ops = ops;

	r->usr = usr;

	r->is_mapped = 0;
	return r;
}

int
gc_is_present( void )
{
	return gc_node ? 1:0;
}

static void 
do_map( int regnum, ulong base, int add_map, void *usr )
{
	gc_range_t *r;       

	if( is_oldworld_boot() ) {
		if( add_map )
			printm("Mapping GC at %08lX\n", base);
		else
			printm("Unmapping GC\n");
	}

	for( r=first_range; r; r=r->next ) {
		if( r->is_mapped )
			remove_io_range( r->range_id );
		
		if( add_map ) {
			r->base = r->offs + base;
			r->range_id = add_io_range( r->base, r->size, r->name, 
						    r->flags, r->ops, r->usr );
		}
		r->is_mapped = add_map;
	}
}

static ulong 
reg_io_read( ulong addr, int len, void *usr )
{
	ulong base = get_mbase( gc_regs_range );

	if ((addr >= base) && (addr < (base + REG_COUNT*4))) {
		return read_mem( ((char *)registers) + (addr - base), len );
	}
	
	printm("heathrow: invalid reg read @0x%lx, len: %d\n", addr, len);
	return 0;
}

static void 
reg_io_write( ulong addr, ulong data, int len, void *usr )
{
	ulong base = get_mbase( gc_regs_range );

	if ((addr >= base) && (addr < (base + REG_COUNT*4))) {
		write_mem( ((char *)registers) + (addr - base), data, len);
		return;
	}
	
	printm("heathrow: invalid reg write @0x%lx, len: %d\n", addr, len);
}

static void 
reg_io_print( int isread, ulong addr, ulong data, int len, void *u_bridge )
{
	printm("heathrow/ohare: %08lX: (len=%d) %s %08lX\n", addr, len,
	       isread ? "read: " : "write:", data );
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static io_ops_t macio_ops = {
	.read	= reg_io_read,
	.write	= reg_io_write, 
	.print	= reg_io_print
};

static int
gc_init( void )
{
	static pci_dev_info_t gc_pci_config = { 0x106b, 0x0002, 0x02, 0x0, 0xff00 };
	static pci_dev_info_t heathrow_pci_config = { 0x106b, 0x0010, 0x02, 0x0, 0xff00 };
	static pci_dev_info_t ohare_pci_config = { 0x106b, 0x0001, 0x03, 0x0, 0x0600 };
	mol_device_node_t *dn;
	pci_addr_t addr;

	if( (dn=prom_find_devices("gc")) ) {
		if( (addr=pci_device_loc(dn)) < 0 )
			printm("gc: Couldn't get pci device number\n");
		else {
			// printm("GC, Grand Central, (%04x) found\n", addr );
			add_pci_device( addr, &gc_pci_config, NULL );
			set_pci_base( addr, 0, 0xfff80000, do_map );
			gc_node = dn;
		}
	}

	if( !dn && (dn=prom_find_devices("ohare")) ) {
		if( (addr=pci_device_loc(dn)) < 0 )
			printm("ohare: Couldn't get pci device number\n");
		else {
			// printm("OHare, (%04x) found\n", addr );
			add_pci_device( addr, &ohare_pci_config, NULL );
				/* I guess these are endian reversed */
			registers[0] = 0x0000ff00;	/* from Starmax */
			registers[1] = 0x10000000;
			registers[2] = 0x00beff7a;
			gc_regs_range = add_gc_range( REG_OFFSET, REG_COUNT*4, "ohare regs", 0, &macio_ops, NULL);
			set_pci_base( addr, 0, 0xfff80000, do_map );
			gc_node = dn;
		}
	}

	if( !dn && (dn=prom_find_devices("mac-io")) ) {
		if( (addr=pci_device_loc(dn)) < 0 ) {
			if( !is_osx_boot() )
				printm("heathrow: Couldn't get pci device number\n");
		} else {
			// printm("Heathrow, (%d:%02d) found\n", bus, (dev_fn>>3) );

			add_pci_device( addr, &heathrow_pci_config, NULL );
			registers[0] = 0x0000FF00; 	/* from G3 */
			registers[1] = 0x391070F0;
			registers[2] = 0x61befffb; 	/* fcr */
			gc_regs_range = add_gc_range( REG_OFFSET, REG_COUNT*4, "mac-io regs", 0, &macio_ops, NULL);
			set_pci_base( addr, 0, 0xfff80000, do_map );
			gc_node = dn;
		}
	}
	return 1;
}

static void
gc_cleanup( void )
{
	gc_range_t *r, *next;

	for( r=first_range; r; r=next ){
		next = r->next;
		free( r );
	}
	first_range = NULL;
}

driver_interface_t gc_driver = {
    "gc", gc_init, gc_cleanup
};
