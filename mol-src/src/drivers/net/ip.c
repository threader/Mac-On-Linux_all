/* 
 *   Creation Date: <2000/12/01 01:25:23 samuel>
 *   Time-stamp: <2001/01/21 17:13:24 samuel>
 *   
 *	<ip.c>
 *	
 *	Handles the TCP and UDP protocol
 *   
 *   Copyright (C) 2000 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#if 0	/******** THIS FILE IS CURRENTLY DISABLED SINCE IT CONTAINS UNFINISHED CODE ***********/

#include "mol_config.h"

#include <sys/socket.h>
#include <sys/poll_compat.h>
#include <sys/param.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include <linux/tcp.h>
#include <linux/udp.h>
#include <linux/ip.h>
#include <linux/if_ether.h>
#include <linux/if_arp.h>

#include "debugger.h"
#include "ip.h"
#include "async.h"

/* Two buffers of size HALF_TCP_WINDOW is used for receive buffering and retransmitting */ 
#define HALF_TCP_WINDOW		0x1000


#define FAKE_ETH_ADDR		0xDEADF00D
static inline void copy_fake_eth_addr( char *s ) { *(short*)s = 0; *(ulong*)&s[2] = FAKE_ETH_ADDR; }

struct tcp_port;
typedef int tcp_handler_t( struct tcp_port *tp, struct iphdr *iph, struct tcphdr *h, int size );

/* TCP state of the MOL endpoint:
 *
 * 	[socket interface <=> MOL] <----TCP----> [MacOS]
 */
enum {
	kCLOSED = 0,
	kSYN_RCVD,			// connecting, ACK,SYN sent if n_sent == 1
	kSYN_SENT,
	kESTABLISHED,
	kCLOSE_WAIT,			// MacOS sent FIN
	kLAST_ACK,

	kFIN_WAIT_1,			// EOF received from socket
	kFIN_WAIT_2,
	// kCLOSING,
	// kTIME_WAIT,

	NUM_TCP_STATES			// MUST BE LAST!
};

#define assert( cond, error ) ({ if( !(cond) ) printm("Assertion failed: %s", error); })

static tcp_handler_t 	tcph_closed, tcph_syn_rcvd, tcph_established, tcph_close_wait;
static tcp_handler_t 	tcph_fin_wait_1, tcph_fin_wait_2, tcph_last_ack;
static tcp_handler_t	*tcp_handlers[ NUM_TCP_STATES ];


#define COMMON_PORT_FIELDS(k) 					\
	int		mac_port;	/* mac port# */		\
	int		port;		/* linux port# */	\
	int		sfd;					\
	int		async_hid;				\
	struct k##_port	*next;

typedef struct gen_port {
	COMMON_PORT_FIELDS(gen);
} gen_port_t;

typedef struct udp_port {
	COMMON_PORT_FIELDS(udp);
} udp_port_t;


typedef struct tcp_port {
	COMMON_PORT_FIELDS(tcp);

	int		state;		// kCLOSED, kLAST_ACK etc.

	ulong		remote_ip;
	ulong		remote_port;
	
	ulong		mac_isn;	// initial sequence number for MacOS
	ulong		mol_isn;	// ISN for MOL

	ulong		n_mac_acked;	// #bytes acked by MacOS
	ulong		n_sent;		// #bytes we have sent to MacOS
	ulong		n_rcvd;		// #bytes we have received from MacOS

#if 0
	ulong		mac_win;     	// Announced mac-window (starts at n_mac_acked) 
	// retransmit buf
#endif
} tcp_port_t;

typedef struct ip_state {
	udp_port_t	*udp_root;	// udp ports in use
	tcp_port_t	*tcp_root;	// tcp ports in use

	void		*enetif;
	ulong		mac_ip;

	unsigned short	next_packet_id;
	ulong		next_tcp_isn;
} ip_state_t;

static ip_state_t 	ip[1];
static int 		use_count = 0;

static void 		handle_tcp_packet( struct iphdr *iph, struct tcphdr *tcph, int tcp_size );
static void		handle_udp_packet( struct iphdr *iph, struct udphdr *h, int udp_size );
static void		handle_arp_packet( const struct ethhdr *eh, int total_size, void *enetif );
static void 		rx_udp( int fd, int events );
static void 		rx_tcp( int fd, int events );
static void 		destroy_tcp_port( tcp_port_t *tp );
static void 		destroy_udp_port( udp_port_t *tp );


/************************************************************************/
/*	Init / Cleanup							*/
/************************************************************************/

void
ip_init( void )
{	
	if( use_count++ > 0 )
		return;

	memset( &ip[0], 0, sizeof(ip));

	// Initial IDs. Should perhaps be randomized
	ip->next_packet_id = 0;
	ip->next_tcp_isn = 0x1000;

	// TCP Handlers
	memset( tcp_handlers, 0, sizeof(tcp_handlers));
	tcp_handlers[ kCLOSED ] = tcph_closed;
	tcp_handlers[ kSYN_RCVD ] = tcph_syn_rcvd;
	tcp_handlers[ kESTABLISHED ] = tcph_established;
	tcp_handlers[ kCLOSE_WAIT ] = tcph_close_wait;
	tcp_handlers[ kLAST_ACK ] = tcph_last_ack;
	tcp_handlers[ kFIN_WAIT_2 ] = tcph_fin_wait_2;
	tcp_handlers[ kFIN_WAIT_1 ] = tcph_fin_wait_1;
}

void
ip_cleanup( void )
{
	if( --use_count != 0)
		return;
	
	while( ip->tcp_root )
		destroy_tcp_port( ip->tcp_root );
	while( ip->udp_root )
		destroy_udp_port( ip->udp_root );
}


/************************************************************************/
/*	Verbose							       	*/
/************************************************************************/

#define C_RED           "\33[1;31m"
#define C_GREEN         "\33[1;32m"
#define C_YELLOW        "\33[1;33m"
#define C_NORMAL        "\33[0;39m"

static void 
print_ipaddr( long addr )
{
	printm("%ld.%ld.%ld.%ld", (addr>>24) & 0xff, (addr>>16) & 0xff,
	       (addr>>8)& 0xff, addr & 0xff );
}

static void
print_tcp_packet( int to_mac, tcp_port_t *tp, struct iphdr *ip )
{
	int seq, ack_seq;
	struct tcphdr *h = (struct tcphdr*)((char*)ip + ip->ihl*4);

#if 1
	return;
#endif
	printm( to_mac ? C_RED : C_GREEN );
	printm("%s: ", to_mac? "=> MAC" : "<= MAC" );
	print_ipaddr( ip->saddr );
	printm(" %d -> ", h->source );
	print_ipaddr( ip->daddr );
	printm(" %d: ", h->dest);

	seq = h->seq;
	ack_seq = h->ack_seq;
	if( tp ){
		if( to_mac ) {
			seq -= tp->mol_isn;
			ack_seq -= tp->mac_isn;
		} else {
			seq -= tp->mac_isn;
			ack_seq -= tp->mol_isn;
		}
	}
	printm(" %4d (%4d)", seq, ip->tot_len - (ip->ihl*4) - (h->doff)*4 );
	if( h->ack )
		printm(" %d", ack_seq );
	printm("%s%s%s%s%s%s",
	       h->ack? " ACK" : "",
	       h->rst? " RST" : "",
	       h->urg? " URG" : "",
	       h->psh? " PSH" : "",
	       h->fin? " FIN" : "",
	       h->syn? " SYN" : "");
	printm( C_NORMAL );
	printm("\n");
}

/************************************************************************/
/*	Transmit side						       	*/
/************************************************************************/

int 
tx_packet_hook( char *p, int size, void *enetif )
{
	struct ethhdr *eh = (struct ethhdr*)p;
	struct iphdr *iph;
	int offs;

	// We handle ARP & IP
	if( eh->h_proto == ETH_P_ARP ){
		handle_arp_packet( eh, size, enetif );
		return 1;
	}

	if( eh->h_proto != ETH_P_IP )
		return 0;
	iph = (struct iphdr*)(p + sizeof(struct ethhdr));

	if( iph->frag_off & 0x1fff)
		printm("Fragmented package %X!\n", iph->frag_off);

	// offset to TCP / UDP header
	offs = sizeof(struct ethhdr) + 4*iph->ihl;
	
	// XXX: detect mac-interface changes

	// We assume only one interface uses IP
	ip->enetif = enetif;
	ip->mac_ip = iph->saddr;

	if( iph->protocol == IPPROTO_TCP ) {
		handle_tcp_packet( iph, (struct tcphdr*)(p+offs), size-offs );
	} else if( iph->protocol == IPPROTO_UDP ) {
		printm("IP from "); print_ipaddr( iph->saddr );
		printm(" to "); print_ipaddr( iph->daddr ); printm("  ");
		handle_udp_packet( iph, (struct udphdr*)(p+offs), size-offs );
		printm("\n");
	} else {
		printm("Unknown IP packet type received\n");
	}
	return 1;
}

/************************************************************************/
/*	ARP protocol							*/
/************************************************************************/

static void
handle_arp_packet( const struct ethhdr *eh, int size, void *enetif )
{
	struct eth_arphdr *h = (struct eth_arphdr*)((char*)eh + sizeof(struct ethhdr));
	char reply[ sizeof(struct ethhdr) + sizeof(struct eth_arphdr) ];
	struct eth_arphdr *rh = (void*)&reply[sizeof(struct ethhdr)];
	struct ethhdr *reh = (void*)&reply;

	if( size != sizeof(struct eth_arphdr) + sizeof(struct ethhdr) ) {
		printm("Bad arp packet (size = %d)!\n", size);
		return;
	}
	if( h->ar_hrd != ARPHRD_ETHER || h->ar_pro != 0x800 || h->ar_pln != 4 || h->ar_hln != 6 ){
		printm("Bad arp packet\n");
		return;
	}
	if( h->ar_op != ARPOP_REQUEST ) {
		printm("Unimplemented ARP operation %d\n", h->ar_op );
		return;
	}
	if( *(ulong*)h->ar_sip == 0 ) {
		// printm("ARP: MacOS tries to find its IP\n");
		// MacOS is trying to see if its own IP is free (it is)
		return;
	}
	if( *(ulong*)h->ar_tip == *(ulong*)h->ar_sip ) {
		// printm("Gratious ARP\n");
		return;
	}

	// Don't modify eh
	*rh = *h;
	*(ulong*)rh->ar_sip = *(ulong*)h->ar_tip;
	*(ulong*)rh->ar_tip = *(ulong*)h->ar_sip;
	*(short*)&rh->ar_tha[4] = *(short*)&h->ar_sha[4];
	*(ulong*)rh->ar_tha = *(ulong*)h->ar_sha;

	copy_fake_eth_addr( reh->h_source );
	copy_fake_eth_addr( rh->ar_sha );
	
	reh->h_proto = ETH_P_ARP;
	rh->ar_op = ARPOP_REPLY;

	if( synthesize_eth_packet( reply, sizeof(reply), enetif )){
		printm("synthesize_eth_packet error\n");
	}
}


/************************************************************************/
/*	Port allocation							*/
/************************************************************************/

#define bind_tcp_port(portnum)	((tcp_port_t*)bind_port(portnum,0))
#define bind_udp_port(portnum)	((udp_port_t*)bind_port(portnum,1))

static gen_port_t *
bind_port( int portnum, int is_udp )
{
	struct sockaddr_in saddr;
	gen_port_t *up;
	int hid=-1;
	int sfd;

	// assertion: portnum not in the linked list

	if( portnum && portnum < 1024 ){
#if 0
		printm("Refusing to bind TCP/UDP port below 1024\n");
		return NULL;
#else
		printm("Warning: binding TCP/UDP port below 1024\n");
#endif
	}
	if( (sfd = socket(PF_INET, is_udp? SOCK_DGRAM : SOCK_STREAM, 0)) < 0 ) {
		perrorm("opening UDP socket:\n");
		return NULL;
	}
	saddr.sin_family = AF_INET;
	saddr.sin_port = portnum;
	saddr.sin_addr.s_addr = INADDR_ANY;

	if( bind( sfd, &saddr, sizeof(saddr)) < 0 ){
		perrorm("binding port %d:\n", portnum);
		close(sfd);
		return NULL;
	}

	// Start the async handler
	if( is_udp )
		hid = add_async_handler( sfd, POLLIN, rx_udp, 1 /* sigio capable*/ );
	else
		hid = add_async_handler( sfd, 0 /* no events yet */, rx_tcp, 0 );

	if( hid < 0 ){
		printm("Could not register async UDP handler!\n");
		close(sfd);
		return NULL;
	}

	// Insert in linked list and register 
	up = calloc( 1, is_udp ? sizeof(udp_port_t) : sizeof(tcp_port_t) );
	up->sfd = sfd;
	up->port = portnum;
	up->async_hid = hid;
	
	up->next = is_udp? (gen_port_t*)ip->udp_root : (gen_port_t*)ip->tcp_root;
	*(is_udp? (gen_port_t**)&ip->udp_root : (gen_port_t**)&ip->tcp_root) = up;

	// up->mac_port = portnum;	/* callee responsibility */
	return up;
}


static void
destroy_port( gen_port_t *tp, gen_port_t **rootp )
{
	gen_port_t **pp;
	int found=0;
	
	printm("Destroying port %d\n", tp->mac_port );
	for( pp=rootp; *pp; pp=&(**pp).next )
		if( *pp == tp ) {
			found=1;
			*pp = tp->next;
			
			delete_async_handler( tp->async_hid );
			close(tp->sfd );
			free( tp );
			return;
		}
	printm("Internal error: destroy_tcp_port\n");
}

static void
destroy_udp_port( udp_port_t *up )
{
	destroy_port( (gen_port_t*)up, (gen_port_t**)&ip->udp_root );
}

static void 
destroy_tcp_port( tcp_port_t *tp )
{
	destroy_port( (gen_port_t*)tp, (gen_port_t**)&ip->tcp_root );
}


/************************************************************************/
/*	IP header							*/
/************************************************************************/

/* tlen is #bytes data (EXCLUDING the ip header!) */
static inline void
build_eth_ip_header(struct ethhdr *eh, int datalen, ulong saddr, int protocol )
{
	struct iphdr *iph = (struct iphdr*)((char*)eh + sizeof(struct ethhdr));

	// Ethernet header
	eh->h_proto = ETH_P_IP;
	copy_fake_eth_addr( eh->h_source );

	// IP header
	memset(iph, 0, sizeof(struct iphdr));
	iph->version = 4;
	iph->ihl = sizeof(struct iphdr) / 4;
	iph->tot_len = datalen + sizeof(struct iphdr);
	iph->id = ip->next_packet_id++;
	iph->ttl = 20;
	iph->saddr = saddr;
	iph->daddr = ip->mac_ip;
	iph->protocol = protocol;
	// iph->check = 0;

	// XXX: Tune these fields later 
	// iph->tos = 0;
	// iph->frag_off = 0;
}

/************************************************************************/
/*	TCP protocol							*/
/************************************************************************/

#define	SIZEOF_HEADERS_TCP	(sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct tcphdr))

static int 
send_tcp( tcp_port_t *tp, int flags, char *tcpdata, int datalen )
{
	char buf[ ETH_FRAME_LEN + 2 ];
	struct ethhdr *eh = (struct ethhdr*)&buf[2];
	struct iphdr *iph = (struct iphdr*)((char*)eh + sizeof(struct ethhdr));
	struct tcphdr *th = (struct tcphdr*)((char*)iph + sizeof(struct iphdr));
	unsigned int checksum;

	// NOTE: all fields in tp are not valid when called by send_tcp_nonconnected()
	assert( datalen + SIZEOF_HEADERS_TCP <= ETH_FRAME_LEN, "send_tcp: datalen too big\n");

	// Build IP header
	build_eth_ip_header( eh, datalen + sizeof(struct tcphdr), tp->remote_ip, 6 );
	iph->check = ip_fast_csum( (char*)iph, iph->ihl );

	// Build TCP header
	tcp_flag_word(th) = flags;	// Do first: overwrites other fields
	th->source = tp->remote_port;
	th->dest = tp->mac_port;
	th->window = 4096;		// XXX: Tune later
	th->urg_ptr = 0;
	th->doff = sizeof(struct tcphdr) / 4;

	th->seq = tp->mol_isn + tp->n_sent;
	th->ack_seq = tp->mac_isn + tp->n_rcvd;

	// Copy TCP data
	memcpy( (char*)th + sizeof(struct tcphdr), tcpdata, datalen );

	// Calculate TCP checksum
	th->check = 0;
	checksum = csum_partial( (char*)th, sizeof(struct tcphdr) + datalen, 0 );
	th->check = csum_tcpudp_magic( ip->mac_ip, tp->remote_ip, sizeof(struct tcphdr) + datalen, 
				       6 /* proto */, checksum );

	// Done... send it!
	print_tcp_packet( 1, tp, iph );

	if( synthesize_eth_packet( (char*)eh, datalen + SIZEOF_HEADERS_TCP, ip->enetif )){
		printm("synthesize_eth_packet error\n");
	} else {
		tp->n_sent += datalen;
	}
	return 0;
}

static int
send_tcp_nonconnected( int remote_ip, int remote_port, int flags )
{
	tcp_port_t dummy_port;

	memset( &dummy_port, 0, sizeof(dummy_port) );
	dummy_port.remote_ip = remote_ip;
	dummy_port.remote_port = remote_port;
	return send_tcp( &dummy_port, flags, NULL, 0 );
}


enum{ TCPH_NOERR=0, TCPH_RESET, TCPH_AGAIN, TCPH_IGNORE };

static void
handle_tcp_packet( struct iphdr *iph, struct tcphdr *h, int size )
{
	tcp_port_t *tp;

	// XXX: Check that size is large enough

	// Find TCP connection
	for( tp = ip->tcp_root; tp; tp=tp->next )
		if( tp->mac_port == h->source )
			break;
	print_tcp_packet( 0, tp, iph );

	// Handle RST packets
	if( h->rst ){
		if( tp ) {
			struct linger lin = {1,0};
			/* By using the SO_LINGER a RST is sent upon close */
			if( setsockopt( tp->sfd, SOL_SOCKET, SO_LINGER, &lin, sizeof(lin) ) < 0 )
				perrorm("setsockopt, SO_LINGER");
			destroy_tcp_port(tp);
			return;
		}
	}

	if( !tp ){
		if( !h->syn ){
			printm("Not-SYN packet to nonexistent port\n");
			send_tcp_nonconnected( iph->saddr, h->source, TCP_FLAG_RST );
			return;
		}
		if( h->syn ){
			if( !(tp=bind_tcp_port(h->source)) && !(tp=bind_tcp_port(0)) ) {
				printm("Failed to allocate TCP port!\n");
				send_tcp_nonconnected( iph->saddr, h->source, TCP_FLAG_RST );
				return;
			}
			tp->state = kCLOSED;
		}
	}

	for( ;; ) {
		int ret;
		if( !tcp_handlers[tp->state] ) {
			printm("Tcp handler %d unimplemented\n", tp->state );
			break;
		}
		ret = (*tcp_handlers[tp->state])(tp, iph, h, size);
		/* tp only valid after TCPH_RESET and TCPH_AGAIN */
		if( ret == TCPH_NOERR )
			break;
		if( ret == TCPH_AGAIN )
			continue;
		if( ret == TCPH_IGNORE ){
			printm("TCPH_IGNORE\n");
			break;
		}
		/* TCPH_RESET */
		send_tcp( tp, TCP_FLAG_RST | TCP_FLAG_ACK, NULL, 0 );
		destroy_tcp_port( tp );
		break;
	}
}

static int 
tcph_closed( tcp_port_t *tp, struct iphdr *iph, struct tcphdr *h, int size )
{
	struct sockaddr_in to;

	if( !h->syn )
		return TCPH_RESET;

	// Open connection
	to.sin_family = AF_INET;
	tp->remote_ip = to.sin_addr.s_addr = iph->daddr;
	tp->remote_port = to.sin_port = h->dest;
	tp->mac_port = h->source;

	tp->mac_isn = h->seq;
	tp->mol_isn = (ip->next_tcp_isn += 0x10000);
	tp->n_rcvd = 1;
	tp->n_sent = 0;
	tp->n_mac_acked = 0;
	
	tp->state = kSYN_RCVD;
	set_async_handler_events( tp->async_hid, POLLOUT );
	if( connect( tp->sfd, &to, sizeof(to)) < 0 ){
		if( errno == EINPROGRESS )
			return TCPH_NOERR;
		return TCPH_RESET;
	}

	// We almost certainly don't come here since connect will not complete synchronously.
	send_tcp( tp, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0 );
	tp->n_sent++;
	return TCPH_NOERR;
}

static int 
tcph_syn_rcvd( tcp_port_t *tp, struct iphdr *iph, struct tcphdr *h, int size )
{
	if( h->syn ) {
		// XXX: Handle new ISN?
		return TCPH_NOERR;
	}
	if( !h->ack || h->ack_seq != tp->mol_isn +1 || tp->n_sent != 1)
		return TCPH_RESET;

	set_async_handler_events( tp->async_hid, POLLIN );
	
	tp->state = kESTABLISHED;
	return TCPH_AGAIN;		// handle ACK and any data
}

static int 
tcph_established( tcp_port_t *tp, struct iphdr *iph, struct tcphdr *h, int size )
{
	int data_len;
	int data_offs;
	
	if( h->ack ) {
		int new_count = MAX( h->ack_seq - tp->mol_isn, tp->n_mac_acked );
		if( new_count > tp->n_sent ){
			printm("Bad ACK\n");
			return TCPH_IGNORE;
		}
		tp->n_mac_acked = new_count;
	}

	// Simply drop out of order packets for now.
	if( h->seq != tp->mac_isn + tp->n_rcvd ){
		printm("Out of order packet received\n");
		send_tcp( tp, TCP_FLAG_ACK, NULL, 0 );
		return TCPH_IGNORE;
	}

	// Handle (in order) data packet
	data_offs = sizeof(long) * (iph->ihl + h->doff);
	data_len = iph->tot_len - data_offs;

	if( data_len > 0 ) {
		// printm("Got %d bytes of data\n", data_len );
		if( write( tp->sfd, (char*)iph + data_offs, data_len ) != data_len )
			perrorm("write");
		tp->n_rcvd += data_len;

		send_tcp( tp, TCP_FLAG_ACK, NULL, 0 );
	}

	// FIN?
	if( h->fin ){
		tp->n_rcvd++;
		send_tcp( tp, TCP_FLAG_ACK, NULL, 0 );
		shutdown( tp->sfd, 1 );
		tp->state = kCLOSE_WAIT;
	}
	return TCPH_NOERR;
}

static int
tcph_close_wait( tcp_port_t *tp, struct iphdr *iph, struct tcphdr *h, int size )
{
	printm("tcph_close_wait....\n");
	// send_tcp( tp, TCP_FLAG_ACK, NULL, 0 );
	return TCPH_NOERR;
}

static int
tcph_last_ack( tcp_port_t *tp, struct iphdr *iph, struct tcphdr *h, int size )
{
	if( h->ack ) {
		if( h->seq != tp->mol_isn + tp->n_sent )
			printm("tcph_last_ack error\n");
		destroy_tcp_port( tp );
	}
	// send_tcp( tp, TCP_FLAG_ACK, NULL, 0 );
	return TCPH_NOERR;
}

static int
tcph_fin_wait_1( tcp_port_t *tp, struct iphdr *iph, struct tcphdr *h, int size )
{
	if( h->ack ) {
		if( h->ack_seq == tp->mol_isn + tp->n_sent )
			tp->state = kFIN_WAIT_2;
		else {
			// data ACK
			return TCPH_NOERR;
		}
	}
	if( h->fin && h->ack ){
		tp->n_rcvd++;
		send_tcp( tp, TCP_FLAG_ACK, NULL, 0 );
		destroy_tcp_port( tp );
	}
	// XXX: Receive and ACK data
	return TCPH_NOERR;
}

static int
tcph_fin_wait_2( tcp_port_t *tp, struct iphdr *iph, struct tcphdr *h, int size )
{
	if( h->fin ){
		tp->n_rcvd++;
		send_tcp( tp, TCP_FLAG_ACK, NULL, 0 );
		destroy_tcp_port( tp );
	}
	// send_tcp( tp, TCP_FLAG_ACK, NULL, 0 );
	return TCPH_NOERR;
}


static void
remote_tcp_close( tcp_port_t *tp, int was_reset )
{
	set_async_handler_events( tp->async_hid, 0 );

	if( !was_reset ) {
		switch( tp->state ){
		case kCLOSE_WAIT:
			tp->state = kLAST_ACK;
			break;
		case kESTABLISHED:
			tp->state = kFIN_WAIT_1;
			break;
		default:
			printm("Remote TCP close in state %d\n", tp->state );
			was_reset = 1;
			break;
		}
	}
	if( !was_reset ) {
		shutdown( tp->sfd, 0 );
		send_tcp( tp, TCP_FLAG_FIN | TCP_FLAG_ACK, NULL, 0 );
		tp->n_sent++;
	} else {
		send_tcp( tp, TCP_FLAG_RST, NULL, 0 );
		destroy_tcp_port( tp );
	}
}

static void
rx_tcp( int fd, int events )
{
	tcp_port_t *tp;
	char buf[ETH_FRAME_LEN - sizeof(struct iphdr) - sizeof(struct tcphdr) - sizeof(struct ethhdr)];
	struct msghdr mh;
	int s;
	
	for( tp=ip->tcp_root; tp; tp=tp->next )
		if( tp->sfd == fd )
			break;
	if( !tp ){
		printm("rx_tcp: internal error\n");
		return;
	}

	// Handle asynchronous open of connection
	if( events & POLLOUT ){
		int serr, optsize = sizeof(serr);
		if( !getsockopt( tp->sfd, SOL_SOCKET, SO_ERROR, &serr, &optsize ) && !serr ) {
			send_tcp( tp, TCP_FLAG_SYN | TCP_FLAG_ACK, NULL, 0 );
			tp->n_sent++;
			set_async_handler_events( tp->async_hid, 0 );
		} else {
			// Could not open connection, respond with a RST
			send_tcp( tp, TCP_FLAG_RST | TCP_FLAG_ACK, NULL, 0 );
			destroy_tcp_port( tp );
		}
		return;
	}
	
	if( events & ~POLLIN ) {
		if( events & POLLHUP ) {
			remote_tcp_close( tp, 1 );
			return;
		}
		if( events & POLLERR ){
			printm("******** POLLERR *******\n");
		}
		if( events & POLLNVAL ){
			printm("******** POLLNVAL ******\n");
		}
		printm("RX_TCP from ");
		print_ipaddr( tp->remote_ip );
		printm(" %ld ", tp->remote_port );
		printm("---> %d (state = %d)\n", tp->mac_port, tp->state );
		set_async_handler_events( tp->async_hid, 0 );
		return;
	}

	if( !(events & POLLIN )) {
		printm("--- POLLIN not set ---\n");
		return;
	}

	for( ;; ) {
		struct iovec iov;
		iov.iov_base = buf;
		iov.iov_len = sizeof(buf);
		
		mh.msg_name = NULL;
		mh.msg_namelen = 0 ;
		mh.msg_iov = &iov;
		mh.msg_iovlen = 1;
		mh.msg_control = NULL;
		mh.msg_controllen = 0;
		mh.msg_flags = 0;

		if( (s = recvmsg(fd, &mh, 0)) < 0 ){
			if( errno == EAGAIN )
				break;
			perrorm("rx_tcp, rcvmsg");
			return;
		}
		// Connection closed
		if( !s ) {
			remote_tcp_close( tp, 0 );
			return;
		}

		// XXX: Check TCP state
		send_tcp( tp, TCP_FLAG_ACK, buf, s );
	}
}


/************************************************************************/
/*	UDP protocol							*/
/************************************************************************/

static void
handle_udp_packet( struct iphdr *iph, struct udphdr *h, int udp_size )
{
	struct sockaddr_in to;
	udp_port_t *up;
	char *data = (char*)h + sizeof(struct udphdr);
	int size;

	printm("UDP, port: %d --> %d", h->source, h->dest);

	// Safetychecks
	if( udp_size < sizeof(struct udphdr) || (h->len < sizeof(struct udphdr)) || h->len > udp_size ) {
		printm("Malformed UDP packet\n");
		return;
	}
	size = h->len - sizeof(struct udphdr);

	// Find/bind UDP port
	for( up = ip->udp_root; up; up=up->next )
		if( up->mac_port == h->source )
			break;
	if( !up && !(up = bind_udp_port(h->source)) && !(up=bind_udp_port(0)) ) {
		printm("Failed to bind a UDP port!\n");
		return;
	}
	up->mac_port = h->source;

	// Send packet
	to.sin_family = AF_INET;
	to.sin_port = h->dest;
	to.sin_addr.s_addr = iph->daddr;
	if( sendto( up->sfd, data, size, 0, &to, sizeof(to) ) < 0 ){
		perrorm("sendto");
		return;
	}
	printm(" [SENT]");
}

#define MAX_UDP_DATA		(ETH_DATA_LEN - sizeof(struct iphdr) - sizeof(struct udphdr))
#define SIZEOF_HEADERS_UDP 	(sizeof(struct iphdr) + sizeof(struct udphdr) + sizeof(struct ethhdr))

static void
rx_udp( int fd, int events )
{
	struct sockaddr_in from;
	int size, fromlen = sizeof(from);
	udp_port_t *up;
	char buf[ ETH_FRAME_LEN + 2 ];
	char *data = buf + 2 + sizeof(struct ethhdr) + sizeof(struct iphdr) + sizeof(struct udphdr);
	struct ethhdr *eh = (struct ethhdr*)&buf[2];
	struct iphdr *iph = (struct iphdr*)((char*)eh + sizeof(struct ethhdr));
	struct udphdr *uh = (struct udphdr*)((char*)iph + sizeof(struct iphdr));

	for( up=ip->udp_root; up; up=up->next )
		if( up->sfd == fd )
			break;
	if( !up ){
		printm("rx_udp: internal error\n");
		return;
	}
	if( (size=recvfrom( fd, data, MAX_UDP_DATA, 0, &from, &fromlen )) < 0 ){
		perrorm("recvfrom");
		return;
	}
	printm("UDP packet size %d received, ", size);
	print_ipaddr( from.sin_addr.s_addr );
	printm(": %d ----> %d (mac: %d)\n", from.sin_port, up->port, up->mac_port);

	// ************* Build ethernet packet *******************
	build_eth_ip_header( eh, size + sizeof(struct udphdr), from.sin_addr.s_addr, 17 /* udp protocol */ );
	iph->check = ip_fast_csum( (char*)iph, iph->ihl );

	// UDP header
	uh->source = from.sin_port;
	uh->dest = up->mac_port;
	uh->len = size + sizeof(struct udphdr);
	/* XXX: The checksum is not required for UDP but we should perhaps use one anyway */
	uh->check = 0;	

	if( synthesize_eth_packet( buf+2, size + SIZEOF_HEADERS_UDP, ip->enetif )){
		printm("synthesize_eth_packet error\n");
	}
}


/************************************************************************/
/*	Packet reception (incoming packets from the network)		*/
/************************************************************************/

int 
rx_packet_hook( char *p, int size, void *enetif )
{
	struct ethhdr *eh = (struct ethhdr*)p;

	// We do IP and ARP
	if( eh->h_proto != ETH_P_IP && eh->h_proto != ETH_P_ARP )
		return 0;		/* XXX: FIXME */

	printm("Got IP/ARP packet\n");
	return 1;
}

#endif
