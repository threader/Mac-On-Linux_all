/* 
 *   Creation Date: <1999/05/23 00:00:00 BenH>
 *   
 *	<mac_enet.c>
 *	
 *   	OSI Ethernet interface
 *   
 *   Copyright (C) 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   Copyright (C) 1999, 2000, 2001 Benjamin Herrenschmidt (bh40@calva.net)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/uio.h>
#include "poll_compat.h"
//#include "verbose.h"
#include "os_interface.h"
#include "osi_driver.h"
#include "memory.h"
#include "driver_mgr.h"
#include "session.h"
#include "pic.h"
#include "async.h"
#include "res_manager.h"
#include "pci.h"
#include "booter.h"
#include "enet.h"
#include "mac_sh.h"

#define MAX_ENET_IFS			2
#define RX_QUEUE_SIZE			32

#define ETH_ADDR_MULTICAST		0x1
#define ETH_ADDR_LOCALLY_DEFINED	0x2

typedef struct rx_qentry {
	int		 p_size;			/* including pad bytes */
	unsigned char 	 packet[PACKET_BUF_SIZE];
	struct rx_qentry *next;
	struct rx_qentry *prev;
} rx_qentry_t;

typedef struct {
	enet_iface_t	iface;				/* must go first */
	
	struct osi_driver *osi_driver;
	int		async_id;

	rx_qentry_t 	*rx_qhead;
	rx_qentry_t	*rx_qtail;
	rx_qentry_t	*free_head;
	char		*queue_space;

	int		irq;
	int		started;
} mac_enet_t;

#define PD		(*is->iface.pd)
#define IFACE		is->iface

static int 		numifs;
static mac_enet_t	enetif[MAX_ENET_IFS];

static char			*bad_access_str = "mac_enet: Bad access";
#define BAD_ACCESS_MSG()	printm( "%s\n", bad_access_str )
#define BAD_ACCESS_CHECK(addr)	do { if( !addr ) { BAD_ACCESS_MSG() ; return -1; } } while(0)


/************************************************************************/
/*	Session Save Support						*/
/************************************************************************/

static int
save_enet_state( void )
{
	mac_enet_t *is=enetif;
	int i;

	for(i=0; i<numifs; i++, is++ ) {
		if( PD.load_save_state && PD.load_save_state(&is->iface, NULL, i, 0) )
			return -1;
		if( write_session_data( "enet", i, (char*)is, sizeof(mac_enet_t) ))
			return -1;
	}
	return 0;
}

static void
load_enet_state( void )
{
	mac_enet_t lis, *is = enetif;
	int i;

	for( i=0; i<MAX_ENET_IFS; i++, is++ ){
		if( read_session_data( "enet", i, (char*)&lis, sizeof(lis) ) )
			break;
		if( PD.load_save_state && PD.load_save_state(&is->iface, &lis.iface, i, 1) )
			session_failure("Unexpected error in load_save_state hook\n");

		is->started = lis.started;
	}
}


/************************************************************************/
/*	OSI Interface							*/
/************************************************************************/

#define ENETIF_FROM_ID( id ) ({ if( (id)<0 || (id)>=numifs ) return -1; &enetif[(id)]; })

static int
osip_open( int sel, int *params )
{
	struct osi_driver *d = get_osi_driver_from_id( params[0] );
	int i;
	
	for( i=0; d && i<numifs && enetif[i].osi_driver != d; i++ )
		;
	if( i >= numifs )
		return -1;
	return i;
}

static int
osip_close( int sel, int *params )
{
	return 0;
}

static int
osip_get_address( int sel, int *params )
{
	mac_enet_t *is = ENETIF_FROM_ID( params[0] );
	unsigned char *address = transl_mphys( params[1] );
	int i;

	BAD_ACCESS_CHECK( address );

	/* XXX: This is more easily done by returning values in several registers 
	 * - avoids the necessity to pass locked, physically continuous memory. 
	 */
	for( i=0; i<6; i++ )
		address[i] = IFACE.c_macaddr[i];
	return 0;
}

static int
osip_get_status( int sel, int *params )
{
	mac_enet_t *is = ENETIF_FROM_ID( params[0] );
	int result;
		
	result = 0;
	if( is->started && is->rx_qhead ) {
		result |= ((is->rx_qhead->p_size - IFACE.packet_pad) & 0xFFFFUL);
		result |= kEnetStatusRxPacket;
	}
	return result;
}

static int
osip_control( int sel, int *params )
{
	mac_enet_t *is = ENETIF_FROM_ID( params[0] );
	int command = params[1];

	switch( command ) {
	case kEnetAckIRQ:
		return irq_line_low( is->irq );

	case kEnetCommandStart:
		is->started = 1;
		set_async_handler_events( is->async_id, POLLIN | POLLPRI );
		break;

	case kEnetCommandStop:
		is->started = 0;
		set_async_handler_events( is->async_id, 0 );
		irq_line_low( is->irq );
		
		/* flush queue */
		if( is->rx_qtail ) {
			is->rx_qtail->next = is->free_head;
			is->free_head = is->rx_qhead;
			is->rx_qtail = NULL;
			is->rx_qhead = NULL;
		}
		break;

	case kEnetRouteIRQ:
		oldworld_route_irq( params[2], &is->irq, "mac_enet" );
		break;
	}
	return 0;
}

static int
osip_get_packet( int sel, int *params )
{
	mac_enet_t *is = ENETIF_FROM_ID( params[0] );
	unsigned char *address = transl_mphys( params[1] );	
	rx_qentry_t *pkg;
	
	if ( !is->rx_qhead )
		return -1;
	
	/* copy and free package */
	pkg = is->rx_qhead;
	if( params[1] ) {
		if( !address )
			BAD_ACCESS_MSG();
		else
			memcpy(address, pkg->packet + IFACE.packet_pad, pkg->p_size - IFACE.packet_pad );
	}
	is->rx_qhead = is->rx_qhead->next;
	if( !is->rx_qhead )
		is->rx_qtail = NULL;
	pkg->next = is->free_head;
	is->free_head = pkg;

	return 0;
}

static int
osip_send_packet( int sel, int *params )
{
	mac_enet_t *is = ENETIF_FROM_ID( params[0] );
	unsigned char *address = transl_mphys( params[1] );	
	int size = params[2];
	struct iovec vec;
	
	BAD_ACCESS_CHECK( address );

	if( (uint)size > MAX_PACKET_SIZE ) {
		printm("tx-packet too big\n");
		return -1;
	}

	vec.iov_len = size;
	vec.iov_base = address;
	if( send_packet(&is->iface, &vec, 1) )
		return -1;
	return 0;
}

static int
osip_add_multicast( int sel, int *params )
{
	mac_enet_t *is = ENETIF_FROM_ID( params[0] );
	unsigned char *address = transl_mphys( params[1] );

	BAD_ACCESS_CHECK( address );

	if( PD.add_multicast )
		return (PD.add_multicast)( &IFACE, (char *)address );	
	return 0;
}

static int
osip_del_multicast( int sel, int *params )
{
	mac_enet_t *is = ENETIF_FROM_ID( params[0] );
	unsigned char *address = transl_mphys( params[1] );

	BAD_ACCESS_CHECK( address );

	if( PD.del_multicast )
		return PD.del_multicast( &IFACE, (char *)address );
	return 0;
}


/************************************************************************/
/*	RX Packet Handler						*/
/************************************************************************/

#if 0
/* space for the ethernet header should be included (but this
 * function will initialize it properly).
 */
int
synthesize_eth_packet( char *ethpacket, int size, void *iface )
{
	mac_enet_t 	*is = (mac_enet_t*)iface;
	rx_qentry_t 	*pkg;
	char 		*buf;
	
	// printm("Got synthesized packet size %d\n", size );

	if( size > MAX_PACKET_SIZE ) {
		printm("synthesize_ip_packet: Too big packet detected (%d)!\n", size);
		return -1;
	}
	if( !is->free_head ) {
		printm("No free buffers!\n");
		return -1;
	}
	buf = is->free_head->packet + IFACE.packet_pad;
	memcpy( buf, ethpacket, size );
	memcpy( buf, &IFACE.c_macaddr, 6 );

	// Queue packet
	pkg = is->free_head;
	is->free_head = is->free_head->next;

	pkg->p_size = size;
	pkg->next = NULL;

	if( is->rx_qtail ){
		pkg->prev = is->rx_qtail;
		is->rx_qtail->next = pkg;
		is->rx_qtail = pkg;
	} else {
		is->rx_qtail = pkg;
		is->rx_qhead = pkg;
	}
	irq_line_hi( is->irq );
	return 0;
}
#endif

static void
queue_rx_packet( mac_enet_t *is, rx_qentry_t *pkg )
{
	pkg->next = NULL;

	if( is->rx_qtail ) {
		pkg->prev = is->rx_qtail;
		is->rx_qtail->next = pkg;
		is->rx_qtail = pkg;
	} else {
		is->rx_qtail = pkg;
		is->rx_qhead = pkg;
	}
	irq_line_hi( is->irq );
}

static void 
rx_packet_handler( int fd, int events )
{
	char discardbuf[PACKET_BUF_SIZE];
	rx_qentry_t *pkg;
	char *buf;
	int i, size;
	mac_enet_t *is;
	struct iovec iov;
	
	for( is=&enetif[0], i=0; i<numifs && IFACE.fd != fd ; i++, is++ )
		;
	if( i >= numifs ) {
		printm("rx_packet_handler: bad fd\n");
		return;
	}

	if( !(events & POLLIN) )
		printm("rx_packet_handler called with events %08X\n", events);

	for( ;; ) {
		if( is->free_head )
			buf = (char *) is->free_head->packet;
		else {
			static int warned=0;
			if( ++warned < 10 )
				printm("WARNING: Ethernet packet dropped\n");
			buf = discardbuf;
		}

		/* read packet from device */
		iov.iov_base = buf;
		iov.iov_len = PACKET_BUF_SIZE;
		if( (size=receive_packet(&IFACE, &iov, 1)) < 0 ) {
			if( errno == EAGAIN )
				break;
			else
				perrorm("packet read");
		}
		
		if( size < 14 + IFACE.packet_pad || !is->started || !is->free_head )
			continue;

		if( !(buf[0+IFACE.packet_pad] & ETH_ADDR_MULTICAST) && memcmp( &IFACE.c_macaddr, buf+IFACE.packet_pad, 6) ) {
			// printm("Dropping packet with wrong HW-addr\n");
			continue;
		}
		/* Queue packet */

		// printm("Got packet size %d\n", size );
		pkg = is->free_head;
		is->free_head = is->free_head->next;

		/* Truncate packets containing more than 1514 bytes */
		if( size > IFACE.packet_pad + MAX_PACKET_SIZE )
			size = IFACE.packet_pad + MAX_PACKET_SIZE;

		pkg->p_size = size;
		queue_rx_packet( is, pkg );
	}
}

static void
inject_packet( enet_iface_t *iface, const char *buf, int len )
{
	mac_enet_t *is = (mac_enet_t*)iface;
	rx_qentry_t *pkg = is->free_head;

	if( !pkg )
		return;
	is->free_head = is->free_head->next;

	if( len > MAX_PACKET_SIZE )
		len = MAX_PACKET_SIZE;
	memcpy( pkg->packet + IFACE.packet_pad, buf, len );

	pkg->p_size = len + IFACE.packet_pad;
	queue_rx_packet( is, pkg );
}

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static int 
add_netdev( mac_enet_t *is )
{
	static pci_dev_info_t enet_pci_config = { 0x6666, 0x7777, 0x00, 0x0, 0x0200 };
	char name[20];
	int i;

	is->iface.inject_packet = inject_packet;

	if( PD.open(&IFACE) ) {
		printm("Failed to initialize the %s-<%s> device\n", PD.name, IFACE.iface_name);
		return 1;
	}

	snprintf( name, sizeof(name), "mol-enet-%d", numifs );
	if( !(is->osi_driver=register_osi_driver("enet", name, &enet_pci_config, &is->irq)) ) {
		printm("Could not register the ethernet driver\n");
		return 1;
	}
	
	/* allocate rx queue elements */
	is->queue_space = calloc( RX_QUEUE_SIZE, sizeof(rx_qentry_t) );
	for( i=0; i<RX_QUEUE_SIZE; i++ ) {
		rx_qentry_t *pkg = &((rx_qentry_t*)is->queue_space)[i];
		pkg->next = is->free_head;
		is->free_head = pkg;
	}

	if( (is->async_id=add_async_handler(IFACE.fd, 0, rx_packet_handler, 0)) < 0 )
		printm("mac_enet: Could not add async handler!\n");

	is->started = 0;
	netif_print_blurb( &IFACE, numifs+1 );

	numifs++;
	return 0;
}

static int
mac_enet_init( void )
{
	int i;
	
	numifs = 0;
	memset( (char*)enetif, 0, sizeof(enetif) );

	if( !is_classic_boot() )
		return 0;

	for( i=0; numifs < MAX_ENET_IFS; i++ ) {
		mac_enet_t *is = &enetif[numifs];
		memset( is, 0, sizeof(*is) );		

		if( find_packet_driver(i, &is->iface) )
			break;
		add_netdev( is );
	}
	if( !numifs )
		return 0;
	
	os_interface_add_proc( OSI_ENET_OPEN, osip_open );
	os_interface_add_proc( OSI_ENET_CLOSE, osip_close );
	os_interface_add_proc( OSI_ENET_GET_ADDR, osip_get_address );
	os_interface_add_proc( OSI_ENET_GET_STATUS, osip_get_status );
	os_interface_add_proc( OSI_ENET_CONTROL, osip_control );
	os_interface_add_proc( OSI_ENET_GET_PACKET, osip_get_packet );
	os_interface_add_proc( OSI_ENET_SEND_PACKET, osip_send_packet );
	os_interface_add_proc( OSI_ENET_ADD_MULTI, osip_add_multicast );
	os_interface_add_proc( OSI_ENET_DEL_MULTI, osip_del_multicast );

	session_save_proc( save_enet_state, NULL, kDynamicChunk );
	if( loading_session() )
		load_enet_state();
	return 1;
}

static void
mac_enet_cleanup( void )
{
	int i;

	for( i=0; i<numifs; i++ ) {
		mac_enet_t *is = &enetif[i];

		delete_async_handler( is->async_id );
		PD.close( &IFACE );

		if( is->queue_space )
			free( is->queue_space );
		free_osi_driver( is->osi_driver );
	}
	numifs = 0;
}

driver_interface_t mac_enet_driver = {
    "mac_enet", mac_enet_init, mac_enet_cleanup
};
