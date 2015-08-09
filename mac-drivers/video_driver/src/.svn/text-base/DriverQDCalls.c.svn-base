
#ifndef __VideoDriverPrivate_H__
#include "VideoDriverPrivate.h"
#endif

#ifndef __VideoDriverPrototypes_H__
#include "VideoDriverPrototypes.h"
#endif

#include "DriverQDCalls.h"
#include "MOLVideo.h"
#include "linux_stub.h"
#include "LinuxOSI.h"

static OSStatus		GraphicsCoreDoSetEntries(VDSetEntryRecord *entryRecord, Boolean directDevice, UInt32 start, UInt32 stop, Boolean useValue);

/************************ Color Table Stuff ****************************/

OSStatus
GraphicsCoreSetEntries(VDSetEntryRecord *entryRecord)
{
	Boolean useValue	= (entryRecord->csStart < 0);
	UInt32	start		= useValue ? 0UL : (UInt32)entryRecord->csStart;
	UInt32	stop		= start + entryRecord->csCount;
	return GraphicsCoreDoSetEntries(entryRecord, false, start, stop, useValue);
}
						
OSStatus
GraphicsCoreDirectSetEntries(VDSetEntryRecord *entryRecord)
{
	Boolean useValue	= (entryRecord->csStart < 0);
	UInt32	start		= useValue ? 0 : entryRecord->csStart;
	UInt32	stop		= start + entryRecord->csCount;
	return GraphicsCoreDoSetEntries(entryRecord, true, start, stop, useValue);
}

OSStatus
GraphicsCoreDoSetEntries(VDSetEntryRecord *entryRecord, Boolean directDevice, UInt32 start, UInt32 stop, Boolean useValue)
{
	UInt32 i;
	
	CHECK_OPEN( controlErr );
	if (VMODE.depth != 8)
		return controlErr;
	if (NULL == entryRecord->csTable)
		return controlErr;
//	if (directDevice != (VMODE.depth != 8))
//		return controlErr;
	
	/* Note that stop value is included in the range */
	for(i=start;i<=stop;i++) {
		UInt32	tabIndex = i-start;
		UInt32	colorIndex = useValue ? entryRecord->csTable[tabIndex].value : tabIndex;
		MOLVideo_SetColorEntry(colorIndex, &entryRecord->csTable[tabIndex].rgb);
	}
	MOLVideo_ApplyColorMap();
	
	return noErr;
}

OSStatus
GraphicsCoreGetEntries(VDSetEntryRecord *entryRecord)
{
	Boolean useValue	= (entryRecord->csStart < 0);
	UInt32	start		= useValue ? 0UL : (UInt32)entryRecord->csStart;
	UInt32	stop		= start + entryRecord->csCount;
	UInt32	i;
	
	for(i=start;i<=stop;i++) {
		UInt32	tabIndex = i-start;
		UInt32	colorIndex = useValue ? entryRecord->csTable[tabIndex].value : tabIndex;
		MOLVideo_GetColorEntry(colorIndex, &entryRecord->csTable[tabIndex].rgb);
	}

	return noErr;
}

/************************ Gamma ****************************/

OSStatus
GraphicsCoreSetGamma(VDGammaRecord *gammaRec)
{
	CHECK_OPEN( controlErr );
		
	return noErr;
}

OSStatus
GraphicsCoreGetGammaInfoList(VDGetGammaListRec *gammaList)
{
	return statusErr;
}

OSStatus
GraphicsCoreRetrieveGammaTable(VDRetrieveGammaRec *gammaRec)
{
	return statusErr;
}

OSStatus
GraphicsCoreGetGamma(VDGammaRecord *gammaRecord)
{
	CHECK_OPEN( statusErr );
		
	gammaRecord->csGTable = NULL;

	return noErr;
}


/************************ Gray pages ****************************/
			
OSStatus
GraphicsCoreGrayPage(VDPageInfo *pageInfo)
{
	CHECK_OPEN( controlErr );
		
	if (pageInfo->csPage != 0)
		return paramErr;
		
	return noErr;
}
			
OSStatus
GraphicsCoreSetGray(VDGrayRecord *grayRecord)
{
	CHECK_OPEN( controlErr );
	
	GLOBAL.qdLuminanceMapping	= grayRecord->csMode;
	return noErr;
}


OSStatus
GraphicsCoreGetPages(VDPageInfo *pageInfo)
{
/*	DepthMode mode; */
	CHECK_OPEN( statusErr );

	pageInfo->csPage = 1;
	return noErr;
}

			
OSStatus
GraphicsCoreGetGray(VDGrayRecord *grayRecord)
{
	CHECK_OPEN( statusErr );
		
	grayRecord->csMode = (GLOBAL.qdLuminanceMapping);
	
	return noErr;
}

/************************ Hardware Cursor ****************************/

OSStatus
GraphicsCoreSupportsHardwareCursor(VDSupportsHardwareCursorRec *hwCursRec)
{
	CHECK_OPEN( statusErr );
		
	hwCursRec->csReserved1 = 0;
	hwCursRec->csReserved2 = 0;

	hwCursRec->csSupportsHardwareCursor = MOLVideo_SupportsHWCursor();
	return noErr;
}

OSStatus
GraphicsCoreSetHardwareCursor(VDSetHardwareCursorRec *setHwCursRec)
{
	return MOLVideo_SetHWCursor( setHwCursRec );
	/* return controlErr; */
}

OSStatus
GraphicsCoreDrawHardwareCursor(VDDrawHardwareCursorRec *drawHwCursRec)
{
	return MOLVideo_DrawHWCursor( drawHwCursRec );
	/* return controlErr; */
}

OSStatus
GraphicsCoreGetHardwareCursorDrawState(VDHardwareCursorDrawStateRec *hwCursDStateRec)
{
	return MOLVideo_GetHWCursorDrawState( hwCursDStateRec);
	/* return statusErr; */
}

/************************ Misc ****************************/

OSStatus
GraphicsCoreSetInterrupt(VDFlagRecord *flagRecord)
{
	CHECK_OPEN( controlErr );

	GLOBAL.qdInterruptsEnable = !flagRecord->csMode;
	return noErr;
}

OSStatus
GraphicsCoreGetInterrupt(VDFlagRecord *flagRecord)
{
	CHECK_OPEN( statusErr );
		
	flagRecord->csMode = !GLOBAL.qdInterruptsEnable;
	return noErr;
}

/* assume initial state is always "power-on" */
static unsigned long MOLVideoPowerState = kAVPowerOn;

OSStatus
GraphicsCoreSetSync(VDSyncInfoRec *syncInfo)
{
	unsigned char syncmask;
	unsigned long newpowermode;
	CHECK_OPEN( controlErr );

	syncmask = (!syncInfo->csFlags)? kDPMSSyncMask: syncInfo->csFlags;
	if (!(syncmask & kDPMSSyncMask)) /* nothing to do */
		return noErr;
	switch (syncInfo->csMode & syncmask) {
	case kDPMSSyncOn:
		newpowermode = kAVPowerOn;
		break;
	case kDPMSSyncStandby:
		newpowermode = kAVPowerStandby;
		break;
	case kDPMSSyncSuspend:
		newpowermode = kAVPowerSuspend;
		break;
	case kDPMSSyncOff:
		newpowermode = kAVPowerOff;
		break;
	default:
		return paramErr;
	}
	if (newpowermode != MOLVideoPowerState) {
		OSI_SetVPowerState(newpowermode);
		MOLVideoPowerState = newpowermode;
	}

	return noErr;
}

OSStatus
GraphicsCoreGetSync(VDSyncInfoRec *syncInfo)
{
	CHECK_OPEN( statusErr );
		
	if (syncInfo->csMode == 0xff) {
		/* report back the capability */
		syncInfo->csMode = 0 | ( 1 << kDisableHorizontalSyncBit)
							 | ( 1 << kDisableVerticalSyncBit)
							 | ( 1 << kDisableCompositeSyncBit);
	} else if (syncInfo->csMode == 0) {
		/* current sync mode */
		switch (MOLVideoPowerState) {
		case kAVPowerOn:
			syncInfo->csMode = kDPMSSyncOn;
			break;
		case kAVPowerStandby:
			syncInfo->csMode = kDPMSSyncStandby;
			break;
		case kAVPowerSuspend:
			syncInfo->csMode = kDPMSSyncSuspend;
			break;
		case kAVPowerOff:
			syncInfo->csMode = kDPMSSyncOff;
			break;
		}
	} else /* not defined ? */
		return paramErr;

	return noErr;
}

OSStatus
GraphicsCoreSetPowerState(VDPowerStateRec *powerStateRec)
{
	CHECK_OPEN( controlErr );

	if (powerStateRec->powerState > kAVPowerOn)
		return paramErr;
		
	if (MOLVideoPowerState != powerStateRec->powerState) {
		OSI_SetVPowerState(powerStateRec->powerState);
		MOLVideoPowerState = powerStateRec->powerState;
	}
	powerStateRec->powerFlags = 0;

	return noErr;
}

OSStatus
GraphicsCoreGetPowerState(VDPowerStateRec *powerStateRec)
{
	CHECK_OPEN( statusErr );
		
	powerStateRec->powerState = MOLVideoPowerState;
	powerStateRec->powerFlags = 0;
	return noErr;
}
		
OSStatus
GraphicsCoreSetPreferredConfiguration(VDSwitchInfoRec *switchInfo)
{
	CHECK_OPEN( controlErr );
	
	return noErr;
}

OSStatus
GraphicsCoreGetPreferredConfiguration(VDSwitchInfoRec *switchInfo)
{
	osi_get_vmode_info_t pb;
	CHECK_OPEN( statusErr );

	OSI_GetVModeInfo( 0, 0, &pb );
	
	switchInfo->csMode		= pb.cur_depth_mode;
	switchInfo->csData		= pb.cur_vmode;
	switchInfo->csPage		= 0;
	switchInfo->csBaseAddr	= FB_START(&VMODE);
	return noErr;
}

// €***************** Misc status calls *********************/

OSStatus
GraphicsCoreGetBaseAddress(VDPageInfo *pageInfo)
{
	CHECK_OPEN( statusErr );

	if (pageInfo->csPage != 0)
		return paramErr;
		
	pageInfo->csBaseAddr = FB_START(&VMODE);
	return noErr;
}
			
OSStatus
GraphicsCoreGetConnection(VDDisplayConnectInfoRec *connectInfo)
{
	CHECK_OPEN( statusErr );
		
	connectInfo->csDisplayType		= kVGAConnect;
	connectInfo->csConnectTaggedType	= 0;
	connectInfo->csConnectTaggedData	= 0;

	connectInfo->csConnectFlags		=
		(1 << kTaggingInfoNonStandard) | (1 << kUncertainConnection);
		
	connectInfo->csDisplayComponent		= 0;
	
	return noErr;
}

OSStatus
GraphicsCoreGetMode(VDPageInfo *pageInfo)
{
	CHECK_OPEN( statusErr );
	
	pageInfo->csMode		= VMODE.cur_depth_mode;
	pageInfo->csPage		= 0;
	pageInfo->csBaseAddr	= FB_START(&VMODE);
	
	return noErr;
}

OSStatus
GraphicsCoreGetCurrentMode(VDSwitchInfoRec *switchInfo)
{
	CHECK_OPEN( statusErr );
		
	switchInfo->csMode		= VMODE.cur_depth_mode;
	switchInfo->csData		= VMODE.cur_vmode;
	switchInfo->csPage		= 0;
	switchInfo->csBaseAddr	= FB_START(&VMODE);

	return noErr;
}

/********************** Video mode *****************************/
						
OSStatus
GraphicsCoreGetModeTiming(VDTimingInfoRec *timingInfo)
{
	CHECK_OPEN( statusErr );

	if (timingInfo->csTimingMode < 1 || timingInfo->csTimingMode > VMODE.num_vmodes )
		return paramErr;
	
	timingInfo->csTimingFlags	=
		(1 << kModeValid) | (1 << kModeDefault) | (1 <<kModeSafe);

	timingInfo->csTimingFormat	= kDeclROMtables;
	timingInfo->csTimingData	= timingVESA_640x480_60hz;

	return noErr;
}

OSStatus
MOLVideo_GetNextResolution(VDResolutionInfoRec *resInfo)
{
	osi_get_vmode_info_t pb;
	int id = resInfo->csPreviousDisplayModeID;

	CHECK_OPEN( statusErr );

	if( id == kDisplayModeIDFindFirstResolution )
		id = 0;
	else if( id == kDisplayModeIDCurrent )
		id = VMODE.cur_vmode;
	id++;
	
	if( id == VMODE.num_vmodes + 1) {
		resInfo->csDisplayModeID = kDisplayModeIDNoMoreResolutions;
		return noErr;
	}
	if( id < 1 || id > VMODE.num_vmodes + 1 )
		return paramErr;
	
	if( OSI_GetVModeInfo( id, kDepthMode1, &pb ) )
		return paramErr;

	resInfo->csDisplayModeID = id;
	resInfo->csHorizontalPixels	= pb.w;
	resInfo->csVerticalLines	= pb.h;
	resInfo->csRefreshRate		= pb.refresh;
	resInfo->csMaxDepthMode		= kDepthMode1 + pb.num_depths - 1;
	return noErr;
}


OSStatus
GraphicsCoreSetMode(VDPageInfo *pageInfo)
{
	osi_get_vmode_info_t pb;
		
	CHECK_OPEN( controlErr );

	if (pageInfo->csPage != 0)
		return paramErr;
	if( OSI_GetVModeInfo( VMODE.cur_vmode, pageInfo->csMode, &pb ))
		return paramErr;

	if( pageInfo->csMode != VMODE.cur_depth_mode ) {
		if( OSI_SetVMode( VMODE.cur_vmode, pageInfo->csMode ) )
			return controlErr;
		GLOBAL.vmode = pb;
	}
	pageInfo->csBaseAddr = FB_START(&VMODE);

	return noErr;
}			


OSStatus
GraphicsCoreSwitchMode(VDSwitchInfoRec *switchInfo)
{
	osi_get_vmode_info_t pb;

	CHECK_OPEN( controlErr );

	if (switchInfo->csPage != 0)
		return paramErr;
	if( OSI_GetVModeInfo( switchInfo->csData, switchInfo->csMode, &pb ))
		return paramErr;
	
	if( switchInfo->csData != VMODE.cur_vmode || switchInfo->csMode != VMODE.cur_depth_mode ) {
		if( OSI_SetVMode( switchInfo->csData, switchInfo->csMode ) )
			return controlErr;
		GLOBAL.vmode = pb;
	}
	switchInfo->csBaseAddr = FB_START(&VMODE);

	return noErr;
}

// Looks quite a bit hard-coded, isn't it ?
OSStatus
GraphicsCoreGetVideoParams(VDVideoParametersInfoRec *videoParams)
{
	osi_get_vmode_info_t pb;
	OSStatus err = noErr;
	
	CHECK_OPEN( statusErr );
		
	if (videoParams->csDisplayModeID < 1 || videoParams->csDisplayModeID > VMODE.num_vmodes )
		return paramErr;

	if( OSI_GetVModeInfo( videoParams->csDisplayModeID, videoParams->csDepthMode, &pb ) )
		return paramErr;

	videoParams->csPageCount	= 1;	
	
	(videoParams->csVPBlockPtr)->vpBaseOffset 		= 0;			// For us, it's always 0
	(videoParams->csVPBlockPtr)->vpBounds.top 		= 0;			// Always 0
	(videoParams->csVPBlockPtr)->vpBounds.left 		= 0;			// Always 0
	(videoParams->csVPBlockPtr)->vpVersion 			= 0;			// Always 0
	(videoParams->csVPBlockPtr)->vpPackType 		= 0;			// Always 0
	(videoParams->csVPBlockPtr)->vpPackSize 		= 0;			// Always 0
	(videoParams->csVPBlockPtr)->vpHRes 			= 0x00480000;	// Hard coded to 72 dpi
	(videoParams->csVPBlockPtr)->vpVRes 			= 0x00480000;	// Hard coded to 72 dpi
	(videoParams->csVPBlockPtr)->vpPlaneBytes 		= 0;			// Always 0

	(videoParams->csVPBlockPtr)->vpBounds.bottom	= pb.h;
	(videoParams->csVPBlockPtr)->vpBounds.right		= pb.w;
	(videoParams->csVPBlockPtr)->vpRowBytes			= pb.row_bytes;

	switch ( pb.depth )
	{
		case 8:
			videoParams->csDeviceType 						= clutType;
			(videoParams->csVPBlockPtr)->vpPixelType 		= 0;
			(videoParams->csVPBlockPtr)->vpPixelSize 		= 8;
			(videoParams->csVPBlockPtr)->vpCmpCount 		= 1;
			(videoParams->csVPBlockPtr)->vpCmpSize 			= 8;
			(videoParams->csVPBlockPtr)->vpPlaneBytes 		= 0;
			break;
		case 16:
			videoParams->csDeviceType 						= directType;
			(videoParams->csVPBlockPtr)->vpPixelType 		= 16;
			(videoParams->csVPBlockPtr)->vpPixelSize 		= 16;
			(videoParams->csVPBlockPtr)->vpCmpCount 		= 3;
			(videoParams->csVPBlockPtr)->vpCmpSize 			= 5;
			(videoParams->csVPBlockPtr)->vpPlaneBytes 		= 0;
			break;
		case 32:
			videoParams->csDeviceType 						= directType;
			(videoParams->csVPBlockPtr)->vpPixelType 		= 16;
			(videoParams->csVPBlockPtr)->vpPixelSize 		= 32;
			(videoParams->csVPBlockPtr)->vpCmpCount 		= 3;
			(videoParams->csVPBlockPtr)->vpCmpSize 			= 8;
			(videoParams->csVPBlockPtr)->vpPlaneBytes 		= 0;
			break;
		default:
			err = paramErr;
			break;
	}

	return err;
}
