
#include "VideoDriverPrivate.h"
#include "VideoDriverPrototypes.h"
#include "DriverQDCalls.h"

#include "MOLVideo.h"
#include "linux_stub.h"
#include "mouse_sh.h"
#include "logger.h"
#include "LinuxOSI.h"


OSStatus
MOLVideo_Init( void )
{
#ifndef OS_X
	GLOBAL.useHWCursor = OSI_VideoCntrl( kVideoUseHWCursor );
	OSI_VideoCntrl( kVideoStartVBL );
#else	
	GLOBAL.useHWCursor = false;

	/* This is a good place to test if the OSX audio and network drivers
	 * have loaded. If not, the driver disk is virtually inserted by mol.
	 * This has of course very little to do with video...
	 */
	OSI_CMountDrvVol();
#endif
	return noErr;
}

OSStatus
MOLVideo_Exit( void )
{
	OSI_VideoCntrl( kVideoStopVBL );
	return noErr;
}

OSStatus
MOLVideo_Open( void )
{
	osi_get_vmode_info_t pb;
	lprintf("Video Driver v1.12\n");

	/* get the default mode */ 
	if( OSI_GetVModeInfo( 0, 0, &pb ))
		return controlErr;	/* XXX: find a better errorcode */ 
	GLOBAL.vmode = pb;
	GLOBAL.isOpen = true;

	OSI_VideoCntrl( kVideoStartVBL );

	//lprintf("Is open: %d*%d depth %d, rb %d, offs %d\n",
	//	VMODE.w, VMODE.h, VMODE.depth, VMODE.row_bytes, VMODE.offset );
	return noErr;
}

OSStatus
MOLVideo_Close( void )
{
	//lprintf("MOLVideo_Close\n");

	OSI_VideoCntrl( kVideoStopVBL );
	return noErr;
}

OSStatus
MOLVideo_SetColorEntry( UInt32 index, RGBColor *color )
{
	int c = ((int)(color->red >> 8) << 16 )
		| ((int)(color->green >> 8) << 8 )
		| (int)(color->blue >> 8 );

	OSI_SetColor( index, c );
	return noErr;
}

OSStatus
MOLVideo_GetColorEntry( UInt32 index, RGBColor *color )
{
	int c = OSI_GetColor( index );

	if( color ) {
		color->red = (c >> 8) & 0xff00;
		color->green = c & 0xff00;
		color->blue = (c << 8) & 0xff00;
	}
	return noErr;
}

OSStatus
MOLVideo_ApplyColorMap( void )
{
	OSI_SetColor( -1, 0 );
	return noErr;
}


/* Hardware cursor (or more probably, X-cursor) */
int
MOLVideo_SupportsHWCursor( void )
{
	//lprintf("MOLVideo_SupportHWCursor %d\n", GLOBAL.useHWCursor );

	return GLOBAL.useHWCursor;
}

OSStatus
MOLVideo_SetHWCursor( VDSetHardwareCursorRec *p )
{
	//lprintf("MOLVideo_SetHWCursor\n");

	if( !GLOBAL.useHWCursor )
		return controlErr;

	/* p->csCursorRef points to data? */
	return noErr;
}

OSStatus
MOLVideo_DrawHWCursor( VDDrawHardwareCursorRec *p )
{
	//lprintf("MOLVideo_DrawHWCursor\n");

	if( !GLOBAL.useHWCursor )
		return controlErr;

	GLOBAL.csCursorX = p->csCursorX;
	GLOBAL.csCursorY = p->csCursorY;
	GLOBAL.csCursorVisible = p->csCursorVisible;
	return noErr;
}

OSStatus
MOLVideo_GetHWCursorDrawState( VDHardwareCursorDrawStateRec *p )
{
	//lprintf("MOLVideo_GetHWCursorDrawState\n");

	if( !GLOBAL.useHWCursor )
		return statusErr;

	CLEAR( *p );
	p->csCursorX = GLOBAL.csCursorX;
	p->csCursorY = GLOBAL.csCursorY;
	p->csCursorVisible = GLOBAL.csCursorVisible;
	p->csCursorSet = true;	/* last call OK */
	return noErr;
}	

#pragma internal off

InterruptMemberNumber
PCIInterruptServiceRoutine( InterruptSetMember member, void *refCon, UInt32 theIntCount )
{
	int events;
	
	//lprintf("Video interrupt poll memb=%d count=%d \n", member, theIntCount );
	if( !OSI_VideoAckIRQ( &events ) )
		return kIsrIsNotComplete;

	if( GLOBAL.inInterrupt )
		return kIsrIsComplete;
	GLOBAL.inInterrupt = 1;

	if( events & VIDEO_EVENT_CURS_CHANGE )
		GLOBAL.useHWCursor = OSI_VideoCntrl( kVideoUseHWCursor );		

	if( GLOBAL.qdVBLInterrupt && GLOBAL.qdInterruptsEnable ) /* SR */
		VSLDoInterruptService( GLOBAL.qdVBLInterrupt );

	GLOBAL.inInterrupt = 0;
	return kIsrIsComplete;
}
