/*
 * MolMouse, Copyright (C) 2001-2002 Samuel Rydh (samuel@ibrium.se)
 *
 */

#include "mol_config.h"
#include "MolMouse.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOWorkLoop.h>

#include "osi_calls.h"
#include "mouse_sh.h"

#define super IOHIPointing
OSDefineMetaClassAndStructors( MolMouse, IOHIPointing );


bool
MolMouse::start( IOService *provider )
{
	if( !super::start(provider) )
		return false;

	_irq = IOInterruptEventSource::interruptEventSource( this, (IOInterruptEventSource::Action)&MolMouse::interrupt,
							     provider, 0 );
	if( !_irq ) {
		printm("MolMouse: no irq\n");
		return false;
	}
	getWorkLoop()->addEventSource( _irq );
	_irq->enable();

	//printm("MolMouse::start\n");
	return true;
}

void
MolMouse::interrupt( IOInterruptEventSource *sender, int count )
{
	AbsoluteTime time;
	osi_mouse_t m;
	UInt32 status, buts;
	static UInt32 oldbuts;
	
	(void) OSI_GetMouse( &m );
	buts = m.event & kMouseEvent_ButMask;
	status = m.event & (kMouseEvent_MoveDelta | kMouseEvent_MoveTo);

	if( status || buts != oldbuts ) {
		clock_get_uptime(&time);
		
		if( status & kMouseEvent_MoveDelta ) {
			dispatchRelativePointerEvent( m.x, m.y, buts, time );
		} else if( status & kMouseEvent_MoveTo ) {
			Point p;
			Bounds b;
			p.x = m.x, p.y = m.y;
			b.minx = b.miny = 0; 
			b.maxx = m.width; b.maxy = m.height;
			dispatchAbsolutePointerEvent( &p, &b, buts, 0/*proximity?*/, 0, 0, 32000,
						      0/*stylusAngle*/, time );
			dispatchRelativePointerEvent( 0, 0, buts, time );
		} else {
			dispatchRelativePointerEvent( 0, 0, buts, time );
		}
		oldbuts = buts;
	}
}

OSData *
MolMouse::copyAccelerationTable()
{
	return super::copyAccelerationTable();
}

IOItemCount
MolMouse::buttonCount()
{
	return 3;
}

IOFixed
MolMouse::resolution()
{
	return (100<<16);
}
