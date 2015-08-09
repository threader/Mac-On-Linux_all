/* 
 *   Creation Date: <2000/12/01 01:26:38 samuel>
 *   Time-stamp: <2000/12/10 21:10:58 samuel>
 *   
 *	<ip.h>
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

#ifndef _H_IP
#define _H_IP

/* defined in ip.c */
extern void ip_init( void );
extern void ip_cleanup( void );

extern int tx_packet_hook( char *ethpacket, int size, void *enetif );
extern int rx_packet_hook( char *ethpacket, int size, void *enetif );

/* defined in osi_enet.c */
extern int synthesize_eth_packet( char *ethpacket, int size, void *iface );


/* The kernel arp header does not include the ethernet specific fields */
struct eth_arphdr {
        unsigned short	ar_hrd;			/* format of hardware address	*/
        unsigned short	ar_pro;			/* format of protocol address	*/
        unsigned char	ar_hln;			/* length of hardware address	*/
        unsigned char	ar_pln;			/* length of protocol address	*/
        unsigned short	ar_op;			/* ARP opcode (command)		*/
	
	/* ethernet specific fields */
	unsigned char	ar_sha[6];		/* sender hardware address      */
	unsigned char	ar_sip[4];		/* sender IP address		*/
	unsigned char	ar_tha[6];		/* target hardware address      */
	unsigned char	ar_tip[4];		/* target IP address		*/
};


/************************************************************************/
/*	TCP Checksum (definitions from checksum.S)			*/
/************************************************************************/

/*
 * computes the checksum of a memory block at buff, length len,
 * and adds in "sum" (32-bit)
 *
 * returns a 32-bit number suitable for feeding into itself
 * or csum_tcpudp_magic
 *
 * this function must be called with even lengths, except
 * for the last fragment, which may be odd
 *
 * it's best to have buff aligned on a 32-bit boundary
 */
extern unsigned int csum_partial(const unsigned char * buff, int len,
				 unsigned int sum);

/*
 * turns a 32-bit partial checksum (e.g. from csum_partial) into a
 * 1's complement 16-bit checksum.
 */
static inline unsigned int csum_fold(unsigned int sum)
{
	unsigned int tmp;

	/* swap the two 16-bit halves of sum */
	__asm__("rlwinm %0,%1,16,0,31" : "=r" (tmp) : "r" (sum));
	/* if there is a carry from adding the two 16-bit halves,
	   it will carry from the lower half into the upper half,
	   giving us the correct sum in the upper half. */
	sum = ~(sum + tmp) >> 16;
	return sum;
}

/*
 * this routine is used for miscellaneous IP-like checksums, mainly
 * in icmp.c
 */
static inline unsigned short ip_compute_csum(unsigned char * buff, int len)
{
	return csum_fold(csum_partial(buff, len, 0));
}


/*
 * FIXME: I swiped this one from the sparc and made minor modifications.
 * It may not be correct.  -- Cort
 */
static inline ulong csum_tcpudp_nofold( ulong saddr, ulong daddr, unsigned short len, 
					unsigned short proto, unsigned int sum ) 
{
    __asm__("
	addc %0,%0,%1
	adde %0,%0,%2
	adde %0,%0,%3
	addze %0,%0
	"
	: "=r" (sum)
	: "r" (daddr), "r"(saddr), "r"((proto<<16)+len), "0"(sum));
    return sum;
}

/*
 * This is a version of ip_compute_csum() optimized for IP headers,
 * which always checksum on 4 octet boundaries.  ihl is the number
 * of 32-bit words and is always >= 5.
 */
extern unsigned short ip_fast_csum(unsigned char * iph, unsigned int ihl);

/*
 * computes the checksum of the TCP/UDP pseudo-header
 * returns a 16-bit checksum, already complemented
 */
extern unsigned short csum_tcpudp_magic(ulong saddr, ulong daddr, unsigned short len, 
					unsigned short proto, unsigned int sum);

/*
 * Computes the checksum of a memory block at src, length len,
 * and adds in "sum" (32-bit), while copying the block to dst.
 */
extern unsigned int csum_partial_copy_generic(const char *src, char *dst,
					      int len, unsigned int sum );

#endif   /* _H_IP */
