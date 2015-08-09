/* 
 *   Creation Date: <2002/11/23 14:31:06 samuel>
 *   Time-stamp: <2003/03/09 14:33:28 samuel>
 *   
 *	<ioctl.c>
 *	
 *	
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "global.h"
#include <DriverGestalt.h>
#include <Disks.h>
#include "util.h"
#include "logger.h"
#include "ccodes.h"
#include "unit.h"


static OSStatus
DriverGestaltHandler( DriverGestaltParam *pb )
{
	unit_t *unit = get_unit( pb->ioVRefNum );
	ulong gstr[2];

	pb->driverGestaltResponse = 0;
	
	switch( pb->driverGestaltSelector ) {
	case kdgSync: 
		pb->driverGestaltResponse = 0;		/* We handle asynchronous I/O */
		return noErr;
	case kdgVersion:
		pb->driverGestaltResponse = 0;
		return noErr;
	case kdgInterface:
		pb->driverGestaltResponse = 'pci ';
		return noErr;
	case kdgSupportsSwitching:			/* Support Power up/down switching? */
		pb->driverGestaltResponse = 0;		/* Not supported yet */
		return noErr;
	case kdgSupportsPowerCtl:			/* TRUE if in high-power mode */
		pb->driverGestaltResponse = 0;		/* Power-switching is not supported */
		return noErr;
	case kdgDeviceType: /* this is called with unit==NULL */
		pb->driverGestaltResponse = kdgDiskType;  /* kdgRemovableType */
		if( unit ) {
			int flags = unit->info.flags;
			/* the MacOS installer does not find any volumes if we return
			 * kdgCDType for the boot CD... (strange)
			 */
			if( (flags & ABLK_INFO_CDROM) && !(flags & ABLK_BOOT_HINT) )
				pb->driverGestaltResponse = kdgCDType;
			else if( (flags & ABLK_INFO_REMOVABLE) )
				pb->driverGestaltResponse = kdgRemovableType;
		}
		/* XXX: we really should find a better place for this. */
		mount_drives_hack();
		return noErr;
	case kdgEject: /* eject options for shutdown and restart */
		/* unit might be NULL */
		pb->driverGestaltResponse = kRestartDontEject_Mask | kShutDownDontEject_Mask;
		return noErr;
	}

	if( !unit ) {
		gstr[0] = pb->driverGestaltSelector;
		gstr[1] = 0;
		lprintf("Gestalt selector '%s' skipped (unit==NULL)\n",  (char*)&gstr[0] );	
		return nsDrvErr;
	}

	switch( pb->driverGestaltSelector ) {
	case kdgAPI:	/* refer to PCI C&D */
		/* we do not support FileExchange yet */
		Log("FileExchange probe\n");
		return statusErr;

	case kdgPhysDriveIconSuite:		/* nor color icons */
	case kdgMediaIconSuite:
		Log("get color icon\n");
		return statusErr;
		
	case kdgWide:  /* supports > 4GB volumes */
		*GetDriverGestaltBooleanResponse(pb) = TRUE;
		return noErr;

	case kdgFlush:
		Log( "kdgFlush\n");
		pb->driverGestaltResponse = 0;	/* no flushing necessary */
		return noErr;
		
	case kdgVMOptions:			/* these shold be implemented! */
		Log("kdgVMOptions\n");
		return statusErr;

	case kdgMediaInfo:
		Log("kdgMediaInfo\n");		
		return statusErr;
		
	case kdgBoot:	
		/* Value to place in PRAM for this device */
		pb->driverGestaltResponse = (unit->info.flags & ABLK_BOOT_HINT) ? 1 : unit->unit +2;
		//lprintf("kdgBoot: unit = %d, respons %d\n",  unit->unit, pb->driverGestaltResponse );
		return noErr;

	case kdgNameRegistryEntry:
		pb->driverGestaltResponse = (int)&(GLOBAL.deviceEntry);
		//lprintf("kdgNameRegEntry\n");
		return statusErr;	/* XXX */

	case kdgOpenFirmwareBootSupport:
	case kdgOpenFirmwareBootingSupport:
		//lprintf("kdgOFBoot[ing] gestalt, unit = %d\n",  unit->unit );
		pb->driverGestaltResponse = kOFBootNotPartitioned;
		pb->driverGestaltResponse1 = 0;
		return noErr;

	case kdgDeviceReference:
		/* This number is device specific. */
		//lprintf("kdgDevice Reference gestalt, unit = %d\n",  unit->unit );
		pb->driverGestaltResponse = (unit->channel << 8) | unit->unit;
		return noErr;

	case 'dslp':
		/* undocumented */
		return statusErr;
	}

	gstr[0] = pb->driverGestaltSelector;
	gstr[1] = 0;
	lprintf("ablk: Unkown gestalt '%s'\n",  (char*)&gstr[0] );
	return statusErr;
}

OSStatus
DriverStatusCmd( CntrlParam *pb )
{
	unit_t *unit = get_unit( pb->ioVRefNum );
	
	switch( pb->csCode ) {
	case kDriverGestaltCode:
		Log("status - kDriverGestaltCode\n");
		return DriverGestaltHandler( (DriverGestaltParam*)pb );
			
	case kdgLowPowerMode:	/* Sets/Returns the current energy consumption level */
		Log("status - GetLowPowerMode\n");
		return statusErr;			
	}
	
	if( !unit ) {
		lprintf("DriveStatusCmd IGNORED %d\n", pb->csCode );
		return nsDrvErr;
	}
	
	switch( pb->csCode ) {
	case kGetPartInfo: {
		partInfoRec *pinfo = (partInfoRec*)&pb->csParam[0];
		*(UInt32*)&pinfo->SCSIID = 0;
		pinfo->physPartitionLoc = 0;
		pinfo->partitionNumber = 0;
		return noErr;
	}
	case kGetPartitionStatus:	/* refer to the Monster Driver TN */
		return statusErr;
	
	case kGetMountStatus:
		lprintf("kGetMountStatus\n");
		pb->csParam[0] = 1;	/* always mount at startup */
		return noErr;
		
	case driveStatusSC: {
		DrvSts *driveStats;
		DrvQElPtr driveQEl;

		Log("status - driveStatusSC\n");
			
		/* Drive Stats... */
		driveStats = (DrvSts *)pb->csParam;
		driveStats->track = 0;		/* not applicable */
		driveStats->writeProt = (unit->info.flags & ABLK_INFO_READ_ONLY) ? 0x80 : 0;
		/* perhaps we should specify diskInPlace=1 before being read to? */
		driveStats->diskInPlace = 0x08;	/* non-ejectable */
		driveStats->installed = 1;	/* drive installed */
		driveStats->sides = 0;		/* not applicable */
		driveStats->twoSideFmt = 0;	/* not applicable */
		driveStats->needsFlush = 0;	/* not applicable */
		driveStats->diskErrs = 0;	/* not applicable */
		
		/* copy qLink through dQFSID from our DrvQEl */
		driveQEl = (DrvQElPtr)(GetDrvQHdr()->qHead);
		while( driveQEl ) {
			if( unit && driveQEl->dQDrive == unit->drv_num ) {
				driveStats->qLink = driveQEl->qLink;
				driveStats->qType = driveQEl->qType;
				driveStats->dQDrive = driveQEl->dQDrive;
				driveStats->dQRefNum = driveQEl->dQRefNum;
				driveStats->dQFSID = driveQEl->dQFSID;
				break;
			}
			driveQEl = (DrvQElPtr)(driveQEl->qLink);
		}
		return noErr;
	}
	case returnFormatListSC:
		/* from .SONY */
		return statusErr;
	}
	lprintf("status - unknown selector %d\n", pb->csCode );
	return statusErr;
}

static int
unit_is_unmounted( unit_t *unit )
{
	unit_t *u = get_first_unit( unit->ch );

	for( ; u ; u=u->next ) {
		if( u->unit != unit->unit )
			continue;
		if( u->mounted )
			return 0;
	}
	return 1;
}


OSStatus
DriverControlCmd( CntrlParam *pb, IOCommandID ioCmdID )
{
	unit_t *unit = get_unit( pb->ioVRefNum );

	/* handle codes where unit might be NULL */
	switch( pb->csCode ) {
	case kdgLowPowerMode:
		/* sets/returns the current energy consumption level */
		Log("control - SetLowPowerMode\n");
		return controlErr;
	}

	if( !unit ) {
		/* we get this at shudown... */
		if( pb->csCode == -1 )
			return nsDrvErr;
		lprintf("DriveControlCmd IGNORED %d\n", pb->csCode );
		return nsDrvErr;
	}

	switch( pb->csCode ) {
	case killIOCC:
		lprintf("ablk: control - KillIO");
		return controlErr;
		
	case kVerify:
		Log("control - verifyDiskCC\n" );						
		return noErr;
		
	case kFormat:
		lprintf("control - formatDiskCC\n" );
		return noErr;

	case kEject:
		/* lprintf("control - ejectDiskCC\n"); */
		unit->mounted = 0;
		if( (unit->info.flags & ABLK_INFO_REMOVABLE ) ) {
			/* eject only if all partitions are unmounted */
			if( unit_is_unmounted( unit ) ) {
				/* this also ejects physically related partitions */
				eject_unit_records( unit->ch, unit->unit );
				/* post eject disk command */
				return queue_req( (ParamBlockRec*)pb, unit, ABLK_EJECT_REQ, 0, NULL, 0, ioCmdID );
			}
		} else {
			PostEvent( diskEvt, pb->ioVRefNum );
		}
		return noErr;
			
	case kDriveIcon:
		Log("control - physicalIconCC\n" );
		/* return pointer to icon and where string */
		/* *(Ptr *)pb->csParam = (Ptr)globe->physicalIcon; */
		return controlErr;

	case kMediaIcon:
		Log("control - mediaIconCC\n" );
		/* return pointer to icon and where string */
		/* *(Ptr *)pb->csParam = (Ptr)globe->mediaIcon; */
		return controlErr;			
			
	/* When a HFS volume is mounted, the File Manager calls the disk driver
	 * with a "Return Drive Info" _Control call (csCode=23). Then if there
	 * are no errors, it looks at the low-byte (bits 0-7) of csParam to see
	 * if the drive type is ramDiskType (16, $10) or romDiskType (17, $11)
	 * and if so, vcbAtDontCache is set in vcbAtrb.
	 *
	 * $01 == unspecified drive
	 * 
	 * You shouldn't normally have to mess with the vcbAtDontCache bit in the
	 * vcbAtrb. If you've written a RAM or ROM disk and you want the cache to
	 * be bypassed, you only need to support _Control csCode 23 and say
	 * you're a RAM or ROM disk. Other disk drivers probably should not mess
	 * with the vcbAtDontCache bit because any improvements we make to the
	 * File Manager cache will be lost on those drives (and we'll have to say
	 * so when customers ask why our improvements didn't help their drives).
	 *
	 * See the Inside Macintosh:Files Errata technote for a discussion of this.
	 */
	case kDriveInfo:
		Log("control - driverInfoCC\n" );
		/*  high word (bytes 2 & 3) clear  */
		/*  byte 1 = primary + fixed media + internal  */
		/*  byte 0 = drive type (0x10 = RAM disk)  */
		*(ulong*)pb->csParam = 0x00000401;
		return noErr;
		
	case 20:
		/* undocumented */
		return controlErr;
	
	case 24: /* Return SCSI csCode Partition Size */
		Log("control - cscode 24\n" );
		/* *(ulong*)pb->csParam = globe->ramSize >> 9; */
		return noErr;
		
	case accRun:
		Log("control - accRun\n" );				
		return noErr;
		
	case kDriverConfigureCode:
		Log("control - kDriverConfigureCode\n" );				
		switch( ((DriverConfigParam *)pb)->driverConfigureSelector ) {
		case kdcFlush:
			/* globe->driveNeedsFlush = false; */
			return noErr;
		}
		return controlErr;
		
	case kGetADrive: /* control call to ask the driver to create a drive*/
		Log("control - kGetADrive\n");
		return controlErr;			
	}

	lprintf("Unrecognized control cscode %d\n", pb->csCode );		
	return controlErr;
}
