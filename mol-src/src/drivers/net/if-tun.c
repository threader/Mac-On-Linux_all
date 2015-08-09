/* 
 *   Creation Date: <2004/03/28 00:19:23 samuel>
 *   Time-stamp: <2004/06/05 17:58:57 samuel>
 *   
 *	<if-tun.c>
 *	
 *	TUN packet driver
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/if.h>
#include <linux/if_tun.h>
#include "res_manager.h"
#include "enet.h"

#define TUNSETUP_RES		"tunconfig"
static const char		def_hw_addr[6] = { 0, 0, 0x0D, 0xEA, 0xDB, 0xEE };

/* #define USE_TUN */

static void
tun_preconfigure( enet_iface_t *is )
{
	memcpy( is->c_macaddr, def_hw_addr, 6 );
	is->c_macaddr[5] += g_session_id;
}

static int
tun_open( enet_iface_t *is )
{
	struct ifreq ifr;
	int fd;

	if( !strlen(is->iface_name) )
		strcpy( is->iface_name, "mol" );

	/* allocate tun device */ 
	if( (fd=open("/dev/net/tun", O_RDWR | O_NONBLOCK)) < 0 ) {
		perrorm("Failed to open /dev/net/tun");
		return 1;
	}
	fcntl( fd, F_SETFD, FD_CLOEXEC );

	memset( &ifr, 0, sizeof(ifr) );
#ifdef USE_TUN
	ifr.ifr_flags = IFF_TUN | IFF_NO_PI;
	is->flags |= IP_PAYLOAD;
#else
	ifr.ifr_flags = IFF_TAP | IFF_NO_PI;
#endif
	strncpy( ifr.ifr_name, is->iface_name, IFNAMSIZ );
	if( ioctl(fd, TUNSETIFF, &ifr) < 0 ){
		perrorm("TUNSETIFF");
		goto out;
	}

	/* don't checksum */
	ioctl( fd, TUNSETNOCSUM, 1 );

	/* configure device */
	script_exec( get_filename_res(TUNSETUP_RES), is->iface_name, "up" );
	
	netif_open_common( is, fd );
	return 0;

 out:
	close(fd);
	return 1;
}

static void
tun_close( enet_iface_t *is )
{
	script_exec( get_filename_res(TUNSETUP_RES), is->iface_name, "down" );
	netif_close_common( is );
}

static packet_driver_t tun_pd = {
	.name		= "tun",
	.flagstr	= "-tun",
	.id		= TUN_PACKET_DRIVER_ID,
	.preconfigure	= tun_preconfigure,
	.open 		= tun_open,
	.close		= tun_close,
};

DECLARE_PACKET_DRIVER( init_tun, tun_pd );
