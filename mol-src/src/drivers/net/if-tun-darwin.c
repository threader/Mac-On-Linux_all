/* 
 *   Creation Date: <2004/03/28 00:21:52 samuel>
 *   Time-stamp: <2004/04/03 22:20:06 samuel>
 *   
 *	<if-tun-darwin.c>
 *	
 *	TUN driver for Darwin
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
#include <sys/wait.h>
#include <net/if.h>
#include <sys/sockio.h>
#include "enet.h"
#include "res_manager.h"

static const char	tunconfig_res[] = "tunconfig.osx";
static const char	def_hw_addr[6] = { 0, 0, 0x0D, 0xEA, 0xDB, 0xEE };

static void
tun_preconfigure( enet_iface_t *is )
{
	memcpy( is->c_macaddr, def_hw_addr, 6 );
}

static int
tun_open( enet_iface_t *is )
{
	char tundev[16];
	int fd;

	is->flags |= IP_PAYLOAD;
	sprintf( is->iface_name, "tun%d", g_session_id );
	sprintf( tundev, "/dev/tun%d", g_session_id );
	
	if( (fd=open(tundev, O_RDWR | O_NONBLOCK)) < 0 ) {
		perrorm("Failed to open /dev/tun0");
		return 1;
	}

	script_exec( get_filename_res(tunconfig_res), is->iface_name, "up" );
	
	netif_open_common( is, fd );
	return 0;
}

static void
tun_close( enet_iface_t *is )
{
	script_exec( get_filename_res(tunconfig_res), is->iface_name, "down" );
	netif_close_common( is );
}

static packet_driver_t tun_pd = {
	.name		= "tun",
	.flagstr	= "-tun",
	.id		= TUN_PACKET_DRIVER_ID,
	.preconfigure	= tun_preconfigure,
	.open		= tun_open,
	.close		= tun_close,
};

DECLARE_PACKET_DRIVER( init_tun, tun_pd );
