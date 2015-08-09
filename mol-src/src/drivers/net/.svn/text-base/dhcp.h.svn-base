/* 
 *   Creation Date: <2004/03/31 11:41:42 samuel>
 *   Time-stamp: <2004/04/03 18:16:53 samuel>
 *   
 *	<dhcp.h>
 *	
 *	
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */

#ifndef _H_DHCP
#define _H_DHCP

/* ports */
#define PORT_BOOTPS	67
#define PORT_BOOTPC	68

typedef struct {
	u8		opcode;
	u8		hw_type;
	u8		hw_addrlen;
	u8		hops;
	u32		xid;
	u16		secs;
	u16		flags;
	u32		ciaddr;
	u32		yiaddr;
	u32		siaddr;
	u32		giaddr;
	char		chaddr[16];
	char		sname[64];
	char		file[128];
	u32		option_magic;
	unsigned char	options[64];
} bootp_packet_t;

/* bootp opcode */
#define BOOTREQUEST		1
#define BOOTREPLY		2

/* DHCP type */
#define DHCPDISCOVER		1
#define DHCPOFFER		2
#define DHCPREQUEST		3
#define DHCPDECLINE		4
#define DHCPACK			5
#define DHCPNACK		6
#define DHCPRELEASE		7
#define DHCPINFORM		8

/* DHCP option tags */
#define DHCPO_PAD		0	/* pad (no length byte) */
#define DHCPO_SUBNET_MASK	1	/* subnet mask */
#define DHCPO_TIME_OFFSET	2	/* time offset */
#define DHCPO_ROUTERS		3	/* routers (n=len/4) */
#define DHCPO_TIME_SERVER	4	/* time server */
#define DHCPO_NAME_SERVER	5	/* name server */
#define DHCPO_DNS_SERVER	6	/* domain name server */
#define DHCPO_LOG_SERVER	7	/* log server */
#define DHCPO_LPR_SERVER	9	/* LPR server */
#define DHCPO_IMPRESS_SERVER	10	/* Impress server*/
#define DHCPO_RESOURCE_LOCATION	11	/* Resource location */
#define DHCPO_HOST_NAME		12	/* host name */
#define DHCPO_BOOT_FILE_SIZE	13	/* boot file size */
#define DHCPO_MERIT_DUMP_FILE	14	/* merit dump file */
#define DHCPO_DOMAIN_NAME	15	/* domain name */
#define DHCPO_SWAP_SERVER	16	/* swap server */
#define DHCPO_ROOT_PATH		17	/* root path */
#define DHCPO_EXTENSIONS_PATH	18	/* extensions path */
#define DHCPO_IP_FORWARDING	19	/* IP forwarding disable/enable */
#define DHCPO_NON_LOCAL_ROUTING	20	/* non-local src routing enable/disable */
#define DHCPO_POLICY_FILTER	21	/* policy filter option */
#define DHCPO_DATAGRAM_RES_SIZE	22	/* maximum datagram reassembly size */
#define DHCPO_DEF_IP_TTL	23	/* default IP time-to-live */
#define DHCPO_MTU_AGING_TIMEOUT	24	/* path MTU aging timeout */
#define DHCPO_PATH_MTU_PLATAU	25	/* path MTU plateau table */
#define DHCPO_INTFERFACE_MTU	26	/* interface MTU option */
#define DHCPO_ALL_SUBNETS_LOCAL	27	/* all subnets are local */
#define DHCPO_BROADCAST_ADDR	28	/* broadcast addess */
#define DHCPO_PERF_MASK_DISC	29	/* perform mask discovery */
#define DHCPO_MASK_SUPPLIER	30	/* mask supplier */
#define DHCPO_PERF_ROUTER_DISC	31	/* perform router discovery */
#define DHCPO_ROUTER_SOL_ADDR	32	/* router solicitation address */
#define DHCPO_STATIC_ROUTE	33	/* static route option */
#define DHCPO_TRAILER_ENCAP	34	/* trailer encapsulation */
#define DHCPO_ARP_CACHE_TIMEOUT	35	/* ARP cache timeout */
#define DHCPO_ETHERNET_ENCAP	36	/* ethernet encapsulation */
#define DHCPO_TCP_DEF_TTL	37	/* TCP default TTL option */
#define DHCPO_TCP_KEEPALIVE	38	/* TCP keepalive option */
#define DHCPO_TCP_KEEPALIVE_GB	39	/* TCP keepalive garbage */
#define DHCPO_NIS_DOMAIN	40	/* NIS domain */
#define DHCPO_NIS_SERVERS	41	/* network information servers */
#define DHCPO_NTP_SERVERS	42	/* network time protocol servers */
#define DHCPO_VENDOR		43	/* vendor specific */
#define DHCPO_NETBIOS_NS	44	/* netbios over TCP/IP name server */
#define DHCPO_NETBIOS_DD_SERV	45	/* netbios over TCP/IP datagram distribution server */
#define DHCPO_NETBIOS_NODE_TYPE	46	/* NetBIOS over TCP/IP Node Type */
#define DHCPO_NETBIOS_SCOPE	47	/* netbios over TCP/IP scope option */
#define DHCPO_XWIN_FONT_SERV	48	/* X Windows system font server option */
#define DHCPO_XWIN_DISP_MGR	49	/* X Windows system display manager option */
#define DHCPO_REQ_IP_ADDR	50	/* requested IP address */
#define DHCPO_IP_LEASE_TIME	51	/* IP address lease time */
#define DHCPO_OPTION_OVERLOAD	52	/* Option overload (IMPORTANT) */
#define DHCPO_DHCP_MSG_TYPE	53	/* DHCP message type */
#define DHCPO_SERVER_ID		54	/* server identification */
#define DHCPO_PARAM_REQ_LIST	55	/* parameter request list */
#define DHCPO_MESSAGE		56	/* message */
#define DHCPO_MAX_DHCP_MSG_SIZE	57	/* maximum DHCP message size */
#define DHCPO_RENEWAL_TIME	58	/* renewal (T1) time value */ 
#define DHCPO_REBINDING_TIME	59	/* rebinding (T2) time value */ 
#define DHCPO_CLASS_ID		60	/* class identifier */
#define DHCPO_CLIENT_ID		61	/* client identifier */
#define DHCPO_END		255	/* end marker */

#endif   /* _H_DHCP */
