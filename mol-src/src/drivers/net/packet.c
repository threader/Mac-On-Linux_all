/* 
 *   Creation Date: <2004/03/30 13:21:25 samuel>
 *   Time-stamp: <2004/06/12 22:19:36 samuel>
 *   
 *	<dhcp.c>
 *	
 *	Simple DHCP server
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */

#include "mol_config.h"
#include <sys/uio.h>
#include "enet.h"
#include "ip.h"
#include "dhcp.h"
#include "res_manager.h"


/************************************************************************/
/*	simple DHCP/BOOTP server					*/
/************************************************************************/

#define MAX_SIZE	1514		/* maximal reply size */

static char *
int_reply( char *dest, int opcode, unsigned int value )
{
	*dest++ = opcode;
	*dest++ = 4;
	*dest++ = value >> 24;
	*dest++ = value >> 16;
	*dest++ = value >> 8;
	*dest++ = value;
	return dest;
}

static char *
string_reply( char *dest, int opcode, const char *str )
{
	int len = strlen(str)+1;
	*dest++ = opcode;
	*dest++ = len;
	strcpy( dest, str );
	return dest + len;
}

#if 0
static char *
buf_reply( char *dest, int opcode, const char *buf, int len )
{
	*dest++ = opcode;
	*dest++ = len;
	memcpy( dest, buf, len );
	return dest + len;
}
#endif

static char *
byte_reply( char *dest, int opcode, int val )
{
	*dest++ = opcode;
	*dest++ = 1;
	*dest++ = val;
	return dest;
}

static void
print_dhcp_tag( int tag, const char *data, int len )
{
#ifdef CONFIG_DHCP_DEBUG
	int j;
	printm("DHCP option: %d (len %d)   ", tag, len );
	for( j=0; j<len; j++ )
		printm("%02x ", data[j] );
	printm("\n");
#endif
}

static int
handle_dhcp( enet_iface_t *is, const struct iovec *vec, size_t nvec )
{
	int pad, size, dhcp_type=-1;
	int reply_type, bad_ip = 0;
	eth_header_t *lp;
	bootp_packet_t *bp;
	udp_header_t *udp;
	ip_header_t *ipp;
	unsigned char *dest, *p;
	
	if( !(p=malloc(MAX_SIZE)) )
		return 0;

	memset( p, 0, MAX_SIZE );
	memcpy_fromvec( (char *)p, vec, nvec, MAX_SIZE );

	lp = (eth_header_t*)p;
	ipp = (ip_header_t*)(p + sizeof(eth_header_t));
	udp = (udp_header_t*)(p + (ipp->hlen * 4) + sizeof(eth_header_t));
	bp = (bootp_packet_t*)&udp->data;

	/* fix link level header */
	memcpy( lp->dmac, lp->smac, 6 );
	memcpy( lp->smac, is->s_macaddr, 6 );

	/* fix ip header */
	ipp->daddr = is->client_ip;			/* destination address */
	ipp->saddr = is->my_ip;
	ipp->tos = 0;
	ipp->id *= 0x799;

	/* fix UDP header */
	udp->sport = 67;
	udp->dport = 68;
	udp->checksum = 0;

	if( bp->option_magic == 0x63825363 ) {
		int i=0;
		while( i < MAX_SIZE ) {
			int len, tag = bp->options[i++];
			if( !tag )
				continue;
			if( tag == 255 )
				break;
			len = bp->options[i++];

			/* handle type tag */
			if( tag == DHCPO_DHCP_MSG_TYPE )
				dhcp_type = bp->options[i];
			if( tag == DHCPO_REQ_IP_ADDR ) {
				u32 req_ip = (bp->options[i] << 24) | (bp->options[i+1] << 16) |
					(bp->options[i+2] << 8) | bp->options[i+3];
				if( req_ip != is->client_ip )
					bad_ip = 1;
			}

			print_dhcp_tag( tag, (char *) &bp->options[i], len );
			i += len;
		}
	}

	/* construct reply */
	switch( dhcp_type ) {
	case DHCPDISCOVER:
	case -1: /* might be old style bootp */
		reply_type = DHCPOFFER;
		break;
	case DHCPINFORM:
		ipp->daddr = bp->ciaddr;
		bp->yiaddr = 0;				/* should be cleared */
		reply_type = DHCPACK;
		break;
	case DHCPREQUEST:
		reply_type = bad_ip ? DHCPNACK : DHCPACK;
		break;
	case DHCPDECLINE:
	case DHCPRELEASE:
		goto drop;
	default:
		printm("Unexpected DHCP request %d\n", dhcp_type );
		goto drop;
	}
	bp->option_magic = 0x63825363;
	dest = bp->options;
	dest = (unsigned char *) byte_reply( (char *) dest, 53, reply_type );
	dest = (unsigned char *) int_reply( (char *) dest, DHCPO_SERVER_ID, is->my_ip );
	if( reply_type != DHCPNACK ) {
		if( reply_type == DHCPACK )
			printm("DHCP lease: %d.%d.%d.%d\n", (u8)(is->client_ip >> 24), (u8)(is->client_ip >>16),
			       (u8)(is->client_ip >>8), (u8)is->client_ip );
		if( dhcp_type == DHCPDISCOVER || dhcp_type == DHCPREQUEST )
			dest = (unsigned char *) int_reply( (char *) dest, DHCPO_IP_LEASE_TIME, 0xffffffff );
		dest = (unsigned char *) int_reply( (char *) dest, DHCPO_BROADCAST_ADDR, is->broadcast_ip );
		dest = (unsigned char *) int_reply( (char *) dest, DHCPO_SUBNET_MASK, is->netmask );
		dest = (unsigned char *) int_reply( (char *) dest, DHCPO_ROUTERS, is->gateway );
		// dest = (unsigned char *) int_reply( (char *) dest, NAME_SERVER, net | 0x1 );	/* name server */
		dest = (unsigned char *) int_reply( (char *) dest, DHCPO_DNS_SERVER, is->nameserver );
		dest = (unsigned char *) string_reply( (char *) dest, DHCPO_DOMAIN_NAME, "localdomain" );
	}
	*dest++ = DHCPO_END;
	
	/* pad (is this necessary?) */
	pad = 60 - (dest - bp->options);
	if( pad > 0 ) {
		memset( dest, 0, pad );
		dest += pad;
	}
	size = dest - p;

	/* fix bootp reply */
	bp->opcode = 2;				/* BOOTREPLY */
	/* bp->htype unmodified */
	/* bp->hlen unmodified */
	bp->hops = 0;
	/* bp->xid unmodified */
	bp->secs = 0;
	/* bp->flags unmodified */
	bp->ciaddr = 0;
	bp->yiaddr = is->client_ip;
	bp->siaddr = is->my_ip;
	/* bp->giaddr unmodified */

	/* finish and transmit */
	ipp->totlen = size - 14;
	udp->len = ipp->totlen - 4*ipp->hlen;
	ipp->checksum = 0;
	ipp->checksum = ip_fast_csum( (u8*)ipp, ipp->hlen );
	is->inject_packet( is, (char *) p, dest-p );
 drop:
	free( p );
	return 1;
}


/************************************************************************/
/*	ARP handling							*/
/************************************************************************/

#define ARP_PACKET_SIZE 	(sizeof(eth_header_t) + sizeof(arp_request_t))

static int
handle_arp( enet_iface_t *is, const struct iovec *vec, size_t nvec )
{
	char *p = malloc( ARP_PACKET_SIZE );
	arp_request_t *arp = (arp_request_t*)(p + sizeof(eth_header_t));
	eth_header_t *lp = (eth_header_t*)p;
	unsigned char my_hw[] = { 0x00, 0x03, 0x93, 0xd4, 0x32, 0xe5 };	
	u32 ip, *t_ip, *s_ip;

	if( !p )
		return 1;
	memcpy_fromvec( p, vec, nvec, ARP_PACKET_SIZE );
	
	switch( arp->op ) {
	case ARP_REQUEST:
		arp->op = ARP_REPLY;
		t_ip = (u32*)arp->target_ip;
		s_ip = (u32*)arp->sender_ip;

		ip = *t_ip;
		if( ip != is->my_ip && ip != is->gateway )
			goto drop;
		*t_ip = *s_ip;
		*s_ip = ip;

		memcpy( arp->sender_mac, arp->target_mac, 6 );
		memcpy( arp->target_mac, my_hw, 6 );
		memcpy( lp->dmac, lp->smac, 6 );
		memcpy( lp->smac, my_hw, 6 );
		is->inject_packet( is, p, ARP_PACKET_SIZE );
		break;
	default:
		/* drop */
		break;
	}
drop:
	free( p );
	return 1;
}


/************************************************************************/
/*	send/receive packet						*/
/************************************************************************/

static inline int
intercept_packet( enet_iface_t *is, const struct iovec *vec, size_t nvec )
{
	int type=0, len, dport=0, ip_prot=0, fragoffs=0;
	int success=0;

	if( (is->flags & NO_DHCP) && !(is->flags & IP_PAYLOAD) )
		return 0;

	/* fast path */
	if( vec[0].iov_len >= 40 ) {
		unsigned char *p = vec[0].iov_base;

		type = ((u32)p[12] << 8) | p[13];
		len = (p[14] & 0xf) << 2;
		ip_prot = p[14+9];
		fragoffs = (((u32)p[14+6] & 0x1f) << 8) | p[14+7];
		
		if( 14 + len + 3 < 40 ) {
			dport = ((u32)p[14+len+2] << 8) | p[14+len+3];
			success = 1;
		}
	}

	/* slow path */
	if( !success ) {
		type = (iovec_getbyte(12, vec, nvec) << 8)
			| iovec_getbyte(13,vec,nvec);
		len = (iovec_getbyte(14, vec, nvec) & 0xf) << 2;
		fragoffs = ((iovec_getbyte(14+6, vec, nvec) & 0x1f) << 8)
			| iovec_getbyte(14+7, vec, nvec);
		ip_prot = iovec_getbyte(14+9, vec, nvec);
		dport = (iovec_getbyte(14+len+2,vec,nvec) << 8)
			| iovec_getbyte(14+len+3,vec,nvec);
	}
	if( !(is->flags & NO_DHCP) && type == ETH_TYPE_IP && ip_prot == PROT_UDP && !fragoffs && dport == PORT_BOOTPS )
		return handle_dhcp( is, vec, nvec );

	if( (is->flags & IP_PAYLOAD) ) {
		if( type == ETH_TYPE_ARP )
			return handle_arp( is, vec, nvec );
		if( type != ETH_TYPE_IP )
			return 1;
	}
	return 0;
}

int
send_packet( enet_iface_t *is, struct iovec *vec, size_t nvec )
{
	if( !intercept_packet(is, vec, nvec) ) {
		if( is->packet_pad ) {
			vec[0].iov_len += is->packet_pad;
			vec[0].iov_base -= is->packet_pad;
		}
		if( is->flags & IP_PAYLOAD )
			iovec_skip( 14, vec, nvec );

		if( writev(is->fd, vec, nvec) < 0 ) {
			perrorm("send_packet");
			return 1;
		}
	}
	return 0;
}

static inline void
add_ip_header( enet_iface_t *is, struct iovec *iov, size_t nvec )
{
	char headerbuf[14];
	char *p = iov[0].iov_base;

	if( iov[0].iov_len < 14 )
		p = headerbuf;
	
	memcpy( &p[0], is->c_macaddr, 6 );
	memcpy( &p[6], is->s_macaddr, 6 );
	p[12] = 0x08;
	p[13] = 0x00;

	if( p == headerbuf ) {
		memcpy_tovec( iov, nvec, headerbuf, sizeof(headerbuf) );
		iovec_skip( sizeof(headerbuf), iov, nvec );
	} else {
		iov[0].iov_len -= 14;
		iov[0].iov_base += 14;
	}
}

int
receive_packet( enet_iface_t *is, struct iovec *iov, size_t nvec )
{
	int s, sadd = 0;

	if( is->flags & IP_PAYLOAD ) {
		add_ip_header( is, iov, nvec );
		sadd = 14;
	}
	if( (s=readv(is->fd, iov, nvec)) <= 0 )
		return s;
	return s + sadd;
}
