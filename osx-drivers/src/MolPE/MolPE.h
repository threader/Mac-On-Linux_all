/*
 * MolCPU, Copyright (C) 2001-2002 Samuel Rydh (samuel@ibrium.se)
 *
 * Copyright (c) 1998-2000 Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#ifndef _IOKIT_MOLPE_H
#define _IOKIT_MOLPE_H

#include <IOKit/IOPlatformExpert.h>

#define kMolStdMachineType	1
#define kChipSetTypeMol		170


class MolPE : public IODTPlatformExpert {

	OSDeclareDefaultStructors( MolPE );
  
public:
	virtual bool		start(IOService *provider);
	virtual const char 	*deleteList();
	virtual const char 	*excludeList();
	virtual bool		platformAdjustService( IOService *service );
	virtual IOReturn 	callPlatformFunction( const OSSymbol *functionName, bool waitForFunction,
						      void *p1, void *p2, void *p3, void *p4 );

	virtual long		getGMTTimeOfDay();
	virtual void		setGMTTimeOfDay( long secs );
	virtual bool		getMachineName( char *name, int maxLength );

	virtual int		haltRestart( unsigned int type );
};

#endif
