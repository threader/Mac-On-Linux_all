/* 
 *   Creation Date: <1999/03/06 13:37:20 samuel>
 *   Time-stamp: <2003/06/10 23:15:06 samuel>
 *   
 *	<pci.c>
 *	
 *   PCI top-level driver (used by bandit, chaos etc)
 *   
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   pci_device_loc() was taken from pmac_pci.c:
 *   Copyright (C) 1997 Paul Mackerras (paulus@cs.anu.edu.au)
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

/* #define VERBOSE */

#include "pcidefs.h"
#include "verbose.h"
#include "promif.h"
#include "ioports.h"
#include "debugger.h"
#include "pci.h"
#include "res_manager.h"
#include "driver_mgr.h"
#include "byteorder.h"

SET_VERBOSE_NAME("PCI")

typedef struct basereg {
	ulong	r;		/* B.E. register value */
	ulong	old_r;		/* value used in last mapping */
	ulong	mask;		/* decoded bits (e.g. 0xffff0000) */
	int	flags;
} basereg_t;

/* flags field in basereg */
enum {
	bf_mapped=1,		/* register is currently mapped */
};

typedef struct pci_device pci_device_t;

struct pci_device {
	pci_addr_t	addr;		/* 00 00 bus dev_fn */

	void		*usr;		/* user private pointer */

	int		config_len;	/* length of config_data (>=PCI_BASE_ADDRESS_0) */
	char		*config_data;	/* config data (VENDOR etc) */

	char		*rombuf;
	int		rom_map_id;
	int		rom_fd;
	ulong		rom_size;
	ulong		mapped_rombase;

	/* read/write registers (config space) */
	short		cmd;		/* B.E. cmd register */
	basereg_t	base[6];	/* B.E. base register */
	basereg_t	rombase;	/* B.E. rom base register */

	pci_map_base_fp	map_proc[6];

	/* hooks */
	pci_dev_hooks_t hooks;

	pci_device_t	*next;	
};

static pci_device_t	*first_dev;


static pci_device_t *
find_device( pci_addr_t addr )
{
	pci_device_t *dev = first_dev;

	while( dev && dev->addr != addr )
		dev = dev->next;
	return dev;
}	

int
add_pci_device( pci_addr_t addr, pci_dev_info_t *info, void *usr )
{
	pci_device_t *dev;
	char *p;
	
	VPRINT("Adding pci device %04x\n", addr );
	
	if( find_device(addr) ) {
		printm("PCI device %04x already exists\n", addr );
		return -1;
	}
	dev = malloc( sizeof(*dev) );
	memset( dev, 0, sizeof(*dev) );

	/* by default, allocate only first part of page */ 
	dev->config_len = PCI_BASE_ADDRESS_0;
	p = dev->config_data = calloc( dev->config_len, 1 );

	st_le16( (unsigned short*)(p + PCI_VENDOR_ID), info->vendor_id );
	st_le16( (unsigned short*)(p + PCI_DEVICE_ID), info->device_id );
	st_le16( (unsigned short*)(p + PCI_CLASS_DEVICE), info->device_class );
	*(p + PCI_CLASS_PROG) = info->class_prog;
	*(p + PCI_REVISION_ID) = info->revision;
	*(p + PCI_CACHE_LINE_SIZE) = 0x08;
	*(p + PCI_LATENCY_TIMER) = 0x20;

	/* auto-detect multi-function cards and set the header bit */
	if( (addr & 7) ) {
		pci_device_t *dev2 = find_device( addr & ~7 );
		if( dev2 )
			dev2->config_data[PCI_HEADER_TYPE] |= 0x80;
		VPRINT("Adding mutli-function PCI card\n");
	} else {
		int i;
		for( i=1; i<=7 && !find_device(addr+i) ; i++ )
			;
		if( i <= 7 ) 
			dev->config_data[PCI_HEADER_TYPE] |= 0x80;
	}

	dev->addr = addr;
	dev->usr = usr;
	dev->rom_fd = -1;
	dev->next = first_dev;
	first_dev = dev;
	return 0;
}

pci_addr_t
add_pci_device_from_dn( mol_device_node_t *dn, void *usr )
{
	int vendor, dev_id, rev, class;
	pci_dev_info_t info;
	int addr, err;

	if( (addr=pci_device_loc(dn)) < 0 ) {
		printm("PCI address lookup failure\n");
		return -1;
	}
	err = prom_get_int_property( dn, "vendor-id", &vendor )
		|| prom_get_int_property( dn, "device-id", &dev_id )
		|| prom_get_int_property( dn, "revision-id", &rev )
		|| prom_get_int_property( dn, "class-code", &class );
	if( err ) {
		printm("PCI properties missing\n");
		return -1;
	}
	info.vendor_id = vendor;
	info.device_id = dev_id;
	info.revision = rev;
	info.device_class = class >> 8;
	info.class_prog = class & 0xff;

	if( add_pci_device(addr, &info, usr) )
		return -1;
	return addr;
}

void 
set_pci_dev_usr_info( pci_addr_t addr, void *usr ) 
{
	pci_device_t *dev;
	if( (dev=find_device(addr)) )
		dev->usr = usr;
	else 
		printm("set_pci_user_info: Bad addr\n");
}

void
set_pci_dev_hooks(pci_addr_t addr, pci_dev_hooks_t *h)
{
	pci_device_t *dev = find_device(addr);

	if (dev != NULL)
		/* copy over the hooks table */
		memcpy(&(dev->hooks), h, sizeof(pci_dev_hooks_t));
	else
		printm("set_pci_dev_config_hooks: pci dev not found by addr %#08x\n", addr);
}


/************************************************************************/
/*	MISC								*/
/************************************************************************/

pci_addr_t 
pci_device_loc( mol_device_node_t *dn )
{
	unsigned int *reg;
	int len, domain;

	if( !(reg=(unsigned int *)prom_get_property(dn, "reg", &len)) )
		return -1;
	if( len < sizeof(unsigned int[5]) ) {
		printm("pci.c: Doesn't look lika a PCI device\n");
		return -1;
        }
	domain = get_pci_domain( dn );
	
	return ((reg[0] >> 8) & 0xffff) | (domain << 16);
}

/*
 * This function reads the 'assigned-addresses' property from the device tree
 * and write corresponding values to the PCI registers. This is used in the
 * bootx/newworld setting. 
 */
void
pci_assign_addresses( void )
{
	mol_device_node_t *dn;
	unsigned short cmd;
	pci_addr_t addr;
	int *reg, i, len;

	/* map in PCI devices */
	for( dn=prom_get_allnext(); dn ; dn=dn->allnext ) {
		if( !(reg=(int*)prom_get_property(dn, "assigned-addresses", &len)) )
			continue;
		if( !len || (len % sizeof(int[5])) || (addr=pci_device_loc(dn)) < 0 )
			continue;

		for( i=0; i<len/sizeof(int[5]) ; i++ ) {
			int reg0 = reg[i*5];
			int r = reg0 & 0xff;
			ulong breg = reg[i*5+2];
			
			if( ((reg0 >> 24) & 3) == 2 ) { 
				/* memory */
				if( r != PCI_ROM_ADDRESS )
					breg &= PCI_BASE_ADDRESS_MEM_MASK;
				else {
					breg &= PCI_ROM_ADDRESS_MASK;
					breg |= PCI_ROM_ADDRESS_ENABLE;
				}
			} else {
				breg |= 1;
			}

			/* reloctable? */
			if( r )
				write_pci_config( addr, r, ld_le32(&breg), 4 );
			
			/* set enable bits */
			cmd = read_pci_config( addr, PCI_COMMAND, 2 );
			cmd = ld_le16(&cmd);
			cmd |= (((reg0 >> 24) & 3) == 2) ? PCI_COMMAND_MEMORY : PCI_COMMAND_IO;
			write_pci_config( addr, PCI_COMMAND, ld_le16(&cmd), 2 );
		}
	}
}


/************************************************************************/
/*	ROM I/O								*/
/************************************************************************/

static ulong
rom_read( ulong addr, int len, void *udev )
{
	pci_device_t *dev = (pci_device_t*) udev;

	if( addr + len > dev->mapped_rombase + dev->rom_size || addr < dev->mapped_rombase ) {
		LOG("rom_read: read outside ROM (%08lX, %08lX, %08lX)\n", addr, dev->mapped_rombase, dev->rom_size );
		return 0;
	}
	return read_mem( dev->rombuf + addr - dev->mapped_rombase, len );
}

static void 
rom_write( ulong addr, ulong data, int len, void *usr )
{
	LOG("PCI-ROM write\n");
}

static void
map_rom( pci_device_t *dev, ulong base, int map )
{
	static io_ops_t rom_ops = {
		.read	= rom_read,
		.write	= rom_write
	};

	if( !map ) {
		if( dev->rom_map_id )
			remove_io_range( dev->rom_map_id );
		if( dev->rombuf )
			free( dev->rombuf );
		dev->rombuf = NULL;
		dev->rom_map_id = 0;
		return;
	}
	if( dev->rom_fd == -1 || (dev->rombuf = malloc( dev->rom_size )) == NULL )
		return;

	lseek( dev->rom_fd, 0, SEEK_SET );
	if(read( dev->rom_fd, dev->rombuf, dev->rom_size ) != dev->rom_size) 
		printm("Warning: Couldn't read from PCI ROM!\n");
	
	dev->mapped_rombase = base;
	dev->rom_map_id = add_io_range( base, dev->rom_size, "PCI-ROM", 0, &rom_ops, dev );
}


/************************************************************************/
/*	handle BARs							*/
/************************************************************************/

/* reg == -1 for ROM, 0-5 for the registers PCI_BASE_ADDRESS_0..5 */
static void 
update_base_register( pci_device_t *dev, int reg )
{
	basereg_t *br = &dev->base[reg];
	int do_map=0;
	ulong map_addr=0;
	
	if( (dev->cmd & PCI_COMMAND_IO) && (br->r & PCI_BASE_ADDRESS_SPACE_IO) ){
		/* map I/O - how is this done? */
		LOG("Don't know how to map I/O space\n");
		do_map = 1;
		map_addr = br->r & PCI_BASE_ADDRESS_IO_MASK;
	} else if( (dev->cmd & PCI_COMMAND_MEMORY) && !(br->r & PCI_BASE_ADDRESS_SPACE_IO) ){
		/* map MEMORY - if >1MB */
		if( br->r > 0x100000 ) {
			do_map = 1;
			map_addr = br->r & PCI_BASE_ADDRESS_MEM_MASK;
		}
	}

	if( !do_map && (br->flags & bf_mapped) ) {
		br->flags &= ~bf_mapped;
		VPRINT("PCI base address unmapped (%04x) %08lX\n", dev->addr, br->r );
		if( dev->map_proc[reg] )
			dev->map_proc[reg]( reg, 0,  0 /*unmap*/, dev->usr );
	}

	if( do_map ) {
		/* already mapped? */
		if( (br->flags & bf_mapped) && br->r == br->old_r )
			return;

		br->old_r = br->r;
		br->flags |= bf_mapped;
		
		/* map */
		VPRINT("PCI base address mapped (%04x) %08lX\n", dev->addr, br->r );
		if( dev->map_proc[reg] )
			dev->map_proc[reg]( reg, map_addr, 1 /*map*/, dev->usr );
	}
}

/* 
 * This function sets handles mapping and unmapping of
 * the base registers and the ROM.
 */
static void
update_base_registers( pci_device_t *dev )
{
	basereg_t *rb = &dev->rombase;
	int i, map;
	
	for(i=0; i<6; i++ )
		update_base_register(dev, i );

	/* map ROM */
	map = ((rb->r & PCI_ROM_ADDRESS_ENABLE) && (dev->cmd & PCI_COMMAND_MEMORY)) ? 1:0;
	if( map && (rb->flags & bf_mapped) && rb->r != rb->old_r ) {
		map_rom( dev, 0, 0 );
		rb->flags &= ~bf_mapped;
	}
	if( map != ((rb->flags & bf_mapped)? 1:0) )
		map_rom( dev, rb->r & PCI_ROM_ADDRESS_MASK, map );
	rb->flags &= ~bf_mapped;
	rb->flags |= map ? bf_mapped : 0;
}

/* return 0xffff000 style mask */
static ulong
make_mask( ulong size )
{
	ulong b;

	for( b=(1<<31); b; b=b>>1 )
		if( size & b )
			break;
	if( size & ~b )
		b=b << 1;

	return ~(b-1);
}

int
use_pci_rom( pci_addr_t addr, char *filename )
{
	pci_device_t *dev;
	int fd;
	
	if( !filename )
		return 1;

	if( !(dev=find_device(addr)) || dev->rom_fd != -1 ) {
		LOG("Can't find device (%04x)\n", addr );
		return 1;
	}
	if( (fd=open(filename, O_RDONLY)) < 0 ) {
		LOG_ERR("opening '%s'", filename);
		return 1;
	}

	dev->rom_fd = fd;
	dev->rom_size = lseek( fd, 0, SEEK_END );

	dev->rombase.mask = make_mask( dev->rom_size );
	dev->rombase.mask |= PCI_ROM_ADDRESS_ENABLE;

	update_base_registers( dev );
	return 0;
}

static void
set_base( pci_device_t *dev, int i, ulong val )
{
	val &= dev->base[i].mask;
	if( (dev->base[i].mask & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO ) {
		val &= PCI_BASE_ADDRESS_IO_MASK;
		dev->base[i].r = val | (dev->base[i].mask & ~PCI_BASE_ADDRESS_IO_MASK);
	} else {
		val &= PCI_BASE_ADDRESS_MEM_MASK;
		dev->base[i].r = val | (dev->base[i].mask & ~PCI_BASE_ADDRESS_MEM_MASK);
	}
}

int 
set_pci_base( pci_addr_t addr, int ind, ulong bmask, pci_map_base_fp proc )
{
	pci_device_t *dev;

	if( ind < 0 || ind > 5 || !(dev=find_device(addr)) ) {
		LOG("Error in set_pci_base_reg (%04x)\n", addr );
		return 1;
	}
	dev->base[ind].mask = bmask;
	set_base( dev, ind, 0 );
	dev->map_proc[ind] = proc;

	update_base_registers( dev );
	return 0;
}


/************************************************************************/
/*	Config access							*/
/************************************************************************/

#define ALIGN_INS_BYTE( dest, val8, offs ) \
	dest &= ~(0xffL << ((offs)&3)*8); \
	dest |= (val8 & 0xff) << ((offs)&3)*8;

#define ALIGN_READ_BYTE( val, offs ) \
	(((val) >> ((offs)&3)*8) & 0xff)


static void
do_write_config( pci_device_t *dev, int offs, unsigned char val )
{
	
	int rr = offs >>2;
	ulong old;
	
	/* indeed we now call a hook here ;-) */
	if (dev->hooks.write_config != NULL)
		(*(dev->hooks.write_config))(dev->usr, offs, (char *) &val);

	/* Base registers & rom */
	if( rr >= (PCI_BASE_ADDRESS_0 >> 2) && rr <= (PCI_BASE_ADDRESS_5 >> 2) ) {
		int ind = rr - (PCI_BASE_ADDRESS_0 >> 2);
		old = dev->base[ind].r;

		ALIGN_INS_BYTE( old, val, offs & 3 );
		set_base( dev, ind, old );

	} else if( rr == (PCI_ROM_ADDRESS >> 2) ) {
		old = dev->rombase.r;
		ALIGN_INS_BYTE( old, val, offs & 3 );
		dev->rombase.r = old & dev->rombase.mask;
		dev->rombase.r &= PCI_ROM_ADDRESS_MASK | PCI_ROM_ADDRESS_ENABLE;
	} else if ( (offs & ~1) == PCI_COMMAND ){
		/* Command register */
		ALIGN_INS_BYTE( dev->cmd, val, offs & 1 );
		return;
	}
}

static unsigned char
do_read_config( pci_device_t *dev, int offs )
{
	int rr = offs >>2;
	basereg_t *bp = NULL;
	char val;

	/* base registers & rom */
	if( rr >= (PCI_BASE_ADDRESS_0>>2) && rr <= (PCI_BASE_ADDRESS_5>>2))
		bp = &dev->base[rr-(PCI_BASE_ADDRESS_0>>2)];
	else if( rr == (PCI_ROM_ADDRESS>>2) ) {
		bp = &dev->rombase;
	}

	if( bp ) {
		val = ALIGN_READ_BYTE( bp->r, offs & 3 );
	}

	/* Command register */
	if( (offs & ~1) == PCI_COMMAND )
		val = ALIGN_READ_BYTE( dev->cmd, offs & 1 );

	/* Read from static config data */
	if( dev->config_data && offs < dev->config_len )
		val = dev->config_data[offs];

	/* call hook */
	if (dev->hooks.read_config != NULL)
		(*(dev->hooks.read_config))(dev->usr, offs, &val);

#if 0
	if( offs == PCI_INTERRUPT_LINE ) {
		printm("PCI_INTERRUPT_LINE\n");
		return 1;
	}
	if( offs == PCI_INTERRUPT_PIN ) {
		printm("PCI_INTERRUPT_PIN\n");
		return 1;
	}
#endif
	return val;
}

void
write_pci_config( pci_addr_t addr, int offs, ulong val, int len )
{
	pci_device_t *dev;
	int i;

	VPRINT("config-write [%d] %04x+%02X: %08lX\n",len, addr, offs, val );

	if( !(dev=find_device(addr)) )
		return;

	/* For simplicity, we access byte for byte to circumvent
	 * alignment problems.
	 */
	for( i=len-1; i>=0; i--, val=val>>8 )
		do_write_config( dev, offs+i, val & 0xff ); 

	/* Handle value written */
	if( (offs & ~3) != ((offs+len-1)&~3 ))
		LOG("alignment problems!\n");

	/* This function is only called a few times in the boot process
	 * - we can afford ome extra overhead here.
	 */
	update_base_registers( dev );
}

ulong
read_pci_config( pci_addr_t addr, int offs, int len )
{
	pci_device_t *dev;
	ulong val, i;

	if( !(dev=find_device(addr)) ) {
		/* what should we return according to the standard? */
		return 0xffffffff;
	}

	for( val=0, i=0; i<len; i++ ) {
		val=val<<8;
		val |= do_read_config( dev, offs+i ) & 0xff;
	}
	
	VPRINT("config-read  [%d] %04x+%02x: %08lx\n", len, addr, offs, val );
	return val;
}

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static int
pci_init( void )
{
	return 1;
}

static void
pci_cleanup( void )
{
	pci_device_t *dev;

	while( (dev=first_dev) ) {
		first_dev = dev->next;

		if( dev->rom_fd != -1 )
			close( dev->rom_fd );
		if( dev->rombuf )
			free( dev->rombuf );
		if( dev->rom_map_id )
			remove_io_range( dev->rom_map_id );
		free( dev );
	}
}

driver_interface_t pci_driver = {
	"pci", pci_init, pci_cleanup
};
