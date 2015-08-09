/* 
 *   Creation Date: <2002/07/14 19:25:40 samuel>
 *   Time-stamp: <2003/02/22 12:49:37 samuel>
 *   
 *	<MolBlockDevice.cpp>
 *	
 *	MolBlockDevice
 *   
 *   Copyright (C) 2001-2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOPlatformExpert.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/storage/IOBlockStorageDriver.h>
#include <IOKit/storage/IOStorageDeviceCharacteristics.h>
#include "MolBlockDevice.h"
#include "osi_calls.h"

#define super IOService
OSDefineMetaClassAndStructors( MolBlockDriver, IOService );

#define DESC_NUM_EL	128
#define DESC_MASK	(DESC_NUM_EL - 1)

typedef struct MolBlockRequest {
	queue_chain_t		chain;
	
	// filled in by the caller
	int			unit;
	int			ablk_req;
	IOMemoryDescriptor	*buffer;
	UInt32			block;
	UInt32			nblks;
	IOStorageCompletion	completion;

	// private to the transport layer
	int			xfer_cnt;	// -1 before header has been sent
	int		       	total_cnt;	// total #bytes to transfer
	IOReturn		ret;
	DECLARE_TIMESTAMP

	// valid when on the completion queue
	int		       	req_num;	// used for completion determination
} MolBlockRequest;

#define kMOLMessageDiskEvent	0x1378133a	// hopefully this ID doesn't clash...


/************************************************************************/
/*	Transport layer (BlockDriver)					*/
/************************************************************************/

bool
MolBlockDriver::start( IOService *theProvider )
{
	IOPhysicalAddress phys;
	IOPlatformDevice *provider = OSDynamicCast( IOPlatformDevice, theProvider );

	if( !provider || !super::start( provider ) )
		return false;

	// get rid of 'disk' nodes (interferes with the boot volume matching)
	OSIterator *iter = provider->getChildIterator( gIODTPlane );
	if( iter ) {
		IORegistryEntry *r;
		while( (r=OSDynamicCast(IORegistryEntry, iter->getNextObject())) )
			if( !strcmp( "disk", r->getName() ))
				r->detachFromParent( provider, gIODTPlane );
		iter->release();
	}
	
	// for now we support just a single channel
	_channel = 0;
	queue_init(&_reqQueue);
	queue_init(&_complQueue);

	if( !createWorkLoop() )
		return false;

	if( OSI_ABlkCntrl( _channel, kABlkReset ) )
		return false;

	// allocate the descriptor ring
	_ring = (ablk_req_head_t*)IOMallocContiguous( DESC_NUM_EL * sizeof(ablk_req_head_t),
						      sizeof(ablk_req_head_t), &phys );
	if( !_ring ) {		
		printm("Blk: Ring allocation failed\n");
		return false;
	}

	// initialize the descriptor ring
	memset( _ring, 0, DESC_NUM_EL * sizeof(ablk_req_head_t) );
	_ri = 0;
	_reqCount = 0;
	_ring[0].flags = ABLK_NOP_REQ;
	if( OSI_ABlkRingSetup( _channel, phys, DESC_NUM_EL ) ) {
		printm("Blk: Ring setup failed\n");
		return false;
	}
		
	// setup command gate
	_cmdGate = IOCommandGate::commandGate( this, (IOCommandGate::Action)&MolBlockDriver::receiveCmd );
	if( !_cmdGate || workLoop->addEventSource(_cmdGate) != kIOReturnSuccess) {
		printm("Blk: Couldn't create command gate\n");
		return false;
	}
	
	// setup interrupts
	_irq = IOInterruptEventSource::interruptEventSource( this, 
				(IOInterruptEventAction)&MolBlockDriver::handleIRQ, provider, 0 );
	if( !_irq || workLoop->addEventSource(_irq) != kIOReturnSuccess ) {
		printm("Blk: failed to get irq\n");
		return false;
	}

	registerService();

	// start channel
	if( OSI_ABlkCntrl( _channel, kABlkStart ) ) {
		printm("Blk: Failed to start channel\n");
		return false;
	}
	workLoop->enableAllInterrupts();

	// construct nubs for the units
	if( !publishNubs( provider ) ) {
		printm("Blk: failed to publish nubs\n");
		return false;
	}

	printm("Block Driver v1.1\n");
	return true;
}

void
MolBlockDriver::free()
{
	if( workLoop )
		workLoop->disableAllEventSources();

	if( _irq )
		_irq->release();

	if( workLoop ) {
		OSI_ABlkCntrl( kABlkReset, _channel );
		workLoop->release();
		workLoop = 0;
	}
}

bool
MolBlockDriver::publishNubs( IOPlatformDevice *provider )
{
	int i;
	
	_nunits = 0;
	
	// Single channel for now
	for( i=0 ;; i++ ) {
		MolBlockDevice *nub = new MolBlockDevice;

		if( !nub )
			return false;

		if( !nub->init( _channel, i ) ) {
			nub->free();	/* calls delete */
			break;
		}

		if( !nub->attach( this ) )
			panic("Blk: attach failed\n");

		_nunits++;
		nub->registerService();

		// The IORegistry retains the nub, so we can release or refcount
		// because we don't need it any more.
		//nub->release();
	}
	return true;
}

bool
MolBlockDriver::createWorkLoop()
{
	workLoop = IOWorkLoop::workLoop();
	return !!workLoop;
}

IOWorkLoop *
MolBlockDriver::getWorkLoop() const
{
	return workLoop;
}

void
MolBlockDriver::handleIRQ( IOInterruptEventSource *src, int /*count*/ )
{
	MolBlockRequest *req;
	int compl_count, running, event;
	
	if( !OSI_ABlkIRQAck( _channel, &compl_count, &running, &event ) )
		return;
	_running = running;

	// handle events (like disk insert)
	if( event )
		messageClients( kMOLMessageDiskEvent, NULL );
	
	// process completions
	while( (req=(MolBlockRequest*)dequeue_head( &_complQueue )) ) {
		if( req->req_num - compl_count > 0 ) {
			enqueue_head( &_complQueue, (queue_entry_t)req );
			break;
		}
		COLLECT_STAT( req );
		
		/* printm("Blk completion, blk: %x count: %x res_count: %d, status: %d\n",
				req->block, req->nblks, req->res_count, req->ret ); */

		if( req->completion.action ) {
			int count = max( req->xfer_cnt, 0 );
			req->completion.action( req->completion.target, req->completion.parameter, 
						req->ret, count );
		}
		if( req->buffer )
			req->buffer->release();
		IOFree( req, sizeof(MolBlockRequest) );
	}

	// process request queue
	while( (req=(MolBlockRequest*)dequeue_head(&_reqQueue)) )
		if( !processRequest( req ) )
			break;
}

// the eieio() is used as a scheduler barrier for gcc
#define START_REQUEST(prev)	do { eieio(); prev->proceed = true; } while(0)

bool
MolBlockDriver::processRequest( MolBlockRequest *req )
{
	IOMemoryDescriptor *b = req->buffer;
	ablk_req_head_t *prev, *d;
	bool ret = true;
	UInt32 count;
	int next;

	for( ;; ) {
		next = ((_ri+1) & DESC_MASK);
		prev = &_ring[_ri];
		d = &_ring[next];

		// hardware saturated?
		if( d->flags ) {
			enqueue_head( &_reqQueue, (queue_entry_t)req );
			ret = false;
			next = _ri;
			break;
		}
		_ri = next;
		
		// set the completion request number
		req->req_num = ++_reqCount;
		d->proceed = false;

		// start new command
		if( req->xfer_cnt < 0 ) {
			req->xfer_cnt = 0;
			d->flags = req->ablk_req;
			d->param = req->block;
			d->unit = req->unit;
			if( !req->total_cnt ) {
				d->flags |= ABLK_RAISE_IRQ;
				enqueue_tail( &_complQueue, (queue_entry_t)req );
				START_REQUEST(prev);
				break;
			}
			START_REQUEST(prev);
			continue;
		}

		// write scatter and gather buffers
		ablk_sg_t *dd = (ablk_sg_t*)d;

		if( !(dd->buf = b->getPhysicalSegment(req->xfer_cnt, (UInt32*)&count)) ) {
			// report the error when the request is compeleted...
			printm("buffer to small!\n");
			d->flags = ABLK_NOP_REQ;
			req->ret = kIOReturnError;
			enqueue_tail( &_complQueue, (queue_entry_t)req );
			START_REQUEST(prev);
			break;
		}

		req->xfer_cnt += count;
		if( req->xfer_cnt > req->total_cnt ) {
			count -= req->xfer_cnt - req->total_cnt;
			req->xfer_cnt = req->total_cnt;
		}
		dd->count = count;
		
		// request complete?
		if( req->xfer_cnt == req->total_cnt ) {
			dd->flags = ABLK_SG_BUF | ABLK_RAISE_IRQ;
			enqueue_tail( &_complQueue, (queue_entry_t)req );
			START_REQUEST(prev);
			//printm("[%d] started\n", req->req_num );
			break;
		}
		dd->flags = ABLK_SG_BUF;
		START_REQUEST(prev);
	}

	if( !_running ) {
		_running = true;
		OSI_ABlkKick( _channel );
	}

	// returns false if hardware is saturated
	return ret;
}


/* this function is single threaded with respect to other work-loop requests. 
 * However, it does not necessarily run on the work-loop thread.
 */
IOReturn
MolBlockDriver::receiveCmd( void *newRequest, void *, void *, void * )
{
	MolBlockRequest *req = (struct MolBlockRequest*)newRequest;	

	REQ_TIMESTAMP(req);
	req->xfer_cnt = -1;
	if( req->buffer ) {
		req->total_cnt = req->nblks * 512;
		req->buffer->retain();
	} else {
		req->total_cnt = 0;
	}
	req->ret = kIOReturnSuccess;	

	// If the request queue is empty, try to feed it to the hardware
	if( queue_empty( &_reqQueue ) ) {
		(void)processRequest( req );
	} else {
		enqueue_tail( &_reqQueue, (queue_entry_t)req );
	}
	return kIOReturnSuccess;
}


/************************************************************************/
/*	The nub object							*/
/************************************************************************/

#undef super
#define super IOBlockStorageDevice
OSDefineMetaClassAndStructors( MolBlockDevice, IOBlockStorageDevice );

bool
MolBlockDevice::init( int channel, int unit )
{
	if( !super::init(0) )
		return false;

	_unit = unit;
	_channel = channel;

	if( OSI_ABlkDiskInfo( channel, unit, &_info ) )
		return false;

	// This seems to be necessary in order to get device node
	// entries for disk partitions (used to determine boot volume)

	OSNumber *number = OSNumber::withNumber( _unit, 32 );
	setProperty( "IOUnit", (OSObject*) number );
	number->release();

	// Does this unit have a partition table?
	IOPhysicalAddress phys;
	char *buf = (char*)IOMallocContiguous( 512, 0 /*alignment*/, &phys );
	memset( buf, 0, 512 );
	OSI_ABlkSyncRead( channel, unit, 0, phys, 512 );
	if( *(short*)buf == 0x4552 /* DESC_MAP_SIGNATURE */ )
		setProperty("HasPartable", true);
	IOFreeContiguous( buf, 512 );

	_oldMediaPresent = false;
	_mediaPresent = (_info.flags & ABLK_INFO_MEDIA_PRESENT) ? 1:0;

	// this is necessary in order to get an icon
	OSDictionary *dict = OSDictionary::withCapacity(1);
	OSString *location = OSString::withCString(kIOPropertyInternalKey);
	if( dict && location ) {
		dict->setObject( kIOPropertyPhysicalInterconnectLocationKey, location );
		setProperty( kIOPropertyProtocolCharacteristicsKey, dict );
	}
	if( location )
		location->release();
	if( dict )
		dict->release();

	return true;
}

bool
MolBlockDevice::attach( IOService *provider )
{
	if( !super::attach(provider) )
		return false;
	
	_driver = OSDynamicCast( MolBlockDriver, provider );
	if( !_driver )
		return false;

	
	return true;
}

IOReturn
MolBlockDevice::doAsyncReadWrite( IOMemoryDescriptor *buffer, UInt32 block, UInt32 nblks,
				  IOStorageCompletion completion)
{
	MolBlockRequest	*req = (MolBlockRequest*)IOMalloc(sizeof(MolBlockRequest));

	if( !req )
		return kIOReturnNoMemory;
	if( (_info.flags & ABLK_INFO_READ_ONLY) && buffer->getDirection() == kIODirectionOut )
		return kIOReturnNotWritable;
	// printm("doAsyncReadWrite (nub) blk: %x nblks: %x\n", block, nblks );
	
	req->unit = _unit;
	req->ablk_req = (buffer->getDirection() == kIODirectionOut) ? ABLK_WRITE_REQ : ABLK_READ_REQ;
	req->buffer = buffer;
	req->block = block;
	req->nblks = nblks;
	req->completion = completion;

	return _driver->_cmdGate->runCommand( req, NULL, NULL, NULL );
}

IOReturn 
MolBlockDevice::doSyncReadWrite( IOMemoryDescriptor *buffer, UInt32 block, UInt32 nblks )
{
	printm("doSyncReadWrite\n");
	return kIOReturnUnsupported;
}

IOReturn
MolBlockDevice::doEjectMedia()
{
	MolBlockRequest	*req;

	if( !(_info.flags & ABLK_INFO_REMOVABLE) )
		return kIOReturnUnsupported;

	if( !(req=(MolBlockRequest*)IOMalloc(sizeof(MolBlockRequest))) )
		return kIOReturnNoMemory;

	req->ablk_req = ABLK_EJECT_REQ;
	req->unit = _unit;
	req->buffer = NULL;
	req->block =  0;
	req->nblks = 0;
	req->completion.action = NULL;

	_mediaPresent = false;

	return _driver->_cmdGate->runCommand( req, NULL, NULL, NULL );
}

IOReturn
MolBlockDevice::doFormatMedia( UInt64 byteCapacity )
{
	return kIOReturnUnsupported;
}

UInt32
MolBlockDevice::doGetFormatCapacities( UInt64 * capacities,
				       UInt32 capacitiesMaxCount ) const
{
	return kIOReturnUnsupported;
}
										
IOReturn
MolBlockDevice::doLockUnlockMedia( bool doLock )
{
	return kIOReturnUnsupported;
}

IOReturn
MolBlockDevice::doSynchronizeCache()
{
	return kIOReturnSuccess;
}

char *MolBlockDevice::getVendorString() { return "Ibrium"; }
char *MolBlockDevice::getProductString() { return "MOL-Disk"; }
char *MolBlockDevice::getRevisionString() { return "1.0"; }
char *MolBlockDevice::getAdditionalDeviceInfoString() { return ""; }

IOReturn
MolBlockDevice::reportBlockSize( UInt64 *blockSize )
{
	if( blockSize )
		*blockSize = 512;
	return kIOReturnSuccess;
}

IOReturn
MolBlockDevice::reportMaxReadTransfer( UInt64 blockSize, UInt64 *max)
{
	*max = 16*1024*1024;	// FIXME
	return kIOReturnSuccess;
}

IOReturn
MolBlockDevice::reportMaxWriteTransfer( UInt64 blockSize, UInt64 *max )
{
	*max = 1024*1024;	// FIXME
	return kIOReturnSuccess;
}

IOReturn
MolBlockDevice::reportMaxValidBlock( UInt64 *maxBlock )
{
	if( maxBlock )
		*maxBlock = _info.nblks ? _info.nblks - 1 : 0;
	return kIOReturnSuccess;
}

IOReturn
MolBlockDevice::reportMediaState( bool *mediaPresent, bool *changedState )
{
	if( mediaPresent )
		*mediaPresent = _mediaPresent;
	if( changedState ) 	
		*changedState = !_oldMediaPresent;
	_oldMediaPresent = _mediaPresent;
	return kIOReturnSuccess;
}

IOReturn
MolBlockDevice::reportPollRequirements( bool *pollRequired, bool *pollIsExpensive )
{
	if( pollRequired )
		*pollRequired = false;
	if( pollIsExpensive )
		*pollIsExpensive = false;
	return kIOReturnSuccess;
}

IOReturn
MolBlockDevice::reportEjectability( bool *isEjectable )
{
	*isEjectable = (_info.flags & ABLK_INFO_REMOVABLE)? true : false;
	return kIOReturnSuccess;
}

IOReturn
MolBlockDevice::reportLockability( bool *isLockable )
{
	*isLockable = false;
	return kIOReturnSuccess;
}

IOReturn
MolBlockDevice::reportRemovability( bool *isRemovable )
{
	*isRemovable = (_info.flags & ABLK_INFO_REMOVABLE)? true : false;
	return kIOReturnSuccess;
}

IOReturn
MolBlockDevice::reportWriteProtection( bool *isWriteProtected )
{
	*isWriteProtected = (_info.flags & ABLK_INFO_READ_ONLY) ? 1:0;
	return kIOReturnSuccess;
}


IOReturn
MolBlockDevice::message( UInt32 type, IOService *provider, void *arg )
{
	int event;
	
	switch( type ) {
	case kMOLMessageDiskEvent:
		if( (event=OSI_ABlkCntrl1( _channel, kABlkGetEvents, _unit )) /*> 0*/ ) {
			_mediaPresent = true;
			OSI_ABlkDiskInfo( _channel, _unit, &_info );
			if( event & ABLK_EVENT_DISK_INSERTED )
				messageClients( kIOMessageMediaStateHasChanged, (void*)kIOMediaStateOnline );
		}
		return kIOReturnSuccess;
	}
	return super::message( type, provider, arg );
}

/************************************************************************/
/*	Statistics							*/
/************************************************************************/

#ifdef COLLECT_STATISTICS

/* XXX: Assumes a 25 MHz timebase */
#define TB_MHZ		25

#define PRINT_MS(t)	(int)((t)/TB_MHZ/1000), (int)(((t)/TB_MHZ) % 1000)

static void
collect_stat( BlkStat *r, BlkStat *w, MolBlockRequest *req ) 
{

	UInt32 now = get_tbl();
	UInt32 d = now - req->timestamp;
	bool is_write = (req->buffer->getDirection() == kIODirectionOut);
	BlkStat *s = is_write ? w : r;

	d = now - req->timestamp;

	if( !s->nreq )
		s->min_latency = d;
	s->min_latency = min( d, s->min_latency );
	s->max_latency = max( d, s->max_latency );
	s->latency_sum += d;
	s->nreq++;
	s->xfer_cnt += req->xfer_cnt;

	if( d/TB_MHZ < 3000 )
		s->cache_hit++;
	else
		s->latency_sum_nocache += d;

	if( now - s->timestamp > TB_MHZ * 1000000 ) {
		int n = s->nreq - s->cache_hit;
		int avg = (n > 0)? s->latency_sum_nocache / n : 0;
		
		printm("%s min %3d.%03d, max %2d.%03d, avg %2d.%03d / %3d.%03d, # %3d/%-3d.  %6d K\n", 
		       is_write ? "write:" : "read: ",
		       PRINT_MS( s->min_latency ),
		       PRINT_MS( s->max_latency ),
		       PRINT_MS( s->latency_sum / s->nreq ),
		       PRINT_MS( avg ),
		       s->nreq, s->cache_hit,
		       s->xfer_cnt/1024 );

		s->min_latency = 0;
		s->max_latency = 0;
		s->latency_sum = 0;
		s->latency_sum_nocache = 0;
		s->nreq = 0;
		s->xfer_cnt = 0;
		s->cache_hit = 0;
		s->timestamp = now;
	}
}

#endif
