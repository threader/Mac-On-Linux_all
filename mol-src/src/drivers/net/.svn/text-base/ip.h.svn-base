/* 
 *   Creation Date: <2004/04/03 16:25:48 samuel>
 *   Time-stamp: <2004/06/12 22:19:23 samuel>
 *   
 *	<ip.h>
 *	
 *	IP headers
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */

#ifndef _H_IP
#define _H_IP

#include "byteorder.h"

/* ethernet link layer (RFC894) */
typedef struct {
	char		dmac[6];
	char		smac[6];
	u16		type;
} eth_header_t;

#define ETH_TYPE_IP	0x0800
#define ETH_TYPE_ARP	0x0806

typedef struct {
	u16	hard_type;
	u16	prot_type;
	u8	hard_size;
	u8	prot_size;
	u16	op;
	u8	sender_mac[6];
	u8	sender_ip[4];		/* badly aligned */
	u8	target_mac[6];
	u8	target_ip[4];
} arp_request_t;

/* arp operations */
#define ARP_REQUEST		1
#define ARP_REPLY		2
#define RARP_REQUEST		3
#define RARP_REPLY		4


typedef struct {
#if BYTE_ORDER == LITTLE_ENDIAN
	u8		hlen:4,
			version:4;
#else
	u8		version:4,
			hlen:4;
#endif
	u8		tos;
	u16		totlen;
	u16		id;
	u16		frag_offs;
	u8		ttl;
	u8		protocol;
	u16		checksum;
	u32		saddr;
	u32		daddr;
	/* options if any */
	u8		options[1];
} ip_header_t;

/* protocols */
#define PROT_UDP	17

typedef struct {
	u16		sport;
	u16		dport;
	u16		len;
	u16		checksum;
	char		data[1];
} udp_header_t;

/* assembly exports */
extern u16 	ip_fast_csum( u8 *ipp, int len );

#endif   /* _H_IP */
