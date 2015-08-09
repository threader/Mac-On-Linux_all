/* 
 *   Creation Date: <1999/03/20 07:30:18 samuel>
 *   Time-stamp: <2002/12/31 17:11:51 samuel>
 *   
 *	<driver.c>
 *	
 *	Native MOL Block Driver
 *   
 *   Copyright (C) 1999, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 *
 *   NOTE: This driver has three world visible symbols:
 *
 *	CFMInitialize, CFMTerminate, DoDriverIO
 *
 */
 
#include "headers.h"
#include <DriverGestalt.h>

#include "logger.h"
#include "util.h"
#include "global.h"
#include "LinuxOSI.h"


DriverGlobal		gDriverGlobal;

extern OSErr 		CFMInitialize( void );
extern OSErr 		CFMTerminate( void );
extern OSStatus 	DoDriverIO( AddressSpaceID, IOCommandID, IOCommandContents, 
				    IOCommandCode, IOCommandKind );

/************************************************************************/
/*	DriverDescription						*/
/************************************************************************/

DriverDescription TheDriverDescription = {
	kTheDescriptionSignature,		/* OSType driverDescSignature */
	kInitialDriverDescriptor,		/* DriverDescVersion driverDescVersion */
	/*
	 * DriverType 
	 */
	"\pPCIName",				/* Name of hardware */
#if 0
	kPCIRevisionID, kVersionMinor,		/* NumVersion version */
	kVersionStageValue, kVersionRevision,
#else
	0,0,0,0,
#endif
	/*
	 * DriverOSRuntime driverOSRuntimeInfo
	 */
	0					/* RuntimeOptions driverRuntime */
	| (1 * kDriverIsLoadedUponDiscovery)	/* Loader runtime options */
	| (1 * kDriverIsOpenedUponLoad)		/* Opened when loaded */
	| (0 * kDriverIsUnderExpertControl)	/* I/O expert handles loads/opens */
	/* the use of dCtlPosition ought to make the driver non-concurrent (?) */
	| (0 * kDriverIsConcurrent)		/* Not concurrent yet */
	| (1 * kDriverQueuesIOPB),		/* Internally queued */
#ifdef CDROM_TARGET
	"\p.AppleCD",				/* Str31 driverName (OpenDriver param) */
#else
	"\p.MolBlock",				/* Str31 driverName (OpenDriver param) */
#endif
	0, 0, 0, 0, 0, 0, 0, 0,			/* UInt32 driverDescReserved[8] */
	/*
	 * DriverOSService Information. This section contains a vector count followed by
	 * a vector of structures, each defining a driver service.
	 */
	1,					/* ServiceCount nServices */
	/*
	 * DriverServiceInfo service[0]
	 */
	kServiceCategoryNdrvDriver,		/* OSType serviceCategory */
	kNdrvTypeIsBlockStorage,		/* OSType serviceType */
	1, 0,					/* NumVersion serviceVersion */
	0, 0,					/* NumVersion serviceVersion */
};

OSErr
CFMInitialize(void)
{
	CLEAR( gDriverGlobal );
	Trace("CFMInitialize - MolBlk\n");
	return noErr;
}

OSErr
CFMTerminate(void)
{
	return noErr;
}


/*
 * DriverInitializeCmd
 *
 * The New Driver Manager calls this when the driver is first opened.
 */
static OSStatus
DriverInitializeCmd( DriverInitInfoPtr driverInitInfoPtr )
{
	OSErr status;

	GLOBAL.refNum			= driverInitInfoPtr->refNum;
	GLOBAL.openCount		= 0;
	
	RegistryEntryIDInit(&GLOBAL.deviceEntry);
	status = RegistryEntryIDCopy(&driverInitInfoPtr->deviceEntry, &GLOBAL.deviceEntry);

	/* Put units in table etc. */
	Initialize();
	
	return status;
}

/*
 * DriverReplaceCmd
 *
 * We are replacing an existing driver -- or are completing an initialization sequence.
 * Retrieve any state information from the Name Registry (we have none), install
 * our interrupt handlers, and activate the device.
 *
 * We don't use the calledFromInitialize parameter, but it's here so that a driver can
 * distinguish between initialization (fetch only the NVRAM parameter) and replacement
 * (fetch state information that may be left-over from the previous incantation).
 */
static OSStatus
DriverReplaceCmd( DriverReplaceInfoPtr driverReplaceInfoPtr )
{
	OSStatus		status;

	Trace(DriverReplaceCmd);
	
	GLOBAL.refNum		= driverReplaceInfoPtr->refNum;
	GLOBAL.deviceEntry	= driverReplaceInfoPtr->deviceEntry;

	status = DriverInitializeCmd( driverReplaceInfoPtr );

	return status;
}

/*
 * DriverSupersededCmd
 *
 * We are shutting down, or being replaced by a later driver. Wait for all I/O to
 * complete and store volatile state in the Name Registry whree it will be retrieved
 * by our replacement.
 */
static OSStatus
DriverSupersededCmd( DriverSupersededInfoPtr driverSupersededInfoPtr,
		     Boolean calledFromFinalize )
{
	Trace(DriverSupersededCmd);

	/*
	 * This duplicates DriverKillIOCmd, the correct algorithm would wait for
	 * concurrent I/O to complete. Hmm, what about "infinite wait" I/O, such
	 * as would be posted by a modem server or socket listener? Note that
	 * this section needs to be extended to handle all pending requests.
	 *
	 * It's safe to call CompleteThisRequest, as that routine uses an atomic
	 * operation that allows it to be called when no request is pending without
	 * any possible problems. Since it's a secondary interrupt handler, we
	 * need to call it through the Driver Services Library.
	 *
	 * Warning: GLOBAL.perRequestDataPtr will be NULL if initialization fails
	 * and the Driver Manager tries to terminate us. When we permit concurrent
	 * requests, this will loop on all per-request records.
	 */
	 
	RegistryEntryIDDispose(&GLOBAL.deviceEntry);
	
	return noErr;
}

/*
 * DriverFinalizeCmd
 *
 * Process a DoDriverIO finalize command.
 */
static OSStatus
DriverFinalizeCmd( DriverFinalInfoPtr driverFinalInfoPtr )
{
	Trace(DriverFinializeCmd);
	(void) DriverSupersededCmd((DriverSupersededInfoPtr) driverFinalInfoPtr, TRUE);

	return noErr;
}

/*
 * DriverKillIOCmd stops all I/O for this chip. It's a big hammer, use it wisely.
 * This will need revision when we support concurrent I/O as we must stop all
 * pending requests.
 */
static OSStatus
DriverKillIOCmd( ParmBlkPtr pb )
{
	Trace( DriverKillIOCmd );
	return noErr;
}

/*
 * DriverCloseCmd does nothing..
 */
static OSStatus
DriverCloseCmd(	ParmBlkPtr pb)
{
	if( !GLOBAL.openCount )
		return notOpenErr;
	
	GLOBAL.openCount--;

	if( !GLOBAL.openCount ) {
		/* Do something here...? */
	}
	return noErr;
}

/*
 * DriverOpenCmd does nothing: remember that many applications will open a device, but
 * never close it..
 */
static OSStatus
DriverOpenCmd( ParmBlkPtr pb )
{
	OSStatus status = noErr;
	
	GLOBAL.openCount++;
	if( GLOBAL.openCount == 1 ) {
		DCtlHandle h = GetDCtlEntry(GLOBAL.refNum);
		if( !h || !*h ) {
			lprintf("Blk: GetDctlEntry returned NULL!\n");
			status = paramErr;	/* something else? */
		} else
			GLOBAL.dce = *h;
	}
	return status;
}

/*
 * DoDriverIO
 *
 * In the new driver environment, DoDriverIO performs all driver
 * functions. It is called with the following parameters:
 *	IOCommandID		A unique reference for this driver request. In
 *				the emulated environment, this will be the ParamBlkPtr
 *				passed in from the Device Manager.
 *
 *	IOCommandContents	A union structure that contains information for the
 *				specific request. For the emulated environment, this
 *				will contain the following:
 *		Initialize	Driver RefNum and the name registry id for this driver.
 *		Finalize	Driver RefNum and the name registry id for this driver. 
 *		Others		The ParamBlkPtr
 *
 *	IOCommandCode		A switch value that specifies the required function.
 *
 *	IOCommandKind		A bit-mask indicating Synchronous, Asynchronous, and Immediate
 *
 * For Synchronous and Immediate commands, DoDriverIO returns the final status to
 * the Device Manager. For Asynchronous commands, DoDriverIO may return kIOBusyStatus.
 * If it returns busy status, the driver promises to call IOCommandIsComplete when
 * the transaction has completed.
 */
OSStatus
DoDriverIO( AddressSpaceID addressSpaceID, IOCommandID ioCommandID, 
	    IOCommandContents ioCommandContents, IOCommandCode ioCommandCode,
	    IOCommandKind ioCommandKind)
{
	OSStatus status = noErr;

	/*
	 * Note: Initialize, Open, KillIO, Close, and Finalize are either synchronous
	 * or immediate. Read, Write, Control, and Status may be immediate,
	 * synchronous, or asynchronous.
	 */
	switch( ioCommandCode ) {
	case kInitializeCommand:			/* Always immediate */
		Trace("kInitializeCommand\n");	
		status = DriverInitializeCmd( ioCommandContents.initialInfo);
		CheckStatus(status, "Initialize failed"); 
		break;
	case kFinalizeCommand:				/* Always immediate */
		Trace("kFinalizeCommand\n");		
		status = DriverFinalizeCmd(ioCommandContents.finalInfo); 
		break;
	case kSupersededCommand:
		Trace("kSupersededCommand\n");				
		status = DriverSupersededCmd(ioCommandContents.supersededInfo, FALSE); 
		break;
	case kReplaceCommand:				/* replace an old driver */
		Trace("kReplaceCommand\n");		
		status = DriverReplaceCmd( ioCommandContents.replaceInfo ); 
		break;
	case kOpenCommand:				/* Always immediate */
		Trace("kOpenCommand\n");				
		status = DriverOpenCmd( ioCommandContents.pb);
		break;
	case kCloseCommand:				/* Always immediate */
		Trace("kCloseCommand\n");		
		status = DriverCloseCmd(ioCommandContents.pb);
		break;
	case kControlCommand:
		Trace("kControlCommand\n");				
		status = DriverControlCmd( (CntrlParam *) ioCommandContents.pb, ioCommandID );
		break;
	case kStatusCommand:
		Trace("kStatusCommand\n");
		status = DriverStatusCmd( (CntrlParam *) ioCommandContents.pb );
		break;
	case kReadCommand:
		Trace("kReadCommand\n");
		status = DriverReadCmd( (IOParam*) ioCommandContents.pb, (ioCommandKind & kImmediateIOCommandKind), ioCommandID );
		break;
	case kWriteCommand:
		Trace("kWriteCommand\n");		
		status = DriverWriteCmd( (IOParam*) ioCommandContents.pb, (ioCommandKind & kImmediateIOCommandKind), ioCommandID );
		break;
	case kKillIOCommand:				/* Always immediate */
		Trace("kKillIOCommand\n");				
		status = DriverKillIOCmd(ioCommandContents.pb); 
		break;
	default:
		lprintf("DoDriverIO: unknown selector %d\n", ioCommandCode);		
		status = paramErr;
		break;
	}
	/*
	 * Force a valid result for immediate commands -- they must return a valid
	 * status to the Driver Manager: returning kIOBusyStatus would be a bug..
	 * Non-immediate commands return a status from the lower-level routine. If the
	 * status is kIOBusyStatus, we just return -- an asynchronous I/O completion
	 * routine will eventually complete the request. If it's some other status, the
	 * lower-level routine has completed a non-immediate task, so we call
	 * IOCommandIsComplete and return its (presumably noErr) status.
	 */
	if( (ioCommandKind & kImmediateIOCommandKind) ) {
		/* Immediate commands return the operation status */
		if( status == ioInProgress ) {
			lprintf("ablk: Immediate request handled asyncronous!\n");
		}
	} else if( status == ioInProgress ) {
		/*
		 * An asynchronous operation is in progress. The driver handler promises
		 * to call IOCommandIsComplete when the operation concludes.
		 */
		status = noErr;
	} else {
		/*
		 * Normal command that completed synchronously. Dequeue the user's
		 * parameter block.
		 */
		status = (OSStatus)IOCommandIsComplete(ioCommandID, (OSErr)status);
	}
	return status;
}
