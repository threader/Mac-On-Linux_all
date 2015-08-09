/* 
 *   Creation Date: <1999/03/20 07:30:18 samuel>
 *   Time-stamp: <2002/12/08 03:02:33 samuel>
 *   
 *	<driver.c>
 *	
 *	IRQ Test
 *   
 *   Copyright (C) 1999, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 */
  
#include <DriverServices.h>
#include <DriverGestalt.h>
#include "global.h"
#include "logger.h"
#include "LinuxOSI.h"
#include "IRQUtils.h"

enum { kIRQTestGetID=0, kIRQTestAck0, kIRQTestAck1, kIRQTestRouteIRQ0, kIRQTestRouteIRQ1,
       kIRQTestRaise0, kIRQTestRaise1 };

static int id;

static long
SoftInterrupt( void *p1, void *p2 )
{
	lprintf("secondary interrupt [%d]\n", id);
	OSI_IRQTest( kIRQTestRaise0 + !id, 0 );
	OSI_Debugger(1);
	lprintf("secondary interrupt2 [%d]\n", id);
	for( ;; )
		;
	return 0;
}


static InterruptMemberNumber
hard_irq( InterruptSetMember ISTmember, void *ref_con, UInt32 the_int_count )
{
	if( !OSI_IRQTest( kIRQTestAck0 + id, 0 ) ) {
		lprintf("not complete [%d]\n", id);
		return kIsrIsNotComplete;
	}
	lprintf("IRQ_test: hard_irq [%d]\n", id);

//	if( !id ) {
		QueueSecondaryInterruptHandler( SoftInterrupt, NULL, NULL, NULL );		
		return kIsrIsComplete;
//	}
#if 1
	for(;;)
		;


	
	if( OSI_IRQTest( kIRQTestAck0 + id, 0 ) )
		return kIsrIsNotComplete;
#endif
	return kIsrIsComplete;
}


OSStatus
DriverWriteCmd(	IOParam *pb, int is_immediate, IOCommandID ioCommandID )
{
	lprintf("DriverWriteCmd\n");
	return nsDrvErr;
}

OSStatus
DriverReadCmd( IOParam *pb, int is_immediate, IOCommandID ioCommandID ) 
{
	lprintf("DriverReadCmd\n");
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
	lprintf("DriverControlCmd\n");
	return controlErr;
}

OSErr 
Initialize( void )
{
	static IRQInfo irq_info;
	
	if( InstallIRQ( &GLOBAL.deviceEntry, hard_irq, &irq_info ) ) {
		lprintf("Ablk: Can't install irq!\n");
		return ioErr;
	}
	id = OSI_IRQTest( kIRQTestGetID, GetOSIID(&irq_info) );

	lprintf("IRQTest[%d] - Initialize\n", id);
	OSI_IRQTest( kIRQTestRouteIRQ0 + id, irq_info.aapl_int );

	return noErr;
}
