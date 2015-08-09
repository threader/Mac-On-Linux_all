/* 
 *   Creation Date: <2004/03/28 00:11:25 samuel>
 *   Time-stamp: <2004/06/05 17:58:04 samuel>
 *   
 *	<if-sheep.c>
 *	
 *	sheep packet driver
 *   
 *   Copyright (C) 1999-2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/ioctl.h>
#include "enet.h"


#define SIOC_MOL_GET_IPFILTER	SIOCDEVPRIVATE
#define SIOC_MOL_SET_IPFILTER	(SIOCDEVPRIVATE + 1)

static char sheep_virtual_hw_addr[6] = { 0xFE, 0xFD, 0xDE, 0xAD, 0xBE, 0xEF };


static void
sheep_preconfigure( enet_iface_t *is )
{
	/* disable DHCP for now (FIXME: we could get some of the data from the interface) */
	is->client_ip = is->broadcast_ip = is->my_ip = 0;
	is->netmask = is->nameserver = is->gateway = 0;
	is->flags |= NO_DHCP;
	memcpy( is->c_macaddr, sheep_virtual_hw_addr, 6 );
}

static int
sheep_open( enet_iface_t *is )
{
	int fd;
	
	/* verify that the device is up and running */
	if( check_netdev(is->iface_name) )
		return 1;
	
	/* open sheep_net device */
	if( (fd=open("/dev/sheep_net", O_RDWR | O_NONBLOCK)) < 0 ) {
		printm("-----> Can't open /dev/sheep_net, please check module is present !\n");
		return 1;
	}

	/* attach to selected Ethernet card */
	if( ioctl(fd, SIOCSIFLINK, is->iface_name) < 0) {
		printm("-----> Can't attach to interface <%s>\n", is->iface_name);
		goto err;
	}

	/* get/set ethernet address */
	if( ioctl(fd, SIOCSIFADDR, is->c_macaddr) < 0 )
		printm("----> An old version of the sheep_net kernel module is probably running!\n");
	ioctl( fd, SIOCGIFADDR, is->c_macaddr );

	/* set device specific variables */ 
	is->packet_pad = 0;

	netif_open_common( is, fd );
	return 0;
err:
	close(fd);
	return 1;
}

static void
sheep_close( enet_iface_t *is )
{
	netif_close_common( is );
}

static int
sheep_add_multicast( enet_iface_t *is, char *addr )
{
	if( ioctl(is->fd, SIOCADDMULTI, addr) < 0 ) {
		printm("sheep_add_multicast failed\n");
		return -1;
	}
	return 0;
}

static int 
sheep_del_multicast( enet_iface_t *is, char *addr )
{
	if( ioctl(is->fd, SIOCDELMULTI, addr) < 0 ) {
		printm("sheep_del_multicast failed\n");
		return -1;
	}
	return 0;
}

static int
sheep_load_save_state( enet_iface_t *is, enet_iface_t *load_is, int index, int loading )
{
	/* XXX: Multicast addresses are not handled */
	if( loading ) {
		if( ioctl(is->fd, SIOC_MOL_SET_IPFILTER, load_is->ipfilter) < 0 )
			perrorm("SIOC_MOL_SET_IPFILTER");
	} else {
		if( ioctl(is->fd, SIOC_MOL_GET_IPFILTER, &is->ipfilter) < 0 )
			perrorm("SIOC_MOL_GET_IPFILTER");
	}
	return 0;
}

static packet_driver_t sheep_pd = {
	.name			= "sheep",
	.flagstr		= "-sheep",
	.id			= SHEEP_PACKET_DRIVER_ID,
	.preconfigure		= sheep_preconfigure,
	.open			= sheep_open,
	.close			= sheep_close,
	.add_multicast		= sheep_add_multicast,
	.del_multicast		= sheep_del_multicast,
	.load_save_state	= sheep_load_save_state,
};

DECLARE_PACKET_DRIVER( init_sheep, sheep_pd );
