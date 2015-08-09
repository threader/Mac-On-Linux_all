/* 
 *   Creation Date: <2002/05/20 17:39:20 samuel>
 *   Time-stamp: <2004/04/03 16:45:30 samuel>
 *   
 *	<osi_enet2.c>
 *	
 *	Ring buffer based ethernet driver
 *   
 *   Copyright (C) 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/uio.h>
#include "poll_compat.h"
#include "os_interface.h"
#include "res_manager.h"
#include "driver_mgr.h"
#include "async.h"
#include "promif.h"
#include "pic.h"
#include "memory.h"
#include "mac_registers.h"
#include "enet.h"
#include "enet2_sh.h"
#include "osi_driver.h"

typedef struct {
	enet_iface_t	iface;
	int		rx_async;	/* rx async handler id */
	int		running;

	int		irq;
	int		irq_enable;

	enet2_ring_t	*tx_ring;	/* 2^n entries */
	enet2_ring_t	*tx_of_ring;	/* used when a buffer crosses a page boundary */
	int		tx_head;
	int		tx_mask;	/* index mask for tx ring */

	enet2_ring_t	*rx_ring;
	enet2_ring_t	*rx_of_ring;	/* used when a buffer crosses a page boundary */
	int		rx_tail;
	int		rx_mask;
} mol_enet2_t;

static mol_enet2_t *me;

#define PD	(*me->iface.pd)
#define IFACE	me->iface

static inline void
dbg_dump_packet( const char *str, struct iovec *vec, int nvec )
{
#ifdef CONFIG_DUMP_PACKETS
	int i, s=0;

	for( i=0; i<nvec; i++ )
		s += vec[i].iov_len;
	printm("%s " C_YELLOW "(%d bytes)" C_NORMAL " [", str, s );

	if( s > 64 )
		s = 64;
	for( i=0; i<s ; i++ ) {
		if( !(i&31) )
			printm("\n  ");
		printm("%02x ", iovec_getbyte(i, vec, nvec) );
	}
	printm("]\n");
#endif
}


/************************************************************************/
/*	OSI Interface							*/
/************************************************************************/

static int
osip_enet2_open( int sel, int *params )
{
	//printm("osip_enet2_open\n");
	return 0;
}

static int
osip_enet2_close( int sel, int *params )
{
	//printm("osip_enet2_close\n");
	me->running = 0;
	return 0;
}

/* osip_enet2_ring_setup( int ring, int ring_mphys, int n_el ) */
static int
osip_enet2_ring_setup( int sel, int *params )
{
	enet2_ring_t *r = transl_mphys( params[1] );

	if( me->running ) {
		printm("enet2 busy!\n");
		return -1;
	}
	if( !r )
		return -1;

	switch( params[0] ) {
	case kEnet2SetupRXRing:
		me->rx_ring = r;
		me->rx_tail = 0;
		me->rx_mask = params[2] - 1;
		break;
	case kEnet2SetupTXRing:
		me->tx_ring = r;
		me->tx_head = 0;
		me->tx_mask = params[2] - 1;
		break;
	case kEnet2SetupRXOverflowRing:
		me->rx_of_ring = r;
		break;
	case kEnet2SetupTXOverflowRing:
		me->tx_of_ring = r;
		break;
	}
	return 0;
}

static int
osip_enet2_get_hwaddr( int sel, int *params )
{
	memcpy( &mregs->gpr[4], &IFACE.c_macaddr, 6 );
	return 0;
}

/* int osip_enet2_irq_ack( int irq_enable, int rx_index ) */
static int
osip_enet2_irq_ack( int sel, int *params )
{
	int ret, irq_enable = params[0];
	int ind = params[1];
	
	me->irq_enable = irq_enable;
	
	/* If the queue is empty, return 0 (the next packet
	 * will raise an interrupt).
	 */
	ret = me->running && me->rx_ring[ind & me->rx_mask ].psize;
	irq_line_low( me->irq );

	return ret;
}

static int
osip_enet2_cntrl( int sel, int *params )
{
	switch( params[0] ) {
	case kEnet2CheckAPI:
		return params[1] == OSI_ENET2_API_VERSION ? 0 : -1;
		break;
	case kEnet2OSXDriverVersion:
		set_driver_ok_flag( kOSXEnetDriverOK, params[1] == OSX_ENET_DRIVER_VERSION );
		break;
	case kEnet2Start:
		//printm("kEnet2Start\n");
		if( !me->rx_ring || !me->tx_ring )
			return 1;
		me->running = 1;
		me->irq_enable = 1;
		break;
	case kEnet2Reset:
		//printm("kEnet2Reset\n");
		me->tx_head = me->rx_tail = 0;
		/* fall through */
	case kEnet2Stop:
		//printm("kEnet2Stop\n");
		irq_line_low( me->irq );
		me->running = 0;
		me->irq_enable = 0;
		break;
	default:
		printm("osip_enet2: Unknown control request\n");
		return 1;
	}
	return 0;
}

static int
osip_enet2_kick( int sel, int *params )
{
	struct iovec vec[2];

	if( !me->running )
		return 1;

	for( ;; ) {
		enet2_ring_t *r = me->tx_ring + me->tx_head;
		int nvec = 1;
		
		if( !r->psize )
			break;

		vec[0].iov_len = r->psize;
		if( !(vec[0].iov_base=transl_mphys(r->buf)) )
			goto fault;

		if( (r->flags & kEnet2SplitBufferFlag) ) {
			enet2_ring_t *r2 = me->tx_of_ring + me->tx_head;

			vec[1].iov_len = r2->psize;
			if( !(vec[1].iov_base=transl_mphys(r2->buf)) )
				goto fault;
			nvec = 2;
		}
		dbg_dump_packet(C_RED "OUTGOING PACKET", vec, nvec );
		send_packet( &me->iface, vec, nvec );

		me->tx_head = (me->tx_head + 1) & me->tx_mask;
		r->psize = 0;
	}
	return 0;
 fault:
	printm("enet2_kick: Bad Access\n");
	me->running = 0;
	return 1;
}

static const osi_iface_t osi_iface[] = { 
	{ OSI_ENET2_OPEN,	osip_enet2_open		},
	{ OSI_ENET2_CLOSE,	osip_enet2_close	},
	{ OSI_ENET2_CNTRL,	osip_enet2_cntrl	},
	{ OSI_ENET2_RING_SETUP,	osip_enet2_ring_setup	},
	{ OSI_ENET2_KICK,	osip_enet2_kick		},
	{ OSI_ENET2_GET_HWADDR,	osip_enet2_get_hwaddr	},
	{ OSI_ENET2_IRQ_ACK,	osip_enet2_irq_ack	},
};


/************************************************************************/
/*	Packet Receiver							*/
/************************************************************************/

static int
prepare_iovec( enet2_ring_t *r, struct iovec (*vec)[2], int pad )
{
	int nvec = 1;
	char *buf;

	if( !r->bsize || !(buf=transl_mphys(r->buf)) )
		goto fault;
	(*vec)[0].iov_len = r->bsize + pad;
	(*vec)[0].iov_base = buf - pad;
	
	if( (r->flags & kEnet2SplitBufferFlag) ) {
		enet2_ring_t *r2 = me->rx_of_ring + me->rx_tail;
		
		if( !(buf=transl_mphys(r2->buf)) )
			goto fault;
		
		(*vec)[1].iov_len = r2->bsize;
		(*vec)[1].iov_base = buf;
		nvec = 2;
	}
	return nvec;
 fault:
	me->running = 0;
	printm("rx_packet_error\n");
	return -1;
}

static void
rx_packet_handler( int fd, int events )
{
	char dummy;
	int n = 0;

	if( !me->running ) {
		while( read(fd, &dummy, sizeof(dummy)) >= 0 )
			;
		return;
	}
	for( ;; ) {
		enet2_ring_t *r = me->rx_ring + me->rx_tail;
		struct iovec vec[2];
		int s, nvec;

		if( r->psize ) {
			/* ring full, dropping packets */
			printm("rx ring full\n");
			while( read(fd, &dummy, sizeof(dummy)) >=0 )
				n++;
			break;
		}
		if( (nvec=prepare_iovec(r, &vec, IFACE.packet_pad)) <= 0 )
			break;
		if( (s=receive_packet(&IFACE, vec, nvec)) <= 0 )
			break;
		dbg_dump_packet(C_GREEN "INCOMING_PACKET", vec, nvec );

		r->psize = s - IFACE.packet_pad;
		me->rx_tail = (me->rx_tail+1) & me->rx_mask;
		n++;
	}
	if( n && me->irq_enable )
		irq_line_hi( me->irq );
}

static void
inject_packet( enet_iface_t *iface, const char *packet, int len )
{
	enet2_ring_t *r = me->rx_ring + me->rx_tail;
	struct iovec vec[2];
	int nvec;
	
	if( !me->running || r->psize || (nvec=prepare_iovec(r,&vec,0)) <= 0 )
		return;
	
	r->psize = memcpy_tovec( vec, nvec, packet, len );
	dbg_dump_packet(C_YELLOW "INJECTED_PACKET", vec, nvec );

	me->rx_tail = (me->rx_tail+1) & me->rx_mask;
	irq_line_hi( me->irq );
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static int
add_netdev( int index, int irq )
{
	me = malloc( sizeof(mol_enet2_t) );
	memset( me, 0, sizeof(mol_enet2_t) );

	if( find_packet_driver(index, &me->iface) )
		goto bail;

	me->running = 0;
	me->irq = irq;
	me->iface.inject_packet = inject_packet;

	if( PD.open(&IFACE) ) {
		printm("Failed to initialize the %s-<%s> device\n", PD.name, IFACE.iface_name );
		goto bail;
	}
	netif_print_blurb( &me->iface, -1 );
	
	if( (me->rx_async=add_async_handler(IFACE.fd, POLLIN | POLLPRI, rx_packet_handler,0)) < 0 )
		printm("add_async_handler failed!\n");
	return 0;

bail:
	free( me );
	me = NULL;
	return 1;
}

static int
enet2_init( void )
{
	mol_device_node_t *dn;
	irq_info_t irqinfo;
	
	if( !(dn=prom_find_devices("mol-enet")) )
		return 0;
	if( prom_irq_lookup(dn, &irqinfo) != 1 )
		return 0;

	if( add_netdev(0, irqinfo.irq[0]) ) {
		prom_delete_node( dn );
		return 0;
	}
	register_osi_iface( osi_iface, sizeof(osi_iface) );
	return 1;
}

static void
enet2_cleanup( void )
{
	if( me ) {
		delete_async_handler( me->rx_async );
		
		PD.close( &IFACE );
		free( me );
	}
}

driver_interface_t enet2_driver = {
	"enet2", enet2_init, enet2_cleanup
};
