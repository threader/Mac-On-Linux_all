/* 
 *   Creation Date: <2002/05/22 22:41:40 samuel>
 *   Time-stamp: <2004/06/05 18:01:22 samuel>
 *   
 *	<iface.c>
 *	
 *	Support of various ethernet interfaces
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
#include <sys/socket.h>
#ifdef __linux__
#include <linux/if.h>
#endif
#include "res_manager.h"
#include "enet.h"

/* globals */
packet_driver_t		*g_packet_driver_list;


void
netif_open_common( enet_iface_t *is, int fd )
{
	is->fd = fd;
}

void
netif_close_common( enet_iface_t *is )
{
	close( is->fd );
	is->fd = -1;
}

#ifdef __linux__
int
check_netdev( const char *ifname )
{
	struct ifreq ifr;
	int fd, ret=-1;
	
	if( (fd=socket(AF_INET, SOCK_DGRAM, 0)) < 0 )
		return -1;

	memset( &ifr, 0, sizeof(ifr) );
	strncpy( ifr.ifr_name, ifname, sizeof(ifr.ifr_name) );
	if( ioctl(fd, SIOCGIFFLAGS, &ifr) < 0 ) {
		perrorm("SIOCGIFFLAGS");
		goto out;
	}
	if( !(ifr.ifr_flags & IFF_RUNNING) ) {
		printm("---> The network interface '%s' is not configured!\n", ifname);
		goto out;
	}
	if( (ifr.ifr_flags & IFF_NOARP) ) {
		printm("WARNING: Turning on ARP for device '%s'.\n", ifname);
		ifr.ifr_flags &= ~IFF_NOARP;
		if( ioctl(fd, SIOCSIFFLAGS, &ifr) < 0 ) {
			perrorm("SIOCSIFFLAGS");
			goto out;
		}
	}
	ret = 0;
 out:
	close(fd);
	return ret;
}
#endif /* __linux__ */

static inline void
initialize_drivers( void )
{
#ifdef __linux__
	init_tun();
	init_sheep();
#endif
#ifdef __darwin__
	init_tun();
#endif
}

static u32
lookup_ip( const char *ipstr )
{
	unsigned int val1, val2, val3, val4;
	if( sscanf(ipstr, "%d.%d.%d.%d", &val1, &val2, &val3, &val4) != 4 ) {
		printm("malformed ip %s\n", ipstr );
		return 0;
	}
	return (val1 << 24) | (val2 << 16) | (val3 << 8) | val4;
}

static void
lookup_macaddr( char *destaddr, const char *s )
{
	unsigned int val1, val2, val3, val4, val5, val6;
	if( sscanf(s, "%x:%x:%x:%x:%x:%x", &val1, &val2, &val3, &val4, &val5, &val6) != 6 ) {
		printm("malformed mac address %s\n", s );
		return;
	}
	destaddr[0] = val1;
	destaddr[1] = val2;
	destaddr[2] = val3;
	destaddr[3] = val4;
	destaddr[4] = val5;
	destaddr[5] = val6;
}

static void
print_ip( const char *str, u32 ip )
{
	printm("%s%d.%d.%d.%d", str, (u8)(ip>>24), (u8)(ip>>16), (u8)(ip>>8), (u8)ip );
}

void
netif_print_blurb( enet_iface_t *is, int port )
{
	unsigned char *p = is->c_macaddr;
	
	printm("Ethernet Interface ");
	if( port >= 0 )
		printm("(port %d) ", port );
	printm("'%s-<%s>' @ %02X:%02X:%02X:%02X:%02X:%02X",
	       is->pd->name, is->iface_name, p[0], p[1], p[2], p[3], p[4], p[5]);
	if( is->flags & NO_DHCP )
		printm("  [nodhcp]\n\n");
	else {
		print_ip("\n\n    ip/mask: ", is->client_ip );
		print_ip("/", is->netmask );
		print_ip("  gw: ", is->gateway );
		print_ip("\n    broadcast: ", is->broadcast_ip );
		print_ip("  nameserver: ", is->nameserver );
		printm("\n\n");
	}
}

static void
preconfigure( enet_iface_t *is )
{
	unsigned char s_macaddr[] = { 0x00, 0x03, 0x93, 0xd4, 0x32, 0xe5 };

	int net = 0xc0a82800 + (g_session_id << 8);	/* 192.168.40+sess_id.0 */

	is->netmask = 0xffffff00;
	is->broadcast_ip = net | ~is->netmask;
	is->client_ip = net | 2;
	is->my_ip = net | 1;
	is->nameserver = net | 1;
	is->gateway = net | 1;
	memcpy( is->s_macaddr, s_macaddr, 6 );

	/* let packet driver preconfigure things */
	if( is->pd->preconfigure )
		(*is->pd->preconfigure)( is );
}

int
find_packet_driver( int index, enet_iface_t *is )
{
	const char res_name[] = "netdev";
	packet_driver_t *pd;
	char *s, *macaddr;
	u32 *pp;
	int i;
	
	/* initialize once */
	if( !index )
		initialize_drivers();
	
	if( !(s=get_str_res_ind(res_name, index, 0)) )
		return 1;
	strncpy0( is->iface_name, s, sizeof(is->iface_name) );

	/* find packet driver */
	for( pd=NULL, i=1; !pd && (s=get_str_res_ind(res_name, index, i)); i++ )
		for( pd=g_packet_driver_list ; pd && strcasecmp(pd->flagstr,s) ; pd=pd->_next )
			;
	if( !pd ) {
		int id = (is->iface_name[0] == 't')? TUN_PACKET_DRIVER_ID : SHEEP_PACKET_DRIVER_ID;
		for( pd=g_packet_driver_list ; pd && pd->id != id ; pd=pd->_next )
			;
		if( !pd ) {
			printf("---> selected network driver unsupported\n");
			return -1;
		}
	}
	is->pd = pd;

	preconfigure( is );

	/* handle user options */
	macaddr = NULL;
	for( pp=NULL, i=1; (s=get_str_res_ind(res_name, index, i)); i++ ) {
		if( pp ) {
			*pp = lookup_ip( s );
			pp = NULL;
			continue;
		}
		if( macaddr ) {
			lookup_macaddr( macaddr, s );
			macaddr = NULL;
			continue;
		}
		if( s[0] == '-' )
			continue;
		if( !strcasecmp(s, "ip") )
			pp = &is->client_ip;
		else if( !strcasecmp(s, "netmask") )
			pp = &is->netmask;
		else if( !strcasecmp(s, "nameserver") )
			pp = &is->nameserver;
		else if( !strcasecmp(s, "broadcast") )
			pp = &is->broadcast_ip;
		else if( !strcasecmp(s, "gateway") || !strcasecmp(s, "gw") )
			pp = &is->gateway;
		else if( !strcasecmp(s, "nodhcp") )
			is->flags |= NO_DHCP;
		else if( !strcasecmp(s, "dhcp") )
			is->flags &= ~NO_DHCP;
		else if( !strcasecmp(s, "macaddr") )
			macaddr = (char *) is->c_macaddr;
		else {
			printm("Unknown networking option '%s' ignored\n", s );
			continue;
		}
	}
	return 0;
}
