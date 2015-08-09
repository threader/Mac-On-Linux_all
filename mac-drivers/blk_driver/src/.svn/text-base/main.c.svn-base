/* 
 *   Creation Date: <1999/03/20 07:30:18 samuel>
 *   Time-stamp: <2003/02/02 20:25:33 samuel>
 *   
 *	<driver.c>
 *	
 *	Native MOL Block Driver
 *   
 *   Copyright (C) 1999, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *
 *   NOTE: This driver has three world visible symbols:
 *
 *	CFMInitialize, CFMTerminate, DoDriverIO
 */
  
#include <DriverServices.h>
#include <Disks.h>
#include "global.h"
#include "util.h"
#include "logger.h"
#include "LinuxOSI.h"
#include "queue.h"
#include "unit.h"
#include "misc.h"
#include "IRQUtils.h"
#include "tasktime.h"

#define SYNC_HACK

#define RING_NUM_EL		128
#define RING_MASK		(RING_NUM_EL-1)
#define NUM_PHYSTAB_EL		128		/* 64 * 4K max atomic transfer size */
#define MAX_REQUESTS		8		/* max #uncompleted requests */

#define OBUF_SIZE		0x1000		/* overflow buffer size */

struct channel {
	int			channel;	/* only a single channel is supported for now */
	unit_t			*units;
	struct channel		*next;
	int			running;	/* mol engine is runnign */

	/* MOL communication */
	long			ring_lock;	/* atomic, set to 1 when the ring is accessed */
	int			ri;		/* ring index */
	int			req_count;	/* request count (used for completion etc) */
	ablk_req_head_t		*ring;		/* work ring */

	/* request queues */
	queue_t			req_queue;	/* dequeue serialized by ring_lock */
	queue_t			compl_queue;	/* dequeue serialized by IRQ */ 
	fifo_t			free_fifo; 

	IRQInfo			irq_info;

	/* overflow buffer, used when prepare memory for IO fails */
	int			obuf_completion;
	char			*obuf;
	ulong			obuf_phys;
	int			obuf_cnt;
	char			*obuf_dest;
};

typedef struct {
	queue_el_t		qElem;		/* must go first */

	IOCommandID		cmdID;
	int			unit;
	int			ablk_req;	/* ABLK_REQ_xxx */
	uint			param;		/* first block for ABLK_{READ,WRITE}_REQ */
	uint			total_cnt;	/* total #bytes to transfer */
	int			xfer_cnt;	/* -1 before header has been sent */
	ParamBlockRec		*pb;

	int			req_num;	/* only valid in the completion queue */

	int			mem_prepared;
	IOPreparationTable	ioprep;
	ulong			phystab[ NUM_PHYSTAB_EL ];
	
} request_t;

static channel_t 		*channels;


#ifdef SYNC_HACK
static int			sync_event_sent=0;
static int			no_sync_hack=0;

static void
turn_off_sync( int p1, int p2 )
{
	//lprintf("turn_of_sync\n");
	no_sync_hack = 1;
}
#endif

/************************************************************************/
/*	misc								*/
/************************************************************************/

unit_t *
get_first_unit( channel_t *ch ) 
{
	return ch->units;
}

unit_t *
get_unit( int drv_num )
{
	channel_t *ch;
	unit_t *unit;
	
	for( ch=channels; ch; ch=ch->next )
		for( unit=ch->units; unit; unit=unit->next ) 
			if( unit->drv_num == drv_num )
				return unit;
	//lprintf("get_unit: no such unit (%d)\n", drv_num );
	return NULL;
}

void
add_unit( channel_t *ch, unit_t *unit ) 
{
	unit_t **pp = &ch->units;
	
	/* add to end of list (this feature causes driver nums to be slightly more stable */
	unit->next = NULL;
	for( ; *pp ; pp=&(**pp).next )
		;
	*pp = unit;
}

int
get_channel_num( channel_t *ch )
{
	return ch->channel;
}

void
mount_drives_hack( void ) 
{
	channel_t *ch;
	unit_t *unit;

	for( ch=channels; ch; ch=ch->next )
		for( unit=ch->units; unit; unit=unit->next )
			if( !unit->mounted && !unit->no_disk )
				PostEvent( diskEvt, unit->drv_num );
}


/************************************************************************/
/*	I/O								*/
/************************************************************************/

static inline UInt32
get_physical_segment( request_t *r, int *ret_count ) 
{
	int first_cnt, cnt, i, offs=r->xfer_cnt;
	ulong base;
	
	if( r->ioprep.lengthPrepared <= offs )
		return NULL;

	first_cnt = 0x1000 - (r->phystab[0] & 0xfff);
	if( !offs ) {
		base = r->phystab[0];
		cnt = first_cnt;
		i = 1;
	} else {
		i = 1 + (offs - first_cnt) / 0x1000;
		base = r->phystab[i++];
		cnt = 0x1000;
	}
	/* combine physically continuous entries */
	while( offs + cnt < r->ioprep.lengthPrepared && r->phystab[i] == base + cnt )
		cnt += 0x1000;

	if( offs + cnt > r->total_cnt )
		cnt = r->total_cnt - offs;

	*ret_count = cnt;
	/* lprintf("get_physical_segment: %08lx (%x)\n", base, cnt); */
	return base;
}

/* The volatile stuff will hopefully prevent the compiler from reordering stuff */
#define START_REQUEST(prev) \
	do { *(volatile int*)&prev->proceed = 1; } while(0)

/* returns a non-zoro status if the request ring is sturated */
static inline int
process_request( channel_t *ch, request_t *req )
{
	ablk_req_head_t *prev, *d;
	ablk_sg_t *dd;
	int next, count, ret=0;
	
	for( ;; ) {
		next = ((ch->ri+1) & RING_MASK);
		prev = &ch->ring[ch->ri];
		d = &ch->ring[next];

		/* hardware saturated? */
		if( d->flags ) {
			enqueue_tail( &ch->req_queue, (queue_el_t*)req );
			ret = 1;
			break;
		}
		ch->ri = next;
		
		/* set the completion request number */
		req->req_num = ++(ch->req_count);
		d->proceed = 0;

		/* start new command */
		if( req->xfer_cnt < 0 ) {
			req->xfer_cnt = 0;
			d->flags = req->ablk_req;
			d->param = req->param;	/* block */
			d->unit = req->unit;
			if( !req->total_cnt ) {
				d->flags |= ABLK_RAISE_IRQ;
				enqueue( &ch->compl_queue, (queue_el_t*)req );
				START_REQUEST(prev);
				break;
			}
			START_REQUEST(prev);
			continue;
		}
		
		/* handle scatter and gather buffers */
		dd = (ablk_sg_t*)d;

		if( !(dd->buf=get_physical_segment(req, &count)) ) {
			/* use the overflow buffer */
			if( ch->obuf_cnt ) {
				/* overflow buf already in use... back off */
				enqueue_tail( &ch->req_queue, (queue_el_t*)req );
				ch->req_count--;
				ch->ri = ((ch->ri-1) & RING_MASK);
				ret = 1;
				break;
			}
			if( (count=req->total_cnt-req->xfer_cnt) > OBUF_SIZE )
				count = OBUF_SIZE;
			dd->buf = ch->obuf_phys;
			ch->obuf_cnt = count;
			ch->obuf_completion = req->req_num;
			if( req->ablk_req == ABLK_READ_REQ ) {
				ch->obuf_dest = req->pb->ioParam.ioBuffer + req->xfer_cnt;
			} else if( req->ablk_req == ABLK_WRITE_REQ ) {
				char *src = req->pb->ioParam.ioBuffer + req->xfer_cnt;
				char *dest = ch->obuf;
				int cnt = count;

				ch->obuf_dest = NULL;
				while( cnt-- )
					*dest++ = *src++;
			}
		}
		req->xfer_cnt += count;
		dd->count = count;
		
		/* request complete? */
		if( req->xfer_cnt == req->total_cnt ) {
			dd->flags = ABLK_SG_BUF | ABLK_RAISE_IRQ;
			enqueue( &ch->compl_queue, (queue_el_t*)req );
			START_REQUEST(prev);
			break;
		}
		dd->flags = ABLK_SG_BUF;
		START_REQUEST(prev);
	}

	if( !ch->running ) {
		ch->running = 1;
		OSI_ABlkKick( ch->channel );
	}
	return ret;
}

static void
process_request_queue( channel_t *ch )
{
	/* if somebody is already processing requests, increasing ring_lock
	 * will cause an extra iteration (thus other request _will_ be handled).
	 */
	if( IncrementAtomic( &ch->ring_lock ) )
		return;
	do {
		request_t *r;
		while( (r=(request_t*)dequeue(&ch->req_queue)) )
			if( process_request( ch, r ) )
				break;
	} while( DecrementAtomic(&ch->ring_lock) > 1 );
}


static void
handle_disk_insert( int the_channel, int unit )
{
	channel_t *ch = (channel_t*)the_channel;
	
	register_units( ch, unit );
}

static void
handle_events( channel_t *ch ) 
{
	ablk_disk_info_t info;
	int event, i;
	
	for( i=0; !OSI_ABlkDiskInfo(ch->channel, i, &info) ; i++ ) {
		if( (event=OSI_ABlkCntrl1( ch->channel, kABlkGetEvents, i )) <= 0 )
			continue;
		if( event & ABLK_EVENT_DISK_INSERTED )
			DNeedTime( handle_disk_insert, (int)ch, i );
	}
}

static InterruptMemberNumber
hard_irq( InterruptSetMember ISTmember, void *ref_con, UInt32 the_int_count )
{
	channel_t *ch = channels;	/* fixme */
	int running, compl_cnt, event;
	request_t *r;
	
	/* Note: Q & A DV 34 explicitly forbids the usage of secondary
	 * interrupts on the page-fault path (it leads to deadlocks).
	 */
	/* Note: The OSI call _always_ modifies the return arguments */
	if( !OSI_ABlkIRQAck( ch->channel, &compl_cnt, &running, &event ) )
		return kIsrIsNotComplete;

	ch->running = running;

	if( event )
		handle_events( ch );

	/* handle overflow buffer */
	if( ch->obuf_cnt && compl_cnt - ch->obuf_completion >= 0 ) {
		if( ch->obuf_dest ) {
			char *dest = ch->obuf_dest, *src = ch->obuf;
			int cnt = ch->obuf_cnt;
			while( cnt-- )
				*dest++ = *src++;
		}
		/* XXX: insert optimization barrier here */
		*(volatile int*)&ch->obuf_cnt = 0;
	}

	/* process completions */
	while( (r=(request_t*)dequeue(&ch->compl_queue)) ) {
		IOCommandID cmdID;

		if( r->req_num - compl_cnt > 0 ) {
			enqueue_tail( &ch->compl_queue, (queue_el_t*)r );
			break;
		}
		/* free resources... */
		if( r->mem_prepared )
			CheckpointIO( r->ioprep.preparationID, 0 );

		if( r->ablk_req & (ABLK_READ_REQ | ABLK_WRITE_REQ) )
			((IOParam*)r->pb)->ioActCount = r->xfer_cnt;
		
		cmdID = r->cmdID;
		fifo_put( &ch->free_fifo, (fifo_el_t*)r );

		/* ...and complete */
		IOCommandIsComplete( cmdID, noErr );
	}
	process_request_queue( ch );

	return kIsrIsComplete;
}


/************************************************************************/
/*	request preparation						*/
/************************************************************************/

static OSStatus
PrepareMemory( request_t *r, char *buf )
{
	IOPreparationTable *p = &r->ioprep;

	/* XXX: fix kIOIsInput / kIOIsOutput */

	p->options = ( (0 * kIOIsInput)
		       | (0 * kIOIsOutput)
		       | (0 * kIOMultipleRanges)	/* no scatter-gather list */
		       | (1 * kIOLogicalRanges)		/* Logical addresses */
		       | (0 * kIOMinimalLogicalMapping)	/* Normal logical mapping */
		       | (1 * kIOShareMappingTables)	/* Share with Kernel */
		       | (1 * kIOCoherentDataPath) );	/* Definitely! */

	p->granularity = 0;
	p->addressSpace = kCurrentAddressSpaceID;
	p->firstPrepared = 0;
	p->logicalMapping = NULL;
	p->physicalMapping = (void**)&r->phystab;
	p->mappingEntryCount = NUM_PHYSTAB_EL;
	p->rangeInfo.range.base = buf;
	p->rangeInfo.range.length = r->total_cnt;

	return PrepareMemoryForIO(p);
}

OSStatus
queue_req( ParamBlockRec *pb, unit_t *unit, int ablk_req, int param, char *buf, int cnt, IOCommandID ioCmdID )
{
	channel_t *ch = unit->ch;
	request_t *r;

	while( !(r=(request_t*)fifo_get( &ch->free_fifo )) ) {
		/* as long as interrupts are on, the fifo ought to fill up */
		lprintf("ablk: free_queue exhausted!\n");
	}

	r->unit = unit->unit;
	r->ablk_req = ablk_req;
	r->param = param;
	r->total_cnt = cnt;
	r->xfer_cnt = -1;
	r->pb = pb;
	r->cmdID = ioCmdID;

	if( cnt && !PrepareMemory(r, buf) ) {
		r->mem_prepared = 1;
	} else {
		r->ioprep.lengthPrepared = 0;
		r->mem_prepared = 0;
	}
	enqueue( &ch->req_queue, (queue_el_t*)r );
	process_request_queue( ch );

#ifdef SYNC_HACK
	/* HACK. This workaround removes a QuickTime problem. Without it,
	 * QT behaves very strangely when mp3s (and movies) are played.
	 * The symptom is that the same disk sector is read 10-20 times
	 * (causing considerable CPU load and sound skips).
	 * Note that the hack is turned off _before_ the QT player 
	 * is started. I have no idea why this magically solves the problem...
	 */
	if( !sync_event_sent && ch->req_count >= 5000 ) {
		sync_event_sent = 1;
		DNeedTime( turn_off_sync, 0, 0 );
	}
	if( !no_sync_hack ) {
		while( *(int volatile *)&ch->running )
			;
	}
#endif

	return ioInProgress;
}


static OSStatus
DriverPrime( IOParam *pb, unit_t *unit, int ablk_req, int is_immediate, IOCommandID ioCommandID )
{
	uint pos, low, cnt;
	
	if( unit->no_disk )
		return paramErr;
	
	if( is_immediate ) {
		lprintf("ablk: warning, immediate request (!)\n");
		/* return paramErr; */
	}

	unit->mounted = 1; /* hack */
	cnt = (UInt32)pb->ioReqCount;

	/* handle positioning */
	if( (pb->ioPosMode & kUseWidePositioning) ) {
		low = ((XIOParam*)pb)->ioWPosOffset.lo;
		pos = low >> 9;
		pos |= ((XIOParam*)pb)->ioWPosOffset.hi << (32-9);
	} else {
		/* lprintf("pos: %08lx %08lx : %d\n", pb->ioPosOffset, 
			GLOBAL.dce->dCtlPosition, pb->ioPosMode ); */
		low = GLOBAL.dce->dCtlPosition;
		pos = low >> 9;
		/* IM Device Manager claims we should update dCtlPosition */
		GLOBAL.dce->dCtlPosition = low + cnt;
	}

	/* we are allowed to return an error if access is not sector aligned */
	if( (low | cnt) & 0x1ff )
		return paramErr;

	//lprintf("%7d block %9d, buf %8x len %8d\n", unit->ch->req_count, pos, pb->ioBuffer, cnt );
	return queue_req( (ParamBlockRec*)pb, unit, ablk_req, pos + unit->first_blk,
			  pb->ioBuffer, cnt, ioCommandID );
}

OSStatus
DriverWriteCmd(	IOParam *pb, int is_immediate, IOCommandID ioCommandID )
{
	unit_t *unit = get_unit( pb->ioVRefNum );

	if( unit ) {
		if( (unit->info.flags & ABLK_INFO_READ_ONLY) )
			return vLckdErr;
		return DriverPrime( pb, unit, ABLK_WRITE_REQ, is_immediate, ioCommandID );
	}
	lprintf("DriveWrite - no such drive!\n");
	return nsDrvErr;
}

OSStatus
DriverReadCmd( IOParam *pb, int is_immediate, IOCommandID ioCommandID ) 
{
	unit_t *unit = get_unit( pb->ioVRefNum );

	if( unit )
		return DriverPrime( pb, unit, ABLK_READ_REQ, is_immediate, ioCommandID );

	lprintf("DriveRead - no such drive!\n");
	return nsDrvErr;
}


/************************************************************************/
/*	initialization							*/
/************************************************************************/

static channel_t *
setup_channel( int channel )
{
	channel_t *ch;
	request_t *r;
	ulong mphys;
	int i;

	OSI_ABlkCntrl( channel, kABlkStop );
	
	if( !(ch=PoolAllocateResident( sizeof(*ch), TRUE )) )
		return NULL;

	ch->channel = channel;
	ch->ring = (ablk_req_head_t*) MemAllocatePhysicallyContiguous( 
		RING_NUM_EL * sizeof(ablk_req_head_t), TRUE );
	if( !ch->ring ) {
		PoolDeallocate( ch );
		return NULL;
	}
	mphys = GetPhysicalAddr( (char*)ch->ring );
	if( OSI_ABlkRingSetup( channel, mphys, RING_NUM_EL ) ) {
		lprintf("ablk ring setup: unexpected error\n");
		return NULL;
	}

	r = (request_t*)PoolAllocateResident( sizeof(request_t) * MAX_REQUESTS, TRUE );
	for( i=0; i<MAX_REQUESTS; i++ )
		fifo_put( &ch->free_fifo, (fifo_el_t*)&r[i] );

	if( !(ch->obuf=MemAllocatePhysicallyContiguous( OBUF_SIZE, FALSE )) )
		return NULL;
	ch->obuf_phys = GetPhysicalAddr( (char*)ch->obuf );

	/* set ring index to 0 */
	ch->ri = 0;

	return ch;
}

OSErr 
Initialize( void )
{
	channel_t *ch;

	DNeedTime_Init();

	/* only a single channel is supported */
	if( !(ch=setup_channel(0)) )
		return ioErr;
	channels = ch;

	setup_units( ch, 1 /* boot volume */ );
	setup_units( ch, 0 );

	if( InstallIRQ( &GLOBAL.deviceEntry, hard_irq, &ch->irq_info ) ) {
		lprintf("Ablk: Can't install irq!\n");
		return ioErr;
	}
	/* oldworld interrupt routing */
	OSI_ABlkCntrl1( ch->channel, kABlkRouteIRQ, ch->irq_info.aapl_int );
	OSI_ABlkCntrl( ch->channel, kABlkStart );

	lprintf("Asynchronous Block Driver v1.4\n");
	return noErr;
}
