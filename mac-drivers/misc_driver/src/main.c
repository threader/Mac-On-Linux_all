/* 
 *   Creation Date: <1999/03/20 07:30:18 samuel>
 *   Time-stamp: <2003/01/03 02:47:28 samuel>
 *   
 *	<driver.c>
 *	
 *	Mouse Driver
 *   
 *   Copyright (C) 1999, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 */
  
#include <DriverServices.h>
#include <DriverGestalt.h>
#include <CursorDevices.h>
#include <DateTimeUtils.h>
#include <Gestalt.h>
#include "global.h"
#include "logger.h"
#include "LinuxOSI.h"
#include "IRQUtils.h"
#include "mouse_sh.h"
#include "video_sh.h"
#include "tasktime.h"

static struct {
	IRQInfo			irq_info;

	/* cursor device */
	CursorDevice		cursorDeviceInfo;	
	CursorDevicePtr		cursDev;

	/* hardware/software cursor */
	int			hw_cursor;
	int			tt_pending;
} m;


static void 
show_cursor( int param1, int param2 )
{
	HideCursor();
	SetCursor( &qd.arrow );
	ShowCursor();
	m.tt_pending = 0;
}

static void
hwsw_curs_switch( void ) 
{
	int hw_cursor = OSI_VideoCntrl( kVideoUseHWCursor );

	if( hw_cursor != m.hw_cursor ) {
		m.hw_cursor = hw_cursor;
		if( !m.tt_pending ) {
			m.tt_pending = 1;
			DNeedTime( show_cursor, 0, 0 );
		}
	}
}


static long
soft_irq( void *p1, void *p2 )
{
	static int cnt;
	osi_mouse_t pb;

	(void) OSI_GetMouse( &pb );

	if( (pb.event & kMouseEvent_MoveTo) )
		CursorDeviceMoveTo( m.cursDev, pb.x, pb.y );
	else if( (pb.event & kMouseEvent_MoveDelta) )
		CursorDeviceMove( m.cursDev, pb.x, pb.y );

	CursorDeviceButtons( m.cursDev, (pb.event & kMouseEvent_ButMask) );

	/* detect hardware/software cursor switches */
	if( (pb.event & kMouseEvent_HwSwCursSwitch) )
		hwsw_curs_switch();

	return 0;
}


static InterruptMemberNumber
hard_irq( InterruptSetMember ISTmember, void *ref_con, UInt32 the_int_count )
{
	if( !OSI_MouseAckIRQ() )
		return kIsrIsNotComplete;

	QueueSecondaryInterruptHandler( soft_irq, NULL, NULL, NULL );		
	return kIsrIsComplete;
}

#if 0
static void
custom_butclick( CursorDevicePtr dev, short button )
{
	static int toggle=0;
	
	if( !toggle ) {
		PostEvent( keyDown, (0x3b << 8) | 0xff );
		PostEvent( mouseDown, 0 );
	} else {
		PostEvent( mouseUp, 0 );
		PostEvent( keyUp, (0x3b << 8) | 0xff );
	}
}
#endif

static void 
mouse_init( void )
{
	//CursorDeviceCustomButtonUPP func = NewCursorDeviceCustomButtonUPP( custom_butclick );
	UInt32 mouse_dpi;

	CLEAR( m );
	DNeedTime_Init();

	m.cursDev = &m.cursorDeviceInfo;
	CursorDeviceNewDevice( &m.cursDev );
					
	CursorDeviceSetAcceleration( m.cursDev, (Fixed)(1<<16) );

	CursorDeviceSetButtons( m.cursDev, 3);
	CursorDeviceButtonOp( m.cursDev, 0, kButtonSingleClick, 0L );
	CursorDeviceButtonOp( m.cursDev, 1, kButtonDoubleClick, 0L );
	CursorDeviceButtonOp( m.cursDev, 2, kButtonDoubleClick, 0L );
	
	//CursorDeviceButtonOp( m.cursDev, 2, kButtonCustom, (long)func );

	mouse_dpi = OSI_MouseCntrl( kMouseGetDPI );
	CursorDeviceUnitsPerInch( m.cursDev, (Fixed)((Fixed)(mouse_dpi<<16)) );
	
	if( InstallIRQ( &GLOBAL.deviceEntry, hard_irq, &m.irq_info ) ) {
		lprintf("Mouse: Can't install irq!\n");
		return;
	}
	OSI_MouseCntrl1( kMouseRouteIRQ, m.irq_info.aapl_int );
	
	lprintf( "Mouse Driver v1.0\n" );
}


/************************************************************************/
/*	Generic driver stuff						*/
/************************************************************************/

OSStatus
DriverWriteCmd(	IOParam *pb, int is_immediate, IOCommandID ioCommandID )
{
	return nsDrvErr;
}

OSStatus
DriverReadCmd( IOParam *pb, int is_immediate, IOCommandID ioCommandID ) 
{
	return nsDrvErr;
}

static OSStatus
DriverGestaltHandler( DriverGestaltParam *pb )
{
	switch( pb->driverGestaltSelector ) {
	case kdgSync: 
		pb->driverGestaltResponse = false;	/* We handle asynchronous I/O */
		return noErr;
	}
	return statusErr;
}

OSStatus
DriverStatusCmd( CntrlParam *pb )
{
	switch( pb->csCode ) {
	case kDriverGestaltCode:
		return DriverGestaltHandler( (DriverGestaltParam*)pb );
	}
	return statusErr;
}

OSStatus
DriverControlCmd( CntrlParam *pb )
{
	return controlErr;
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static pascal OSErr
mol_gestalt_selector( OSType selector, long *ret )
{
	if( ret )
		*ret = MOL_GESTALT_VALUE;
	return noErr;
}


OSErr
Initialize( void )
{
	SetDateTime( OSI_GetTime() );

	NewGestalt( MOL_GESTALT_SELECTOR, 
		    NewSelectorFunctionProc(mol_gestalt_selector));

	mouse_init();
	return noErr;
}

