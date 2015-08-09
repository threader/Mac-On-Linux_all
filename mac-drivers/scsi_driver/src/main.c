/* 
 *   Creation Date: <2003/07/16 20:04:28 samuel>
 *   Time-stamp: <2003/07/31 18:53:21 samuel>
 *   
 *	<main.c>
 *	
 *	SCSI SIM driver
 *   
 *   Copyright (C) 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License version 2
 *   
 */
 
#include "headers.h"
#include "scsi_sh.h"
#include "mol.h"
#include "misc.h"
#include "pci_mol.h"
#include "logger.h"
#include "LinuxOSI.h"

#ifndef MIN
#define MIN( x, y )		(((x)<(y)) ? (x) : (y))
#endif

#define MAX_NUM_CMDS		8	/* at most 8 in-flight commands */
#define	SG_ENABLED		1

#define MAX_UNIT_ID		6	/* we reserve 7 for adapter */
#define HBA_ID			7      	/* host bus adaptor ID */

#define MAX_NUM_SG		1024	/* 4K * 8 = 32K */

#define SUPPORTED_FLAGS  						\
	( scsiDirectionMask | scsiCDBIsPointer | 			\
	scsiInitiateSyncData | scsiDisableSyncData | scsiSIMQHead |	\
	scsiSIMQFreeze | scsiSIMQNoFreeze | scsiDoDisconnect |		\
	scsiDontDisconnect | scsiDataReadyForDMA | scsiDataPhysical |	\
	scsiDisableAutosense )

#define UNSUPPORTED_FLAGS 						\
	( scsiCDBLinked | scsiQEnable | scsiSensePhysical | scsiDataPhysical )

#define SUPPORTED_IO_FLAGS 						\
	( scsiNoParityCheck | scsiNoBucketIn | scsiNoBucketOut |	\
	scsiRenegotiateSense | scsiDisableWide | scsiInitiateWide )

typedef struct priv_req {
	SCSIExecIOPB		*pb;		/* the associated pb */
	struct priv_req		*next;
	char			sense_buf[128];

	IOPreparationTable	ioprep;
	AddressRange 		simpleRange;	/* private to PrepareMemory */

	int			phystab[ MAX_NUM_SG ];

	int			r_phys;		/* phsy address of r */
	scsi_req_t		r;		/* must go last */
} priv_req_t;

static struct {
	priv_req_t		*free_req;
} hc;


#define PRIV_REQ_SIZE		(sizeof(priv_req_t) + MAX_NUM_SG * sizeof(sg_t))



/************************************************************************/
/*	Work								*/
/************************************************************************/

static void
do_complete( priv_req_t *p, SCSI_PB *pb, OSErr err )
{
	pb->scsiResult = err;

	/* release req */
	if( p ) {
		p->next = hc.free_req;
		hc.free_req = p;
	}

	/* XXX: should we do the check? */
	if( pb->scsiCompletion ) {
		ExitingSIM();
		MakeCallback( pb );
		EnteringSIM();
	}
}

static OSStatus
PrepareMemory( priv_req_t *p, SCSIExecIOPB *pb )
{
	IOPreparationTable *pt = &p->ioprep;
	scsi_req_t *r = &p->r;
	int i, n, nranges, len, cnt;
	AddressRange *ltab;

	pt->options = ( (0 * kIOIsInput)
		       | (0 * kIOIsOutput)
		       | (0 * kIOMultipleRanges)	/* no scatter-gather list */
		       | (1 * kIOLogicalRanges)		/* Logical addresses */
		       | (0 * kIOMinimalLogicalMapping)	/* Normal logical mapping */
		       | (1 * kIOShareMappingTables)	/* Share with Kernel */
		       | (1 * kIOCoherentDataPath) );	/* Definitely! */

	pt->options |= ( r->is_write )? kIOIsOutput : kIOIsInput;

	pt->granularity = 0;
	pt->addressSpace = kCurrentAddressSpaceID;
	pt->firstPrepared = 0;
	pt->logicalMapping = NULL;
	pt->physicalMapping = (void**)&p->phystab;
	pt->mappingEntryCount = MAX_NUM_SG;

	pt->options |= kIOMultipleRanges;
	if( pb->scsiDataType == scsiDataSG ) {
		pt->rangeInfo.multipleRanges.entryCount = pb->scsiSGListCount;
		pt->rangeInfo.multipleRanges.rangeTable = (void*)pb->scsiDataPtr;
#if 0
		lprintf("Num SG: %d\n", pb->scsiSGListCount );
		for( i=0; i< pb->scsiSGListCount ; i++ ) {
			lprintf("base: %08x len %08x\n",
				pt->rangeInfo.multipleRanges.rangeTable[i].base,
				pt->rangeInfo.multipleRanges.rangeTable[i].length );
		}
#endif
	} else if( pb->scsiDataType == scsiDataBuffer ) {
		/* simple range */
		p->simpleRange.base = pb->scsiDataPtr;
		p->simpleRange.length = pb->scsiDataLength;

		pt->rangeInfo.multipleRanges.rangeTable = &p->simpleRange;
		pt->rangeInfo.multipleRanges.entryCount = 1;
	} else {
		lprintf("bad buffer type!\n");
	}

	ltab = pt->rangeInfo.multipleRanges.rangeTable;
	nranges = pt->rangeInfo.multipleRanges.entryCount;

	if( PrepareMemoryForIO(pt) )
		return -1;

	if( r->size != p->ioprep.lengthPrepared )
		lprintf("short preparation\n");

	i=0;
	for( n=0; n<nranges; n++ ) {
		int seglen = ltab[n].length;

		if( (len=(p->phystab[i] & 0xfff)) )
			len = 0x1000 - len;
		else
			len = 0x1000;
		
		for( cnt=0 ; cnt < seglen; i++ ) {
			if( len + cnt > seglen )
				len = seglen - cnt;
			cnt += len;
		
			r->sglist.vec[i].base = p->phystab[i];
			r->sglist.vec[i].size = len;
			len = 0x1000;
		}
	}
	r->n_sg = r->sglist.n_el = i;
#if 0
	for( i=0; i < r->n_sg; i++ )
		lprintf("PREP [%d]: %08x %x\n", i, r->sglist.vec[i].base, r->sglist.vec[i].size );
#endif
	return 0;
}

static void
SCSI_execIO( SCSIExecIOPB *pb )
{
	unsigned char *cmdptr;
	priv_req_t *p;
	scsi_req_t *r;
	int i;
	
	/* this is important! */
	pb->scsiResult = scsiRequestInProgress;

	/* dequeue */
	p = hc.free_req;
	hc.free_req = p->next;

	if( !p ) {
		lprintf("out of SCSI request blocks\n");
		do_complete( NULL, (SCSI_PB*)pb, scsiBusy );
		return;
	}
	r=&p->r;

	/* lprintf("Issue %x\n", (int)p ); */
	p->pb = pb;

	r->cdb_len = pb->scsiCDBLength;
	cmdptr = (pb->scsiFlags & scsiCDBIsPointer) ? pb->scsiCDB.cdbPtr : pb->scsiCDB.cdbBytes;
	BlockCopy( cmdptr, r->cdb, pb->scsiCDBLength );

	r->target = pb->scsiDevice.targetID;
	r->lun = pb->scsiDevice.LUN;
	r->is_write = ((pb->scsiFlags & scsiDirectionMask) == scsiDirectionOut);

	/* data */
	r->size = pb->scsiDataLength;
	r->n_sg = 0;
	r->sglist.n_el = 0;
	r->sglist.next_sglist = 0;	/* phys */

	if( pb->scsiCommandLink ) {
		lprintf("XXX scsiCommandLink: %x XXX\n", pb->scsiCommandLink );
		do_complete( p, (SCSI_PB*)pb, scsiFunctionNotAvailable );
		return;
	}

	p->ioprep.lengthPrepared = 0;
	if( r->size ) {
		if( pb->scsiDataType != scsiDataSG && pb->scsiDataType != scsiDataBuffer ) {
			lprintf("XXX SCSI: unsupported data type %d XXXX\n", pb->scsiDataType);
			do_complete( p, (SCSI_PB*)pb, scsiDataTypeInvalid );
			return;
		}
		if( pb->scsiFlags & scsiDataPhysical ) {
			lprintf("XXX SCSI: physical datapointers %x XXX\n");
			do_complete( p, (SCSI_PB*)pb, scsiFunctionNotAvailable );
			return;
		}
		//lprintf("r->size = %d\n", r->size );
		if( PrepareMemory(p, pb) ) {
			lprintf("SCSI: PrepareMemory failed\n");
			do_complete( p, (SCSI_PB*)pb, scsiFunctionNotAvailable /*XXX*/ );
			return;
		}
	}
	OSI_SCSISubmit( p->r_phys );
}

static long
SoftInterrupt( void *ack, void *p2 )
{
	priv_req_t *next, *p = (priv_req_t*)((int)ack & SCSI_ACK_REQADDR_MASK);
	int i;
	
	//lprintf("- SCSI Interrupt -\n");
	
	EnteringSIM();
	for( ; p ; p=next ) {
		scsi_req_t *r = &p->r;
		SCSIExecIOPB *pb = p->pb;
		OSErr err = noErr;

		/* lprintf("Complete: %x  [%d]\n", (int)p, r->scsi_status ); */

		next = (priv_req_t*)r->next_completed;

		/* sense / status */
		pb->scsiSCSIstatus = r->scsi_status;
		pb->scsiSenseResidual = pb->scsiSenseLength;
		pb->scsiResultFlags = 0;

		if( r->ret_sense_len && !(pb->scsiFlags & scsiDisableAutosense) ) {
			int slen = MIN(r->ret_sense_len, pb->scsiSenseLength);
			pb->scsiSCSIstatus = scsiStatCheckCondition;

			if( pb->scsiFlags & scsiSensePhysical )
				lprintf("XXX SCSI: physical sense XXX\n");
			
			if( pb->scsiSensePtr )
				BlockCopy( p->sense_buf, pb->scsiSensePtr, slen );

			pb->scsiSenseResidual = pb->scsiSenseLength - slen;
			pb->scsiResultFlags |= scsiAutosenseValid;
		} else {
			pb->scsiSenseResidual = pb->scsiSenseLength;
		}
		if( pb->scsiSCSIstatus )
			err = scsiNonZeroStatus;
		
		/* check adapter status (no unit) */
		if( r->adapter_status ) {
			switch( r->adapter_status ) {
			default:
				lprintf("bad adapter status\n");
				/* fallthrough */
			case kAdapterStatusNoTarget:
				err = scsiSelectTimeout;
				break;
			}
		}
		if( !err ) {
			pb->scsiDataResidual = 0; /* FIXE */
		} else {
			pb->scsiDataResidual = pb->scsiDataLength;
		}

		if( p->ioprep.lengthPrepared )
			CheckpointIO( p->ioprep.preparationID, 0 );

		/* should this be done even if scsiCompletion == NULL? */
		do_complete( p, (SCSI_PB*)pb, err );
	}
	ExitingSIM();

	return 0;
}

InterruptMemberNumber
PCIInterruptHandler( InterruptSetMember intMember, void *refCon, UInt32 theIntCount )
{
	int ack;
	//lprintf("SCSI interrupt\n");

	if( !(ack=OSI_SCSIAck()) )
		return kIsrIsNotComplete;

	QueueSecondaryInterruptHandler( SoftInterrupt, NULL, (void*)ack, NULL );
	return kIsrIsComplete;
}

static void
SIMInterruptPoll( Ptr SIMGlobals )
{
	int ack;
	//lprintf("SIMInterruptPoll\n");

	if( (ack=OSI_SCSIAck()) )
		CallSecondaryInterruptHandler2( SoftInterrupt, NULL, (void*)ack, NULL );
}


/************************************************************************/
/*	misc functions							*/
/************************************************************************/

static void
SCSI_inquiry( SCSIBusInquiryPB *pb )
{
//	lprintf("SCSI: inquiry\n");
	
	pb->scsiEngineCount = 0;
	pb->scsiMaxTransferType = 1;	/* or? */
	pb->scsiDataTypes = ( (1 * scsiBusDataBuffer) 	|
			      (0 * scsiBusDataTIB)	|
			      (SG_ENABLED * scsiBusDataSG) );
	
	pb->scsiIOpbSize = MIN_PB_SIZE;
	pb->scsiFeatureFlags = scsiBusInternal | scsiBusCacheCoherentDMA |
				(0 * scsiBusOldCallCapable);
	pb->scsiVersionNumber = 1;
	pb->scsiHBAInquiry = scsiBusSoftReset;
	pb->scsiInitiatorID = HBA_ID;
	pb->scsiFlagsSupported = SUPPORTED_FLAGS;
	pb->scsiIOFlagsSupported = SUPPORTED_IO_FLAGS;
	pb->scsiWeirdStuff = 0;
	pb->scsiMaxTarget = 6;
	pb->scsiMaxLUN = 0;
	CStrCopy( pb->scsiSIMVendor, "Apple" );			/* 16 */
	CStrCopy( pb->scsiHBAVendor, "Mac-on-Linux" );		/* 16 */
	CStrCopy( pb->scsiControllerFamily, "mol-family" );	/* 16 */
	CStrCopy( pb->scsiControllerType, "mol-scsi" );		/* 16 */
	CStrNCopy( pb->scsiControllerFamily, "3.14",4 );	/* 4 */
	CStrNCopy( pb->scsiSIMversion, "9.10", 4 );	       	/* 4 */
	CStrNCopy( pb->scsiHBAversion, "2.71", 4 );		/* 4 */
	pb->scsiHBAslotType = scsiPCIBus;
	pb->scsiAdditionalLength = 0;
	pb->scsiHBAslotNumber = GLOBAL.simSlotNumber;
	pb->scsiSIMsRsrcID = GLOBAL.simSRsrcID;

	do_complete( NULL, (SCSI_PB *)pb, noErr );
}

static void
SCSI_loadDriver( SCSILoadDriverPB *pb )
{
	/* lprintf("SCSI: load driver (failed: %d)\n", pb->scsiDiskLoadFailed ); */

	pb->scsiLoadedRefNum = 0;
	do_complete( NULL, (SCSI_PB *)pb, scsiFunctionNotAvailable );
}


/************************************************************************/
/*	SIMAction							*/
/************************************************************************/

static void
SIMAction( void *scsiPB, Ptr SIMGlobals )
{
	SCSI_PB *pb = (SCSI_PB*)scsiPB;

	/* must do this before EnteringSIM (since the ExitingSIM proc will change...) */
	if( pb->scsiFunctionCode == SCSIRegisterWithNewXPT ) {
		lprintf("ReregisterWithNewXPT\n");
		ReregisterBus();
		return;
	}

	/* the following code is not reentrant */
	EnteringSIM();

	switch( pb->scsiFunctionCode ) {
	case SCSILoadDriver:
		SCSI_loadDriver( (SCSILoadDriverPB*)pb );
		break;

	case SCSIReleaseQ:
		lprintf("SCSI: release queue\n");
		do_complete( NULL, (SCSI_PB *)pb, noErr );
		break;

	case SCSIBusInquiry:
		SCSI_inquiry( (SCSIBusInquiryPB*)pb );
		break;		

	case SCSIExecIO:
		SCSI_execIO( (SCSIExecIOPB*)pb );
		break;

	default:
		lprintf("Unknown SCSI function code %d\n", pb->scsiFunctionCode );
		pb->scsiResult = scsiFunctionNotAvailable;
		break;
	}
	ExitingSIM();
}




/************************************************************************/
/*	initialization							*/
/************************************************************************/

static OSErr 
SIMInit( Ptr siminfo )
{
	/* enable interrupts */
	(*GLOBAL.intEnablerFunction)( GLOBAL.intSetMember, GLOBAL.intRefCon );
	return noErr;
}

void
get_sim_procs( struct SIMInitInfo *info )
{
	info->SIMInit = NewSIMInitProc( SIMInit );
	info->SIMAction = NewSIMActionProc( SIMAction );
	info->SIMInterruptPoll = NewSCSIInterruptPollProc( SIMInterruptPoll );

	/* info->oldCallCapable = 1; */
	/* info->NewOldCall = NewSCSIActionProc( NewOldCall ); */
}

void
driver_init( void )
{
	char *mem = MemAllocatePhysicallyContiguous( PRIV_REQ_SIZE * MAX_NUM_CMDS, TRUE );
	int i;
	
	lprintf("SCSI SIM v1.03\n");
	OSI_SCSIControl( SCSI_CTRL_INIT, 0 );

	hc.free_req = 0;
	for( i=0; i<MAX_NUM_CMDS; i++ ) {
		priv_req_t *p = (priv_req_t*)&mem[PRIV_REQ_SIZE * i];

		/* fill in some constant fields */ 
		p->r.client_addr = (int)p;
		p->r.sense[0].base = GetPhysicalAddr( p->sense_buf );
		p->r.sense[0].size = sizeof( p->sense_buf );
		p->r.sense[1].base = 0;
		p->r.sense[1].size = 0;
		p->r_phys = GetPhysicalAddr( (char*)&p->r );
		
		p->next = hc.free_req;
		hc.free_req = p;
	}
}
