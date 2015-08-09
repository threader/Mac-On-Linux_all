/*
 * MolNVRAM, Copyright (C) 2001-2002 Samuel Rydh (samuel@ibrium.se)
 *
 */

#include "mol_config.h"
#include "MolNVRAM.h"
#include <IOKit/IOLib.h>
#include "osi_calls.h"

#define super IONVRAMController
OSDefineMetaClassAndStructors(MolNVRAM, IONVRAMController);


bool
MolNVRAM::start(IOService *provider)
{
	bool success = super::start(provider);

	if( !success )
		return false;

	//printm("MolNVRAM::start\n");
	return success;
}

void
MolNVRAM::sync( void )
{
	//printm("MolNVRAM::sync\n");
	super::sync();
}

IOReturn
MolNVRAM::read( IOByteCount offset, UInt8 *buffer, IOByteCount length )
{
	unsigned int i;
	// SIZE = kIODTNVRAMImageSize ?

	//printm("MolNVRAM::read %08lx\n", length);
	for( i=0; i<length; i++ )
		*buffer++ = OSI_ReadNVRamByte( offset++ );
	return kIOReturnSuccess;
}

IOReturn
MolNVRAM::write(IOByteCount offset, UInt8 *buffer, IOByteCount length)
{
	unsigned int i;

	printm("MolNVRAM::write %08lx\n", length);
	for( i=0; i<length; i++ )
		OSI_WriteNVRamByte( offset++, *buffer++ );
	return kIOReturnSuccess;
}
