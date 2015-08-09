/*
	File:		dlpiuser.h

	Contains:	Copyright:	� 1993-1996 by Mentat Inc., all rights reserved.

	Version:	Mentat Inc, 2.x ?

	Copyright:	� 1997 by Apple Computer, Inc., all rights reserved.

*/

/*
	File:		dlpiuser.h

	Contains:	Header file for the DLPI template.  These are definitions
				that users need to take advantage of features in the DLPI drivers
				
				 ** dlpiuser.h 5.3, last change 27 Mar 1996

	Copyright:	� 1996-1997 by Mentat Inc. and Apple Computer, Inc., all rights reserved.

*/

#ifndef __DLPIUSER__
#define __DLPIUSER__

#ifndef __OPENTRANSPORT__
#include <OpenTransport.h>
#endif

/* Flags for various structures. */

enum
{
	DL_NORMAL_STATUS	= 0x01,
	DL_ERROR_STATUS		= 0x02,
	DL_TRUNCATED_PACKET	= 0x04,
	DL_VERSION			= 0x00,
	DL_VERSION_BITS		= 0xF0000000
};

/*
 * Receive and send error flags, these should not overlap with other
 * flags above.
 */
enum
{
	DL_CRC_ERROR		= 0x10,
	DL_RUNT_ERROR		= 0x20,
	DL_FRAMING_ERROR	= 0x40,
	DL_BAD_802_3_LENGTH	= 0x80,
	DL_ERROR_MASK		= DL_CRC_ERROR | DL_RUNT_ERROR | DL_FRAMING_ERROR | DL_BAD_802_3_LENGTH
};

/* Input and output structure for I_OTSetRawMode kOTSetRecvMode ioctl. */
struct dl_recv_control_t {
	unsigned long	dl_primitive;
	unsigned long	dl_flags;
	unsigned long	dl_truncation_length;
};

typedef struct dl_recv_control_t dl_recv_control_t;

/* Structure returned with every inbound packet after kOTSetRecvMode. */
struct dl_recv_status_t {
	unsigned long	dl_overall_length;
	unsigned long	dl_flags;
	unsigned long	dl_packet_length_before_truncation;
	unsigned long	dl_pad;
	OTTimeStamp		dl_timestamp;
};

typedef struct dl_recv_status_t	dl_recv_status_t;

/* Input structure for I_OTSetRawMode kOTSendErrorPacket. */
struct dl_send_control_t {
	unsigned long	dl_primitive;
	unsigned long	dl_flags;
};

typedef struct dl_send_control_t	dl_send_control_t;

/*
 * The statistics data in DL_GET_STATISTICS_ACK messages is structured
 * as follows:
 *
 *	TOptionHeader
 *	dle_interface_status_t
 *	TOptionHeader
 *	dle_ethernet_status_t
 *
 * Anyone reading the statistics should pay attention to the length fields
 * in the TOptionHeader structures.  This will allow the statistics
 * structures to grow in the future.
 */

/* Level value for DL_GET_STATISTICS_ACK */
enum
{
	DLPI_XTI_LEVEL		= 12
};

/* Option names for DL_GET_STATISTICS_ACK */
enum
{
	DL_INTERFACE_MIB		= 1,
	DL_ETHERNET_MIB			= 2
};

/*
 * Interface MIB statistics (RFC 1573).  The receive counters are
 * maintained by the common code in dlpiether.c.  The transmit counters
 * must be maintained by the board-specific code; dlpiether code does
 * not see outbound fast path packets and so it is impossible for the
 * counters to be handled in common code.
 */
struct dle_interface_status_s {
	/* Total number of octets received, including framing bytes. */
	unsigned long	bytes_received;

	/* Number of packets, delivered by this sublayer to a higher layer,
	 * which were not addressed to a multicast or broadcast address. */
	unsigned long	unicast_frames_received;

	/* Total number of octets transmitted, including framing characters. */
	unsigned long	bytes_sent;

	/* Total number of packets that higher-level protocols requested
	 * be transmitted, and which were not addressed to a multicast
	 * or broadcast address, including those that were discarded or
	 * not sent. */
	unsigned long	unicast_frames_sent;

	/* Number of packets, delivered by this sublayer to a higher layer,
	 * which were addressed to a multicast address. */
	unsigned long	multicast_frames_received;

	/* Number of packets, delivered by this sublayer to a higher layer,
	 * which were addressed to a address. */
	unsigned long	broadcast_frames_received;

	/* Number of inbound packets which were chosen to be discarded
	 * even though no errors had been detected to prevent their being
	 * deliverable to a higher-layer protocol.  The board-specific
	 * code should increment this count if it cannot deliver a
	 * packet for which dle_inbound_probe returned a valid cookie,
	 * for instance, if an mblk could not be allocated. */
	unsigned long	receive_discards;

	/* Number of inbound packets that contained errors preventing them
	 * from being deliverable to a higher-layer protocol.  This count
	 * is created by the dle code from adding together all of Ethernet
	 * receive error counts. */
	unsigned long	receive_errors;

	/* Number of packets received which were discarded because of an
	 * unknown or unsupported protocol. */
	unsigned long	receive_unknown_protos;

	/* Total number of packets that higher-level protocols requested
	 * be transmitted, and which were addressed to a multicast address. */
	unsigned long	multicast_frames_sent;

	/* Total number of packets that higher-level protocols requested
	 * be transmitted, and which were addressed to a broadcast address. */
	unsigned long	broadcast_frames_sent;

	/* Number of outbound packets which were chosen to be discarded even
	 * though no errors had been detected to prevent their being
	 * transmitted.  One possible reason could be to free up buffer space.*/
	unsigned long	transmit_discards;

	/* Number of outbound packets that could not be transmitted because
	 * of errors.  This count is created by the dle code when necessary
	 * by adding together all of the Ethernet transmit errors. The
	 * common code also adds in packets that could not be sent because
	 * they were malformed by the upper-level protocol. */
	unsigned long	transmit_errors;

	/* Size of the largest packet which can be sent/received, in octets.
	 * Set to 1514 by default; board-specific software may change
	 * this after dle_install if a different size is supported. */
	unsigned long	mtu;

	/* An estimate of the interface's current bandwidth in bits/second.
	 * The common code (dle_install) will set this value to 10Mb/second;
	 * if the board supports a higher speed, then internal code should
	 * change the value after calling dle_install. */
	unsigned long	speed;
};

typedef struct dle_interface_status_s	dle_interface_status_t;

/*
 * Ethernet MIB statistics (RFC 1643).  These are maintained by the board-specific
 * software as learned from the underlying hardware.
 */
struct dle_ethernet_status_s {
	/* Identifies the chipset used to realize the interface. */
	unsigned long	ether_chip_set;

	/* Frames received that are not an integral number of octets
	 * in length and do not pass the FCS check. */
	unsigned long	receive_alignment_errors;

	/* Frames received that are an integral number of octets in
	 * length but do not pass the FCS check. */
	unsigned long	receive_fcs_errors;

	/* Frames received that exceed the maximum permitted frame size. */
	unsigned long	receive_frames_too_long;

	/* Frames for which reception fails due to an internal sublayer
	 * error.  Only counted here if not counted as any other error. */
	unsigned long	receive_internal_mac_errors;

	/* Successfully transmitted frames for which transmission is
	 * inhibited by exactly one collision. */
	unsigned long	transmit_single_collision_frames;

	/* Successfully transmitted frames for which transmission is
	 * inhibited by more than one collision. */
	unsigned long	transmit_multiple_collision_frames;

	/* A count of times that the SQE TEST ERROR message is generated
	 * by the PLS sublayer. */
	unsigned long	transmit_sqe_test_errors;

	/* Frames for which the first transmission attempt is delayed
	 * because the medium is busy. */
	unsigned long	transmit_deferred_transmissions;

	/* The number of times that a collision is detected later than
	 * 512 bit-times into the transmission of a packet. */
	unsigned long	transmit_late_collisions;

	/* Frames for which transmission fails due to excessive collisions. */
	unsigned long	transmit_excessive_collisions;

	/* Frames for which transmission fails due to an internal sublayer
	 * error.  Count should be independent of collision counters. */
	unsigned long	transmit_internal_mac_errors;

	/* The number of times that the carrier sense condition was lost
	 * or never asserted when attempting to transmit a frame. */
	unsigned long	transmit_carrier_sense_errors;
};

typedef struct dle_ethernet_status_s	dle_ethernet_status_t;

#endif
