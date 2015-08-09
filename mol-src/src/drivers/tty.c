/* 
 *   Creation Date: <2003/11/17 14:23:35 samuel>
 *   Time-stamp: <2004/01/05 22:55:34 samuel>
 *   
 *	<tty.c>
 *	
 *	TTY access
 *   
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#include "mol_config.h"
#include <termios.h>
#include "driver_mgr.h"
#include "async.h"
#include "poll_compat.h"
#include "os_interface.h"
#include "pic.h"
#include "promif.h"
#include "res_manager.h"
#include "osi_driver.h"
#include "pci.h"
#include "booter.h"
#include "ioports.h"

static struct {
	struct osi_driver *osi_driver;
	int	iorange_id;
	int	irq;

	int	fd;
	int	handler_id;
} tty;

static void
tty_event( int fd, int dummy_events )
{
	set_async_handler_events( tty.handler_id, 0 );
	irq_line_hi( tty.irq );
}

static int
osip_tty_getc( int sel, int *params )
{
	unsigned char ch;
	if( read(tty.fd, &ch, 1) != 1 )
		return -1;
	return ch;
}

static int
osip_tty_putc( int sel, int *params )
{
	unsigned char ch = params[0];
	return write(tty.fd, &ch, 1);
}

static int
osip_tty_irq_ack( int sel, int *params )
{
	if( tty.irq ) {
		irq_line_low( tty.irq );
		set_async_handler_events( tty.handler_id, POLLIN );
	}
	return 0;
}

static const osi_iface_t osi_iface[] = {
	{ OSI_TTY_GETC,		osip_tty_getc		},
	{ OSI_TTY_PUTC,		osip_tty_putc		},
	{ OSI_TTY_IRQ_ACK,	osip_tty_irq_ack	},
};


/************************************************************************/
/*	PCI register interface						*/
/************************************************************************/

static ulong
io_read( ulong addr, int len, void *usr )
{
	unsigned char ch;
	if( read(tty.fd, &ch, 1) != 1 )
		return -1;
	return ch;
}

static void
io_write( ulong addr, ulong data, int len, void *usr )
{
	unsigned char ch = data;
	write(tty.fd, &ch, 1);
}

static io_ops_t tty_ops = {
	.read	= io_read,
	.write	= io_write,
};

static void 
map_base_proc( int regnum, ulong base, int add_map, void *usr )
{
	if( tty.iorange_id )
		remove_io_range( tty.iorange_id );
	if( add_map )
		tty.iorange_id = add_io_range( base, 0x1000, "mol-tty", 0, &tty_ops, NULL );
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static int
tty_init( void )
{
	mol_device_node_t *dn = prom_find_devices("mol-tty");
	struct termios tio;
	pci_addr_t addr;
	char *s;
	int fd;

	tty.fd = -1;

	if( get_bool_res("tty-interface") != 1 ) {
		prom_delete_node( dn );
		return 0;
	}
	
	/* add the pci card and register the osi selectors */
	if( !is_oldworld_boot() ) {
		irq_info_t irqinfo;
		if( !dn )
			return 0;
		if( prom_irq_lookup(dn, &irqinfo) == 1 )
			tty.irq = irqinfo.irq[0];
	} else {
		static pci_dev_info_t pci_config = { 0x1000, 0x0003, 0x02, 0x00, 0x0100 };
		if( !(tty.osi_driver=register_osi_driver("tty", "mol-tty", &pci_config, &tty.irq)) )
			return 0;
		if( (addr=get_driver_pci_addr(tty.osi_driver)) < 0 ) {
			printm("Could not find tty PCI-slot!\n");
			return 0;
		}
		set_pci_dev_usr_info( addr, &tty );
		set_pci_base( addr, 0, 0xfffff000, map_base_proc );
	}

	/* setup the tty */
	if( (fd=open("/dev/ptmx", O_RDWR | O_NONBLOCK)) < 0 ) {
		perrorm("opening /dev/ptmx:");
		return 0;
	}
	grantpt( fd );
	unlockpt( fd );

	/* set tty options */
	tcgetattr( fd, &tio );
	cfmakeraw( &tio );
	tcsetattr( fd, TCSANOW, &tio );

	printm("TTY endpoint: %s\n", ptsname(fd) );

	/* make a symlink to our pty */
	if( (s=get_filename_res("pty_link")) ) {
		unlink(s);
		symlink( ptsname(fd), s );
	}
	tty.fd = fd;

	if( tty.irq )
		tty.handler_id = add_async_handler( fd, POLLIN, tty_event, 0 );
	register_osi_iface( osi_iface, sizeof(osi_iface) );
	return 1;
}

static void
tty_cleanup( void )
{
	char *s;
	if( (s=get_filename_res("pty_link")) )
		unlink(s);
	if( tty.irq )
		delete_async_handler( tty.handler_id );
	close( tty.fd );
}

driver_interface_t tty_driver = {
    "tty", tty_init, tty_cleanup
};
