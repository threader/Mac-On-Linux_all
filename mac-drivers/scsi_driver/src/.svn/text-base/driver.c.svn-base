/* 
 *   Creation Date: <1999/03/20 07:30:18 samuel>
 *   Time-stamp: <2003/07/31 13:29:10 samuel>
 *   
 *	<driver.c>
 *	
 *	ScsiSIM driver
 *   
 *   Copyright (C) 1999, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *
 *   NOTE: The following symbols should be exported
 *
 *	CFMInitialize, CFMTerminate, LoadSIM
 */
 
#include "headers.h"
#include "scsi_sh.h"
#include "mol.h"
#include "misc.h"
#include "pci_mol.h"
#include "logger.h"
#include "LinuxOSI.h"

extern OSErr		LoadSIM( RegEntryIDPtr entry );

DriverGlobal		gGlobal;

DriverDescription TheDriverDescription = {
	/* signature info */
	kTheDescriptionSignature,		/* OSType driverDescSignature */
	kInitialDriverDescriptor,		/* DriverDescVersion driverDescVersion */

	/* driver type */
	kPCIDeviceNamePString,			/* Name of hardware */
	kPCIRevisionID, kVersionMinor,		/* NumVersion version */
	kVersionStageValue, kVersionRevision,

	/* driver runtime info */
	0					/* RuntimeOptions driverRuntime */
	| (0 * kDriverIsLoadedUponDiscovery)	/* Loader runtime options */
	| (0 * kDriverIsOpenedUponLoad)		/* Opened when loaded */
	| (1 * kDriverIsUnderExpertControl)	/* I/O expert handles loads/opens */
	| (0 * kDriverIsConcurrent)		/* Not concurrent yet */
	| (0 * kDriverQueuesIOPB),		/* Not internally queued yet */
	kDriverNamePString,			/* Str31 driverName (OpenDriver param) */
	0, 0, 0, 0, 0, 0, 0, 0,			/* UInt32 driverDescReserved[8] */

	1,					/* ServiceCount nServices */

	/* DriverServiceInfo service[0] */
	kServiceCategoryScsiSIM,		/* OSType serviceCategory */
	0,					/* OSType serviceType */
	kVersionMajor, kVersionMinor,		/* NumVersion serviceVersion */
	kVersionStageValue, kVersionRevision,	/* NumVersion serviceVersion */
};


#if 0
static void 
NewOldCall( void *scsiPB, Ptr SIMGlobals )
{
	lprintf("NewOldCall\n");
	EnteringSIM();
	
	ExitingSIM();
}
#endif


/************************************************************************/
/*	Initialization							*/
/************************************************************************/

/* 
 * This function is called whenever we get a RegisterWithNewXPT. This is
 * usually done during boot.
 */
void 
ReregisterBus( void )
{
	struct SIMInitInfo info;

	CLEAR( info );
	info.staticSize = 0;
	info.SIMstaticPtr = GLOBAL.SIMstaticPtr;  /* just in case */

	/* XXX: we never release the old universal proc pointers (does the XPT?) */
	get_sim_procs( &info );

	info.ioPBSize = MIN_PB_SIZE;
	info.busID = GLOBAL.busID;

	/* is the following necessary? best to be on the safe side */
	info.simSlotNumber = GLOBAL.simSlotNumber;
	info.simSRsrcID = GLOBAL.simSRsrcID;
	info.simRegEntry = (char*)GLOBAL.regEntryIDPtr;

	if( SCSIReregisterBus(&info) != noErr )
		lprintf("SCSIReregisterBus failed\n");

	GLOBAL.enteringSIMProc = info.EnteringSIM;
	GLOBAL.exitingSIMProc = info.ExitingSIM;
	GLOBAL.makeCallbackProc = info.MakeCallback;

	/* lprintf("procs: %p %p %p\n", info.EnteringSIM, info.ExitingSIM, info.MakeCallback );*/
	
	/* should the following be done? */
	GLOBAL.simSlotNumber = info.simSlotNumber;
	GLOBAL.simSRsrcID = info.simSRsrcID;
}

/* LoadSIM - main entrypoint */
OSErr
LoadSIM( RegEntryIDPtr entry )
{
	struct SIMInitInfo info;
	OSErr status;
	UInt32 size;

	/* lprintf("LoadSIM\n"); */

	CLEAR(info);
	CLEAR(gGlobal);

	driver_init();
	get_sim_procs( &info );

	/* we don't use a static allocation by the sim */
	info.staticSize = 0;
	info.ioPBSize = MIN_PB_SIZE;
	info.simRegEntry = (char*)entry;	/* OR? */
	
	/* initialize hardware interrupts */
	size = sizeof( InterruptSetMember );
	status = RegistryPropertyGet( entry, kISTPropertyName, 
				      (RegPropertyValue*)&GLOBAL.intSetMember, &size );
	if( status != noErr ) {
		lprintf("RegistryPropertyGet Error\n");
		return status;
	}
	status = GetInterruptFunctions(
		GLOBAL.intSetMember.setID,
                GLOBAL.intSetMember.member,
		&GLOBAL.intRefCon,		/* RefCon */
                &GLOBAL.intOldHandlerFunction,	/* Old handler */
                &GLOBAL.intEnablerFunction,	/* Enabler function */
		&GLOBAL.intDisablerFunction	/* Disabler function */
	);
	if( status != noErr ) {
		lprintf("Could no get old interrupt functions!\n");
		return status;
	}
	status = InstallInterruptFunctions(
		GLOBAL.intSetMember.setID,
		GLOBAL.intSetMember.member,
		NULL,				/* RefCon - ignored */
		PCIInterruptHandler,		/* Our interrupt service func. */
		NULL,				/* No new interrupt enabler */
		NULL			       	/* No new interrupt disabler */
	);
	if( status != noErr ) {
		lprintf("Could not install interrupt handler!\n");
		return status;
	}
	
	/* Register SCSI-bus */
	if( (status=SCSIRegisterBus(&info)) ) {
		lprintf("Could not register SCSI bus\n");
		return status;
	}
	GLOBAL.regEntryIDPtr = entry;
	GLOBAL.busID = info.busID;
	
	/* procs */
	GLOBAL.enteringSIMProc = info.EnteringSIM;
	GLOBAL.exitingSIMProc = info.ExitingSIM;
	GLOBAL.makeCallbackProc = info.MakeCallback;
	GLOBAL.simSlotNumber = info.simSlotNumber;	
	GLOBAL.simSRsrcID = info.simSRsrcID;
	/*
	 * We get a pointer even though we request 0 bytes...
	 * we save it though we will probably have no use for it.
	 */
	GLOBAL.SIMstaticPtr = info.SIMstaticPtr;
	info.simRegEntry = (char*)entry;	/* OR? */

	/* lprintf("SIM Load successful\n"); */
	return status;
}
