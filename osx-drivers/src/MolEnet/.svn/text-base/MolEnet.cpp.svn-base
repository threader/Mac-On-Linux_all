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

#include "mol_config.h"
#include "MolEnet.h"
#include "osi_calls.h"

#include <IOKit/IORegistryEntry.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/platform/AppleMacIODevice.h>
#include <IOKit/assert.h>

#define super IOEthernetController
OSDefineMetaClassAndStructors( MolEnet, IOEthernetController )

#define NETWORK_BUFSIZE		(kIOEthernetMaxPacketSize + 2)
#define TRANSMIT_QUEUE_SIZE	1024


IOService *
MolEnet::probe( IOService *provider, SInt32 *score )
{
	if( OSI_Enet2Cntrl1(kEnet2CheckAPI, OSI_ENET2_API_VERSION) < 0 )
		return NULL;
	OSI_Enet2Cntrl1( kEnet2OSXDriverVersion, OSX_ENET_DRIVER_VERSION );

	return this;
}

bool
MolEnet::start( IOService * provider )
{
	IOPhysicalAddress tx_phys, rx_phys;
	IOPhysicalAddress tx_of_phys, rx_of_phys;
	int i, result;

	if( !super::start(provider) )
		return false;

	if( OSI_Enet2Open() ) {
		is_open = 1;
		return false;
	}
	
	//transmitQueue = OSDynamicCast( IOGatedOutputQueue, getOutputQueue() );
	transmitQueue = OSDynamicCast( IOBasicOutputQueue, getOutputQueue() );
	if( !transmitQueue ) {
		printm("MolEnet: output queue initialization failed\n");
		return false;
	}
	transmitQueue->retain();

	// Allocate a IOMbufBigMemoryCursor instance. Currently, the maximum
	// number of segments is set to 2. The maximum length for each segment
	// is set to the maximum ethernet frame size (plus padding).

	txMBufCursor = IOMbufBigMemoryCursor::withSpecification( NETWORK_BUFSIZE, 2 );
	rxMBufCursor = IOMbufBigMemoryCursor::withSpecification( NETWORK_BUFSIZE, 2 );
	if( !txMBufCursor || !rxMBufCursor ) {
		printm("MolEnet: IOMbufBigMemoryCursor allocation failure\n");
		return false;
	}

	// Get a reference to the IOWorkLoop in our superclass.
	IOWorkLoop * myWorkLoop = getWorkLoop();
	assert(myWorkLoop);

	// Allocate a IOInterruptEventSources.
	_irq = IOInterruptEventSource::interruptEventSource( this, 
				     (IOInterruptEventAction)&MolEnet::rxIRQ, provider, 0);
	
        if( !_irq || (myWorkLoop->addEventSource(_irq) != kIOReturnSuccess )) {
		printm("MolEnet: _irq init failure\n");
		return false;
	}

	// Allocate the ring descriptors
	rx_ring = (enet2_ring_t*)IOMallocContiguous( 2 * RX_NUM_EL * sizeof(enet2_ring_t), 
						     sizeof(enet2_ring_t), &rx_phys );
	tx_ring = (enet2_ring_t*)IOMallocContiguous( 2 * TX_NUM_EL * sizeof(enet2_ring_t),
						     sizeof(enet2_ring_t), &tx_phys );		
	if( !rx_ring || !tx_ring )
		return false;

	rx_of_ring = rx_ring + RX_NUM_EL;
	tx_of_ring = tx_ring + TX_NUM_EL;
	rx_of_phys = rx_phys + sizeof(enet2_ring_t) * RX_NUM_EL;
	tx_of_phys = tx_phys + sizeof(enet2_ring_t) * TX_NUM_EL;

	// Allocate receive buffers
	for( i=0; i<RX_NUM_EL; i++ ) {
		if( !(rxMBuf[i]=allocatePacket( NETWORK_BUFSIZE )) ) {
			printm("MolEnet: packet allocation failed\n");
			return false;
		}
		// reserve 2 bytes before the actual packet
		rxMBuf[i]->m_data += 2;
		rxMBuf[i]->m_len -= 2;
	}

	OSI_Enet2Cntrl( kEnet2Reset );
	result = OSI_Enet2RingSetup( kEnet2SetupRXRing, rx_phys, RX_NUM_EL )
		|| OSI_Enet2RingSetup( kEnet2SetupTXRing, tx_phys, TX_NUM_EL )
		|| OSI_Enet2RingSetup( kEnet2SetupRXOverflowRing, rx_of_phys, RX_NUM_EL )
		|| OSI_Enet2RingSetup( kEnet2SetupTXOverflowRing, tx_of_phys, TX_NUM_EL );
	if( result )
		return false;

	if( !resetAndEnable(false) )
		return false;

	// Create a table of supported media types.
	if( !createMediumTables() )
		return false;

	// Attach an IOEthernetInterface client.
	if( !attachInterface( (IONetworkInterface**)&networkInterface, false ) )
		return false;

	// Ready to service interface requests.
	networkInterface->registerService();

	printm("Ethernet driver 1.1\n");
	return true;
}

void
MolEnet::free()
{
	int i;
	
	if( is_open ) {
		OSI_Enet2Cntrl( kEnet2Reset );
		OSI_Enet2Close();
	}

	if( getWorkLoop() )
		getWorkLoop()->disableAllEventSources();

	if( _irq )
		_irq->release();

	if( transmitQueue )
		transmitQueue->release();

	if( networkInterface )
		networkInterface->release();

	if( rxMBufCursor )
		rxMBufCursor->release();
	if( txMBufCursor )
		txMBufCursor->release();

	if( mediumDict )
		mediumDict->release();

	if( rx_ring )
		IOFreeContiguous( rx_ring, sizeof(enet2_ring_t) * RX_NUM_EL );
	if( tx_ring )
		IOFreeContiguous( tx_ring, sizeof(enet2_ring_t) * TX_NUM_EL );

	if( workLoop ) {
		workLoop->release();
		workLoop = 0;
	}
	for( i=0; i<RX_NUM_EL; i++ )
		if( rxMBuf[i] )
			freePacket( rxMBuf[i] );

	// free packets in progress
	super::free();
}

bool
MolEnet::createWorkLoop()
{
	workLoop = IOWorkLoop::workLoop();

	return !!workLoop;
}

IOWorkLoop *
MolEnet::getWorkLoop() const
{
	return workLoop;
}


void
MolEnet::rxIRQ( IOInterruptEventSource *src, int /*count*/)
{
	bool replaced, flush=false;
	struct mbuf *pkt;
	int s;
	
	// printm("enet rx interrupt\n");

	do {
		while( (s=rx_ring[rx_head].psize) ) {

			if( !(pkt = replaceOrCopyPacket( &rxMBuf[rx_head], s, &replaced )) ) {
				// allocation failure, drop packet
				rx_ring[rx_head].psize = 0;
				rx_head = (rx_head + 1) & RX_MASK;
				continue;
			}
			networkInterface->inputPacket( pkt, s, IONetworkInterface::kInputOptionQueuePacket );
			flush = true;

			if( replaced ) {
				// reserve 2 bytes before the actual packet
				rxMBuf[rx_head]->m_data += 2;
				rxMBuf[rx_head]->m_len -= 2;
				updateRXDescriptor( rx_head );
			} else 
				rx_ring[rx_head].psize = 0;

			rx_head = (rx_head + 1) & RX_MASK;
		}

		// OSI_Enet2IRQAck performs an atomic ack
	} while( OSI_Enet2IRQAck(1,rx_head) );

	// Submit all queued packets to the network stack.
	if( flush )
		networkInterface->flushInputQueue();
}

#if 0 
void
MolEnet::txIRQ( IOInterruptEventSource *src, int /*count*/ )
{
	// Unstall the output queue
	transmitQueue->service();
}
#endif

UInt32
MolEnet::outputPacket( struct mbuf *pkt, void *param )
{
	struct IOPhysicalSegment segVector[2];
	int n;

	assert( pkt && netifEnabled );

	//printm("outputPacket %d\n", tx_tail );

	if( tx_ring[tx_tail].psize ) {
		/* Ring full. The txIRQ should call transmitQueue->service to unstall 
		 * the queue. At the moment we transmit each packet synchronously
		 * to the linux side. Thus the ring will never be full...
		 */ 
		printm("output stalled\n");
		return kIOReturnOutputStall;
	}

	n = txMBufCursor->getPhysicalSegmentsWithCoalesce( pkt, segVector );
	if( n < 0 || n > 2 ) {
		printm("getPhysicalSegments returned an error (%d)\n", n );
		freePacket(pkt);
		return kIOReturnOutputDropped;
	}

	tx_ring[tx_tail].flags = 0;
	if( n == 2 ) {
		tx_of_ring[tx_tail].buf = segVector[1].location;
		tx_of_ring[tx_tail].bsize = segVector[1].length;
		tx_of_ring[tx_tail].psize = segVector[1].length;
		tx_of_ring[tx_tail].flags = 0;
		tx_ring[tx_tail].flags = kEnet2SplitBufferFlag;
	}
	tx_ring[tx_tail].buf = segVector[0].location;
	tx_ring[tx_tail].bsize = segVector[0].length;
	eieio(); // gcc scheduling barrier
	tx_ring[tx_tail].psize = segVector[0].length;

	// Increase tail marker
	tx_tail = (tx_tail + 1) & TX_MASK;

	if( OSI_Enet2Kick() )
		printm("transmission error\n");

	freePacket( pkt );

	return kIOReturnOutputSuccess;
}

void
MolEnet::updateRXDescriptor( int ind )
{
	struct mbuf *pkt = rxMBuf[ind];
	struct IOPhysicalSegment segVector[2];
	int n;
	
	n = rxMBufCursor->getPhysicalSegmentsWithCoalesce( pkt, segVector );
	if( n < 1 || n > 2 ) {
		printm("UpdateRXDescriptor: Error\n");
	}

	rx_ring[ind].flags = 0;
	if( n == 2 ) {
		rx_of_ring[ind].buf = segVector[1].location;
		rx_of_ring[ind].bsize = segVector[1].length;
		rx_of_ring[ind].psize = 0;
		rx_of_ring[ind].flags = 0;	/* unused */
		rx_ring[ind].flags = kEnet2SplitBufferFlag;
	}
	rx_ring[ind].buf = segVector[0].location;
	rx_ring[ind].bsize = segVector[0].length;
	eieio();	// gcc scheduling barrier
	rx_ring[ind].psize = 0;
}

bool
MolEnet::resetAndEnable( bool enable )
{
	int i;
	ready = false;

	if( getWorkLoop() )
		getWorkLoop()->disableAllInterrupts();
	OSI_Enet2Cntrl( kEnet2Reset );

	// Initialize the link status.
	setLinkStatus( 0, 0 );

	// Reset ring buffers
	rx_head = tx_tail = 0;
	memset( rx_ring, 0, sizeof(enet2_ring_t) * RX_NUM_EL );
	memset( tx_ring, 0, sizeof(enet2_ring_t) * TX_NUM_EL );

	if( !enable )
		return true;

	for( i=0; i<RX_NUM_EL; i++ )
		updateRXDescriptor( i );

	OSI_Enet2Cntrl( kEnet2Start );
	if( getWorkLoop() )
		getWorkLoop()->enableAllInterrupts();

	ready = true;
	monitorLinkStatus( true );

	return true;
}


/*
 * Called by IOEthernetInterface client to enable the controller.
 * This method is always called while running on the default workloop
 * thread.
 */

IOReturn
MolEnet::enable( IONetworkInterface * netif )
{
	// If an interface client has previously enabled us,
	// and we know there can only be one interface client
	// for this driver, then simply return true.

	if( netifEnabled ) {
		printm("EtherNet(BMac): already enabled\n");
		return kIOReturnSuccess;
	}

	if( !ready && !resetAndEnable(true) )
		return kIOReturnIOError;

	// Record the interface as an active client.
	netifEnabled = true;

	// Start our IOOutputQueue object.
	transmitQueue->setCapacity( TRANSMIT_QUEUE_SIZE );
	transmitQueue->start();

	return kIOReturnSuccess;
}


/*
 * Called by IOEthernetInterface client to disable the controller.
 * This method is always called while running on the default workloop
 * thread.
 */

IOReturn
MolEnet::disable( IONetworkInterface * /*netif*/ )
{
	// Disable our IOOutputQueue object. This will prevent the
	// outputPacket() method from being called.
	transmitQueue->stop();

	// Flush all packets currently in the output queue.
	transmitQueue->setCapacity(0);
	transmitQueue->flush();

	// Disable the controller.
	resetAndEnable( false );
	//dropPower();
	netifEnabled = false;

	return kIOReturnSuccess;
}


/*
 * This function is quite brain-dead... eventually we might want to
 * be able to fake a link down though.
 */
void
MolEnet::monitorLinkStatus( bool firstPoll )
{
	IONetworkMedium *medium;
	UInt32 linkSpeed, linkStatus;
        IOMediumType mediumType;

	linkSpeed = 100;
	linkStatus = kIONetworkLinkValid | kIONetworkLinkActive;
	mediumType = kIOMediumEthernet100BaseTX | kIOMediumOptionFullDuplex;

	if( firstPoll || !linkStatusReported ) {
		linkStatusReported = true;

		medium = IONetworkMedium::getMediumWithType(mediumDict, mediumType);
		setLinkStatus( linkStatus, medium, linkSpeed * 1000000 );

		if( linkStatus & kIONetworkLinkActive )
			printm( "MolEnet: Link up at %ld Mbps - %s Duplex\n",
				linkSpeed, (mediumType & kIOMediumOptionFullDuplex) ? "Full" : "Half" );
		else
			printm( "MolEnet: Link down\n" );
	}
}

const OSString * MolEnet::newVendorString() const {
	return OSString::withCString("Ibrium");
}

const OSString * MolEnet::newModelString() const {
	return OSString::withCString("MolEnet");
}

const OSString * MolEnet::newRevisionString() const {
	return OSString::withCString("");
}

IOReturn
MolEnet::setPromiscuousMode( IOEnetPromiscuousMode mode )
{
	return kIOReturnSuccess;
}

IOReturn
MolEnet::setMulticastMode( IOEnetMulticastMode mode )
{
	//multicastEnabled = (mode == kIOEnetMulticastModeOff) ? false : true;
	return kIOReturnSuccess;
}

IOReturn
MolEnet::setMulticastList(IOEthernetAddress *addrs, UInt32 count)
{
	return kIOReturnSuccess;
}

static struct { UInt32 type, speed; } mediumTable[] = {
	{ kIOMediumEthernetNone,					0	},
	{ kIOMediumEthernetAuto,					0	},
	{ kIOMediumEthernet100BaseTX  | kIOMediumOptionFullDuplex,	100	},
};

bool
MolEnet::createMediumTables()
{
	IONetworkMedium *medium;
	UInt32 i;

	mediumDict = OSDictionary::withCapacity( sizeof(mediumTable)/sizeof(mediumTable[0]) );
	if( !mediumDict )
		return false;

	for( i=0; i<sizeof(mediumTable)/sizeof(mediumTable[0]); i++ ) {
		medium = IONetworkMedium::medium( mediumTable[i].type, mediumTable[i].speed );
		if( medium ) {  
			IONetworkMedium::addMedium( mediumDict, medium );
			medium->release();
		}
	}

	if( !publishMediumDictionary( mediumDict ) )
		return false;

	medium = IONetworkMedium::getMediumWithType( mediumDict, kIOMediumEthernetAuto );
	setCurrentMedium( medium );

	return true;    
}


/*
 * Create a WorkLoop serialized output queue object.
 */
IOOutputQueue *
MolEnet::createOutputQueue()
{
	//return IOGatedOutputQueue::withTarget( this, getWorkLoop(), TRANSMIT_QUEUE_SIZE );
	return IOBasicOutputQueue::withTarget( this, TRANSMIT_QUEUE_SIZE );
}

/*
 * Power management methods. 
 */
IOReturn
MolEnet::registerWithPolicyMaker( IOService * policyMaker )
{
#define number_of_power_states 2

	static IOPMPowerState ourPowerStates[number_of_power_states] = {
		{1,0,0,0,0,0,0,0,0,0,0,0},
		{1,IOPMDeviceUsable,IOPMPowerOn,IOPMPowerOn,0,0,0,0,0,0,0,0}
        };
	currentPowerState = 1;

	return policyMaker->registerPowerDriver( this, ourPowerStates, number_of_power_states );
}


IOReturn
MolEnet::setPowerState( unsigned long powerStateOrdinal, IOService *whatDevice )
{
	IOReturn ret = IOPMAckImplied;

	//printm("MolEnet: setPowerState %d\n", powerStateOrdinal);

	if( currentPowerState == powerStateOrdinal )
		return IOPMAckImplied;
	
	switch( powerStateOrdinal ) {
	case 0:
		printm("MolEnet: powering off\n");
		currentPowerState = powerStateOrdinal;
		break;
		
	case 1:
		printm("MolEnet: powering on\n");
		currentPowerState = powerStateOrdinal;
		break;

	default:
		ret = IOPMNoSuchState;
		break;
	}
	return ret;
}

void
MolEnet::getPacketBufferConstraints( IOPacketBufferConstraints *r ) const
{
	super::getPacketBufferConstraints( r );

	r->alignStart = kIOPacketBufferAlign4;
	r->alignLength = kIOPacketBufferAlign4;
}

IOReturn
MolEnet::getHardwareAddress( IOEthernetAddress *addrP )
{
	unsigned char *p = addrP->bytes;
	OSI_Enet2GetHWAddr( p );

	/* printm("getHardwareAddress: %02x:%02x:%02x:%02x:%02x:%02x\n",
			   p[0], p[1], p[2], p[3], p[4], p[5] ); */
	return kIOReturnSuccess;
}
