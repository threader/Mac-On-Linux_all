/*
 * Copyright (c) 2001, 2002, 2003 Samuel Rydh
 */

#ifndef _IOKIT_MOLKEYBOARD_H
#define _IOKIT_MOLKEYBOARD_H

#include <IOKit/hidsystem/IOHIKeyboard.h>
#include <IOKit/IOInterruptEventSource.h>

class MolKeyboard : public IOHIKeyboard
{
	OSDeclareDefaultStructors( MolKeyboard );

private:
	IOInterruptEventSource 	*_irq;

public:
	void			interrupt( IOInterruptEventSource *sender, int count );
	virtual UInt32		interfaceID( void );
	virtual UInt32		deviceType( void );

	virtual bool		start(IOService *provider);

	virtual const unsigned char *defaultKeymapOfLength( UInt32 * length );
	virtual void		setAlphaLockFeedback( bool val );
	virtual void		setNumLockFeedback( bool val );
	virtual UInt32		maxKeyCodes();
};

#endif /* ! _IOKIT_MOLKEYBOARD_H */
