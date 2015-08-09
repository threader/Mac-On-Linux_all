/*
 * MolMouse, Copyright (C) 2001-2002 Samuel Rydh (samuel@ibrium.se)
 *
 */

#ifndef _IOKIT_MOLMOUSE_H
#define _IOKIT_MOLMOUSE_H

#include <IOKit/hidsystem/IOHIPointing.h>
#include <IOKit/IOInterruptEventSource.h>

class MolMouse : public IOHIPointing {

	OSDeclareDefaultStructors( MolMouse );

private:
	IOInterruptEventSource 	*_irq;

public:
	virtual bool 		start(IOService *provider);

private:
	void			interrupt( IOInterruptEventSource *sender, int count );

	virtual OSData * 	copyAccelerationTable();
	virtual IOItemCount 	buttonCount();
	virtual IOFixed     	resolution();
};

#endif /* ! _IOKIT_MOLMOUSE_H */
