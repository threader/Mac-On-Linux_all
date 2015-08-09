/*
 * Copyright (c) 2001, 2002, 2003 Samuel Rydh
 */

#ifndef _IOKIT_MOLBLOCKDEVICE_H
#define _IOKIT_MOLBLOCKDEVICE_H

#include <IOKit/storage/IOBlockStorageDevice.h>
#include <IOKit/IOInterruptEventSource.h>
#include <IOKit/IOCommandGate.h>
#include "ablk_sh.h"

//#define COLLECT_STATISTICS


/************************************************************************/
/*	Statistics							*/
/************************************************************************/

#ifdef COLLECT_STATISTICS

static inline UInt32 get_tbl() {
	UInt32 ret;
	asm ("mftb %0" : "=r" (ret) : );
	return ret;
}
typedef struct {
	UInt32			min_latency;
	UInt32			max_latency;
	UInt32			latency_sum;
	UInt32			latency_sum_nocache;
	UInt32			xfer_cnt;
	UInt32			nreq;
	UInt32			timestamp;
	UInt32			cache_hit;
} BlkStat;

#define DECLARE_STATS		BlkStat read_stat, write_stat;
#define COLLECT_STAT(r)		collect_stat( &read_stat, &write_stat, r )
#define DECLARE_TIMESTAMP	UInt32 timestamp;
#define REQ_TIMESTAMP(r)	r->timestamp = get_tbl()
static void			collect_stat( BlkStat *r, BlkStat *w, struct MolBlockRequest *req ) ;

#else

#define DECLARE_STATS		
#define DECLARE_TIMESTAMP
#define COLLECT_STAT(r)		do {} while(0)
#define REQ_TIMESTAMP(r)	do {} while(0)

#endif /* COLLECT_STATISTICS */


/************************************************************************/
/*	Transport layer 						*/
/************************************************************************/

class MolBlockDriver : public IOService {
	
	OSDeclareDefaultStructors( MolBlockDriver );

 private:
	int			_channel;
	int			_nunits;

	IOWorkLoop		*workLoop;
	IOInterruptEventSource	*_irq;

	ablk_req_head_t		*_ring;
	int			_ri;		// ring index
	int			_reqCount;	// request count

        queue_head_t		_reqQueue;	// Not passed to the hardware
        queue_head_t		_complQueue;	// Waiting on completion
	
	int			_running;	// The ring is beeing processed

	/**************************************************************/

	void			handleIRQ( IOInterruptEventSource *src, int count );
	bool			publishNubs( IOPlatformDevice *provider );

	IOReturn		receiveCmd( void *newRequest, void *, void *, void * );
	bool			processRequest( struct MolBlockRequest *req );

 public:
	IOCommandGate		*_cmdGate;

	virtual bool		start( IOService *provider );
	virtual void		free();

	virtual IOWorkLoop	*getWorkLoop() const;
	bool			createWorkLoop();

	DECLARE_STATS
};


/************************************************************************/
/*	Nub object (also used to hold unit data)			*/
/************************************************************************/


class MolBlockDevice : public IOBlockStorageDevice {

	OSDeclareDefaultStructors( MolBlockDevice );

private:
	MolBlockDriver		*_driver;
	
	ablk_disk_info_t	_info;
	int			_unit;
	int			_channel;

	bool			_mediaPresent;
	bool			_oldMediaPresent;
	
public:
	bool			init( int channel, int unit );
	virtual bool		attach( IOService *provider );

	virtual IOReturn	doAsyncReadWrite( IOMemoryDescriptor *buffer, UInt32 block,UInt32 nblks,
						  IOStorageCompletion completion );
	virtual IOReturn	doSyncReadWrite( IOMemoryDescriptor *buffer,
						 UInt32 block,UInt32 nblks );

	virtual IOReturn	doEjectMedia();
	virtual IOReturn	doFormatMedia( UInt64 byteCapacity );
	virtual UInt32		doGetFormatCapacities( UInt64 *capacities,
						       UInt32 capacitiesMaxCount ) const;
	virtual IOReturn	doLockUnlockMedia( bool doLock );
	virtual IOReturn	doSynchronizeCache();

	virtual IOReturn	message( UInt32 type, IOService *provider, void *arg );

	virtual char *		getVendorString();
	virtual char *		getProductString();
	virtual char *		getRevisionString();
	virtual char *		getAdditionalDeviceInfoString();
	virtual IOReturn 	reportBlockSize( UInt64 *blockSize );
	virtual IOReturn	reportEjectability( bool *isEjectable );
	virtual IOReturn	reportLockability( bool *isLockable );
	virtual IOReturn	reportMaxReadTransfer( UInt64 blockSize,UInt64 *max );
	virtual IOReturn	reportMaxWriteTransfer( UInt64 blockSize,UInt64 *max );
	virtual IOReturn	reportMaxValidBlock( UInt64 *maxBlock );
	virtual IOReturn	reportMediaState( bool *mediaPresent,bool *changedState );
	virtual IOReturn	reportPollRequirements( bool *pollRequired, bool *pollIsExpensive );
	virtual IOReturn	reportRemovability( bool *isRemovable );
	virtual IOReturn	reportWriteProtection( bool *isWriteProtected );
};

#endif
