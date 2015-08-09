/*
 * MolEnet, Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _H_MOLENET
#define _H_MOLENET

#include <IOKit/network/IOEthernetController.h>
#include <IOKit/network/IOEthernetInterface.h>
#include <IOKit/network/IOGatedOutputQueue.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOTimerEventSource.h>
#include <IOKit/network/IOMbufMemoryCursor.h>
#include <IOKit/IODeviceMemory.h>
#include <string.h>            /* bcopy */

extern "C" {
#include <sys/param.h>
#include <sys/mbuf.h>
#include <net/ethernet.h>
}
#include "enet2_sh.h"

#define RX_NUM_EL		64		// 2^n
#define TX_NUM_EL		16
#define RX_MASK			(RX_NUM_EL - 1)
#define TX_MASK			(TX_NUM_EL - 1)


class MolEnet : /*public*/ IOEthernetController {

	OSDeclareDefaultStructors( MolEnet )

private:
	struct mbuf		*rxMBuf[ RX_NUM_EL ];

	bool			is_open;			// osi_driver opened
	enet2_ring_t		*rx_ring;
	enet2_ring_t		*tx_ring;
	enet2_ring_t		*rx_of_ring;			// second part of the memory descriptor
	enet2_ring_t		*tx_of_ring;
	int			rx_head;
	int			tx_tail;

	IOEthernetInterface	*networkInterface;
	//IOGatedOutputQueue	*transmitQueue;

	IOBasicOutputQueue	*transmitQueue;
	//bool			isPromiscuous;
	//bool			multicastEnabled;

	bool			linkStatusReported;
	
	IOWorkLoop		*workLoop;
	IOInterruptEventSource	*_irq;

	IOMbufBigMemoryCursor	*rxMBufCursor;
	IOMbufBigMemoryCursor	*txMBufCursor;

	bool			ready;
	bool			netifEnabled;

	OSDictionary		*mediumDict;

	unsigned long		currentPowerState;

	/******************************************************/

	void			updateRXDescriptor( int ind );

	bool 			resetAndEnable( bool enable );
	bool 			createMediumTables();

	UInt32			outputPacket( struct mbuf * m, void * param );

	void			rxIRQ( IOInterruptEventSource * src, int count );
	void			monitorLinkStatus( bool firstPoll = false );

public:
	virtual IOService	*probe( IOService *provider, SInt32 *score );
	virtual bool		start( IOService *provider);
	virtual void		free();
    
	virtual bool		createWorkLoop();
	virtual IOWorkLoop 	*getWorkLoop() const;
    
	virtual IOReturn	enable( IONetworkInterface *netif );
	virtual IOReturn	disable( IONetworkInterface *netif );

	virtual IOReturn	getHardwareAddress( IOEthernetAddress *addr );

	virtual IOReturn	setMulticastMode( IOEnetMulticastMode mode );
	virtual IOReturn	setMulticastList( IOEthernetAddress *addrs, UInt32 count );

	virtual IOReturn	setPromiscuousMode( IOEnetPromiscuousMode mode );
    
	virtual IOOutputQueue	*createOutputQueue();

	virtual void		getPacketBufferConstraints( IOPacketBufferConstraints *r ) const;
    
	virtual const OSString	*newVendorString() const;
	virtual const OSString	*newModelString() const;
	virtual const OSString	*newRevisionString() const;

	virtual bool		configureInterface( IONetworkInterface *netif );

	// Simple power managment support:
	virtual IOReturn	setPowerState( UInt32 powerStateOrdinal, IOService *whatDevice );
	virtual IOReturn	registerWithPolicyMaker( IOService *policyMaker );
};


#endif   /* _H_MOLENET */
