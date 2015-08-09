/* 
 *   Creation Date: <2002/08/07 21:59:59 samuel>
 *   Time-stamp: <2003/08/24 16:27:16 samuel>
 *   
 *	<MolAccel.cpp>
 *	
 *	MOL acceleration
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "MolAccel.h"
#include "osi_calls.h"

#define super IOService
OSDefineMetaClassAndStructors( MolAccel, IOService );

UInt32
MolAccel::getFuncAddr( char *fname )
{
	char buf[128];
	sprintf( buf, "accel-%s", fname );
	
	OSData *s = OSDynamicCast( OSData, getProvider()->getProperty(buf) );
	if( !s || !s->getBytesNoCopy() )
		return 0;
	return *(UInt32*)s->getBytesNoCopy();
}

void
MolAccel::replaceFunction( UInt32 *fentry, char *fname )
{
	UInt32 addr = getFuncAddr( fname );
	UInt32 inst;

	if( !addr ) {
		printm("Function %s is missing\n", fname );
		return;
	}
	if( addr & 0xfc000003 ) {
		printm("Out of range!\n");
		return;
	}

	inst = 0x48000002 | addr;	/* ba ... */

	write_hook( fentry, inst );
	// XXX: flush cache 
}


bool
MolAccel::start( IOService *provider )
{
	super::start( provider );
	
	write_hook = (void(*)(UInt32*,UInt32)) getFuncAddr("write_hook");
	if( !write_hook )
		printm("write_hook missing!\n");

	return true;
}
