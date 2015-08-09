/*
 * MolNVRAM, Copyright (C) 2001-2002 Samuel Rydh (samuel@ibrium.se)
 *
 */

#ifndef _IOKIT_MOLNVRAM_H
#define _IOKIT_MOLNVRAM_H

#include <IOKit/nvram/IONVRAMController.h>

class MolNVRAM : public IONVRAMController {

	OSDeclareDefaultStructors(MolNVRAM);
  
public:
	virtual bool		start(IOService *provider);

	virtual void 		sync(void);
 	virtual IOReturn 	read( IOByteCount offset, UInt8 *buffer,IOByteCount length );
	virtual IOReturn	write( IOByteCount offset, UInt8 *buffer, IOByteCount length );
};

#endif
