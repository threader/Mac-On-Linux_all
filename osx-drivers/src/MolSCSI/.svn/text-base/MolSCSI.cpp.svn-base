/* 
 *   Creation Date: <2003/07/08 22:46:47 samuel>
 *   Time-stamp: <2003/07/16 22:34:58 samuel>
 *   
 *	<MolSCSI.cpp>
 *	
 *	
 *   
 *   Copyright (C) 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#include "mol_config.h"
#include "MolSCSI.h"
#include "osi_calls.h"


#define super	IOSCSIParallelController

OSDefineMetaClassAndStructors( MolSCSI, IOSCSIParallelController );

#define MAX_NUM_SG		64
#define NUM_OVERFLOW_PAGES	1

typedef struct priv_req {
	IOSCSIParallelCommand	*cmd;
	struct priv_req		*cancel_next;
	scsi_req_t		r;		// must be last
} priv_req_t;

#define PRIV_DATA_SIZE		(sizeof(priv_req_t) + MAX_NUM_SG * sizeof(sg_t))

#define BARRIER()		asm volatile("")

bool
MolSCSI::configure( IOService *theProvider, SCSIControllerInfo *controller )
{
	int i;
	printm("SCSI driver v1.03\n");

	provider = theProvider;

	// -------- chip init -----------------------
	_irq = IOInterruptEventSource::interruptEventSource( 
		this, (IOInterruptEventSource::Action)&MolSCSI::interrupt, provider, 0 );
	if( !_irq ) {
		printm("MolSCSI: no irq\n");
		return false;
	}
	getWorkLoop()->addEventSource( _irq );
	_irq->enable();

	// -------- allocate overflow lists ---------
	_olist = 0;
	for( i=0; i<NUM_OVERFLOW_PAGES; i++ ) {
		sglist_t *sgl = (sglist_t*)IOMallocContiguous( 0x1000, 0x1000, 0 );
		sgl->drv_next = _olist;
		_olist = sgl;
	}

	OSI_SCSIControl( SCSI_CTRL_INIT, 0 );
	memoryDesc = IOMemoryDescriptor::withAddress( (void*)0, 0, kIODirectionNone );

	// ---------- controller information ------------------
	controller->initiatorId = 7;
	controller->maxTargetsSupported = 16;
	controller->maxLunsSupported = 8;
	controller->minTransferPeriodpS = 0;
	controller->maxTransferOffset = 0;
	controller->maxTransferWidth = 2;

	controller->maxCommandsPerController = 0;
	controller->maxCommandsPerTarget = 0;
	controller->maxCommandsPerLun = 0;

	controller->tagAllocationMethod = kTagAllocationPerController;
	controller->maxTags = 128;
	
	controller->targetPrivateDataSize = 0;
	controller->lunPrivateDataSize = 0;
	controller->commandPrivateDataSize = PRIV_DATA_SIZE;
	
	controller->disableCancelCommands = false;

	return true;	
}

static void
simpleComplete( IOSCSIParallelCommand *cmd, IOReturn err )
{
	SCSIResults result;		
	memset( &result, 0, sizeof(result) );

	result.returnCode = err;
	cmd->setResults( &result );
	cmd->complete();
	return;
}

void
MolSCSI::releaseOLists( scsi_req_t *r )
{
	sglist_t *sgl;

	while( (sgl=r->sglist.drv_next) ) {
		r->sglist.drv_next = sgl->drv_next;

		_olists_in_use--;
		sgl->drv_next = _olist;
		_olist = sgl;
	}
}

int
MolSCSI::prepareDataXfer( IOSCSIParallelCommand *cmd, scsi_req_t *r ) 
{
	IOMemoryDescriptor *desc;
	IOByteCount offs, cnt;
	int phys, i, n_el, max_el;
	sglist_t *sglist;
	UInt32 xfer_cnt;
	bool is_write;

	cmd->getPointers( &desc, &xfer_cnt, &is_write );
	r->is_write = is_write;

	// prepare scatter and gather list
	offs = n_el = i = 0;
	max_el = MAX_NUM_SG;
	sglist = &r->sglist;
	while( offs < xfer_cnt ) {
		if( !(phys=desc->getPhysicalSegment(offs, &cnt)) )
			break;
		sglist->vec[i].base = phys;
		sglist->vec[i].size = cnt;
		offs += cnt;
		n_el++;

		// use an overflow list?
		if( ++i == max_el ) {
			if( !_olist )
				break;
			memoryDesc->initWithAddress( (void*)_olist, 1, kIODirectionNone );
			sglist->n_el = i;
			sglist->next_sglist = memoryDesc->getPhysicalAddress();
			sglist->drv_next = _olist;

			// start on the overflow list
			_olists_in_use++;
			sglist = _olist;
			max_el = (0x1000 - sizeof(sglist_t))/sizeof(sg_t);
			i = 0;

			// unlink
			_olist = _olist->drv_next;
		}
	}
	sglist->n_el = i;
	sglist->next_sglist = 0;
	sglist->drv_next = 0;
	r->n_sg = n_el;
	r->size = xfer_cnt;

	if( offs < xfer_cnt ) {
		releaseOLists( r );
		return _olist ? -1 : 1;
	}
	return 0;
}

void
MolSCSI::executeCommand( IOSCSIParallelCommand *cmd )
{
	priv_req_t *p = (priv_req_t*)cmd->getCommandData();
	scsi_req_t *r = &p->r;
	IOMemoryDescriptor *desc;
	SCSITargetLun addr;
	SCSICDBInfo cdb;
	int i, ret;
	
	cmd->getTargetLun( &addr );
	cmd->getCDB( &cdb );

	// misc
	p->cmd = cmd;
	p->cancel_next = 0;

	r->client_addr = (int)p;
	r->target = addr.target;
	r->lun = addr.lun;
	r->cdb_len = cdb.cdbLength;
	for( i=0; i<(int)cdb.cdbLength; i++ )
		r->cdb[i] = cdb.cdb[i];

	// prepare scatter & gather buffer
	while( (ret=prepareDataXfer(cmd, r)) > 0 ) {
		sglist_t *sgl;
		if( _olists_in_use ) {
			if( !_is_disabled )
				disableCommands();
			_is_disabled = 1;
			rescheduleCommand( cmd );
			return;
		}
		if( !(sgl=(sglist_t*)IOMallocContiguous(0x1000, 0x1000, 0)) ) {
			simpleComplete( cmd, kIOReturnNoMemory );
			return;
		}
		sgl->drv_next = _olist;
		_olist = sgl;
	}
	if( ret < 0 ) {
		printm("Bad SCSI command\n");
		simpleComplete( cmd, kIOReturnError );
		return;
	}

	// prepare sense buffer
	cmd->getPointers( &desc, 0, 0, true );
	if( !desc ) {
		r->sense[0].size = 0;
		r->sense[1].size = 0;
	} else {
		// 4K should always be enough...
		IOByteCount cnt;
		r->sense[0].base = desc->getPhysicalSegment( 0, &cnt );
		r->sense[0].size = cnt;
		r->sense[1].base = desc->getPhysicalSegment( cnt, &cnt );
		r->sense[1].size = cnt;
	}

	// OK... issue the command
	memoryDesc->initWithAddress( (void*)r, 1, kIODirectionNone );
	OSI_SCSISubmit( memoryDesc->getPhysicalAddress() );
}

void
MolSCSI::cancelCommand( IOSCSIParallelCommand *cmd )
{
	IOSCSIParallelCommand *orgCmd = cmd->getOriginalCmd();
	priv_req_t *op, *p = (priv_req_t*)cmd->getCommandData();

	printm("MolSCSI: cancelCommand\n");
	memset( p, 0, PRIV_DATA_SIZE );
	p->cmd = cmd;

	/* unfortunately, the linux side driver does not really support
	 * canceling commands. We will just wait for command completion to
	 * complete...
	 */
	op = (priv_req_t*)orgCmd->getCommandData();
	p->cancel_next = op->cancel_next;
	op->cancel_next = p;
}

void
MolSCSI::resetCommand( IOSCSIParallelCommand *cmd )
{
	printm("MolSCSI: resetCommand\n");
	simpleComplete( cmd, kIOReturnSuccess );
}

void
MolSCSI::interrupt( IOInterruptEventSource *sender, int count )
{
	priv_req_t *p, *next;
	int ack;
	
	if( !(ack=OSI_SCSIAck()) )
		return;
	p = (priv_req_t*)(ack & SCSI_ACK_REQADDR_MASK);

	for( ; p ; p=next ) {
		scsi_req_t *r = &p->r;
		priv_req_t *cancelp = p->cancel_next;
		SCSIResults result;

		next = (priv_req_t*)r->next_completed;

		// result status
		memset( &result, 0, sizeof(result) );
		result.returnCode = kIOReturnSuccess;
		result.adapterStatus = kSCSIAdapterStatusSuccess;
		result.scsiStatus = r->scsi_status;
		result.bytesTransferred = r->act_size;
		result.requestSenseDone = r->ret_sense_len ? true : false;
		result.requestSenseLength = r->ret_sense_len;

		if( result.requestSenseLength )
			result.scsiStatus = kSCSIStatusCheckCondition;

		if( r->adapter_status ) {
			switch( r->adapter_status ) {
			default:
				printm("bad adapter status\n");
				// fallthrough
			case kAdapterStatusNoTarget:
				result.adapterStatus = kSCSIAdapterStatusSelectionTimeout;
				break;
			}
		}
		releaseOLists( r );
		p->cmd->setResults( &result, 0 );
		p->cmd->complete();

		while( cancelp ) {
			IOSCSIParallelCommand *cmd = cancelp->cmd;
			cancelp = cancelp->cancel_next;

			simpleComplete( cmd, kIOReturnSuccess );
		}
	}

	if( _is_disabled ) {
		enableCommands();
		_is_disabled = 0;
	}
}

