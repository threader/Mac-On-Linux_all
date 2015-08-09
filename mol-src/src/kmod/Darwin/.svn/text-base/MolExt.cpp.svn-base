/*
 *   Creation Date: <2002/01/09 22:43:20 samuel>
 *   Time-stamp: <2004/03/13 13:20:21 samuel>
 *   
 *	<MolExt.cpp>
 *	
 *	
 *   
 *   Copyright (C) 2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "archinclude.h"
#include "alloc.h"
#include "MolExt.h"

extern "C" {
#include "version.h"
#include "kernel_vars.h"
#include "misc.h"
#include "osx.h"
}


#define super IOService
OSDefineMetaClassAndStructors( MolExt, IOService )


/************************************************************************/
/*	MolExt Object							*/
/************************************************************************/

bool
MolExt::start( IOService *provider )
{
	bool res = super::start( provider );

	IOLog("MolExt::start\n");
	dev_register();
	return res;
}

void
MolExt::stop( IOService *provider )
{
	IOLog("MolExt::stop\n");
	super::stop( provider );
	dev_unregister();
}
