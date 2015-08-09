/* 
 *   Creation Date: <1999/05/23 00:00:00 BenH>
 *   
 *	<osi_enet.h>
 *	
 *	 Mac-On-Linux project
 *
 *   Linux/macos common definitions for ethernet interface
 *   
 *   Copyright (C) 1999 Benjamin Herrenschmidt (bh40@calva.net)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_OSI_ENET
#define _H_OSI_ENET

/* Also used as VendorID/DeviceID */
#define kMOL_EtherChipID	0x66666665
#define kMOL_BoardName		"MacOnLinuxENet"

/* Status flags returned by LinuxOSIEnetGetStatus() */
#define kEnetStatusMask		0xFFFF0000

#define kEnetStatusRxPacket	0x00010000
#define kEnetStatusRxLengthMask	0x0000FFFF

/* Commands sent to LinuxOSIEnetControl() */
#define kEnetAckIRQ		0x0
#define kEnetCommandStart	0x1
#define kEnetCommandStop	0x2
#define kEnetRouteIRQ		0x3

#if !defined(macintosh)
typedef __u32 UInt32;
#endif

#endif /* _H_OSI_ENET */
