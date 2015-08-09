/* 
 *   Creation Date: <2002/05/22 22:09:00 samuel>
 *   Time-stamp: <2004/04/03 16:49:08 samuel>
 *   
 *	<enet.h>
 *	
 *	Network interfaces
 *   
 *   Copyright (C) 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_ENET
#define _H_ENET

typedef struct packet_driver packet_driver_t;
typedef struct enet_iface enet_iface_t;

/* id field */
#define TUN_PACKET_DRIVER_ID		1
#define TAP_PACKET_DRIVER_ID		2
#define SHEEP_PACKET_DRIVER_ID		4

/* enet iface flags */
#define NO_DHCP				1
#define HAS_CUSTOM_MACADDR		2
#define IP_PAYLOAD			4	/* no link layer */

struct enet_iface {
	packet_driver_t	*pd;

	int		fd;
	int		packet_pad;		/* #bytes before the eth packet */

	char		iface_name[16];
	unsigned char	c_macaddr[6];		/* client mac address */
	unsigned char	s_macaddr[6];		/* system mac address */

	u32		client_ip;		/* for DHCP */
	u32		broadcast_ip;		/* for DHCP */
	u32		my_ip;			/* for DHCP, ip of machine MOL is running on */
	u32		netmask;		/* for DHCP */
	u32		nameserver;		/* for DHCP */
	u32		gateway;		/* for DHCP */

	int		flags;
	ulong		ipfilter;		/* sheep private, for save/load state only */

	void		(*inject_packet)( enet_iface_t *is, const char *addr, int len );
};

struct packet_driver {
	const char	*name;
	const char	*flagstr;
	int		id;			/* XXX_PACKET_DRIVER_ID */

	void		(*preconfigure)( enet_iface_t *is );
	int		(*open)( enet_iface_t *is );
	void		(*close)( enet_iface_t *is );
	int		(*add_multicast)( enet_iface_t *is, char *addr );
	int		(*del_multicast)( enet_iface_t *is, char *addr );
	int		(*load_save_state)( enet_iface_t *is, enet_iface_t *load_is, int index, int loading );
	
	packet_driver_t	*_next;
};

extern packet_driver_t	*g_packet_driver_list;

/* initialization */
extern void		init_tun( void );
extern void		init_sheep( void );

#define DECLARE_PACKET_DRIVER( initname, pd ) \
void			initname( void ) { pd._next = g_packet_driver_list; g_packet_driver_list = &pd; }

/* packet driver helper functions */
extern void		netif_open_common( enet_iface_t *is, int fd );
extern void		netif_close_common( enet_iface_t *is );
extern int		check_netdev( const char *ifname );

/* both of these might modify the iovec */
extern int		send_packet( enet_iface_t *iface, struct iovec *vec, size_t nvec );
extern int		receive_packet( enet_iface_t *iface, struct iovec *vec, size_t nvec );

/* world interface */
extern int		find_packet_driver( int index, enet_iface_t *is );
extern void		netif_print_blurb( enet_iface_t *is, int port );

#define TAP_PACKET_PAD		2		/* used by read & writes */
#define MAX_PACKET_SIZE		1514
#define PACKET_BUF_SIZE		(1514 + TAP_PACKET_PAD)

#endif   /* _H_ENET */
