/*
 * MolCPU, Copyright (C) 2001-2003 Samuel Rydh (samuel@ibrium.se)
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
/*
 * Copyright (c) 1999 Apple Computer, Inc.  All rights reserved.
 *
 *  DRI: Josh de Cesare
 *
 */

#include "mol_config.h"

extern "C" {
#include <machine/machine_routines.h>
}

#include <IOKit/pwr_mgt/RootDomain.h>
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/platform/ApplePlatformExpert.h>
#include "MolPE.h"
#include "osi_calls.h"


#define super IODTPlatformExpert
OSDefineMetaClassAndStructors(MolPE, IODTPlatformExpert);


bool
MolPE::start( IOService *provider )
{
	bool ret;
	setChipSetType(kChipSetTypeMol);
	setMachineType(kMolStdMachineType);
	setBootROMType(kBootROMTypeNewWorld);
	
	//printm("MolPE::start\n");
    
	// power mgr features (XXX: check this!)
	_pePMFeatures     = kStdDesktopPMFeatures;
	_pePrivPMFeatures = kStdDesktopPrivPMFeatures;      
	_peNumBatteriesSupported = kStdDesktopNumBatteries;

	ret = super::start(provider);

	publishResource("IORTC", this );
	
	//printm("MolPE::start - ret %d\n", ret );
	return ret;
}

const char *
MolPE::deleteList()
{
	return( "('packages', 'psuedo-usb', 'psuedo-hid', 'multiboot', 'rtas')" );
}

const char *
MolPE::excludeList()
{
    return( "('chosen', 'memory', 'openprom', 'AAPL,ROM', 'rom', 'options', 'aliases')");
}

static const char *
getClassName( const OSObject *obj ) 
{
	const OSMetaClass *meta = obj->getMetaClass();
	return (meta) ? meta->getClassName() : "";
}

static void
adjustBlockDevice( IOService *mblk, IOService *media ) 
{
	char location[16];
	IOService *s;
	OSNumber *num;
	
	if( media->inPlane(gIODTPlane) )
		return;

	for( s=media ; s && !s->inPlane(gIODTPlane); s=s->getProvider() )
		;

	// Don't add device the node if there is a partition table
	if( !s || mblk->getProperty("HasPartable") )
		return;

	if( !(num=OSDynamicCast( OSNumber, mblk->getProperty( "IOUnit" ) )) ) {
		printm("IOUnit missing\n");
		return;
	}
	media->attachToParent( s, gIODTPlane );
	sprintf(location, "%x:0", num->unsigned32BitValue() );
	media->setLocation( location, gIODTPlane );
	media->setName("", gIODTPlane );
}

bool
MolPE::platformAdjustService( IOService *service )
{
	/* this seem hackish, but this is how other platforms do it... */
	if( IODTMatchNubWithKeys( service, "osi-pic") ) {
		const OSSymbol *tmpSymbol = IODTInterruptControllerName(service);
		service->setProperty("InterruptControllerName", (OSSymbol*)tmpSymbol );
		return true;
	}
	//printm("platformAdjustService: %s\n", service->getName() );
	if( !strcmp("IOMedia", getClassName(service)) ) {
		IOService *p = service->getProvider();
		if( p && (p=p->getProvider()) && !strcmp("MolBlockDevice", getClassName(p)) ) {
			adjustBlockDevice( p, service );
		}
	}
	return true;
}

IOReturn
MolPE::callPlatformFunction( const OSSymbol *functionName, bool waitForFunction, 
			     void *param1, void *param2, void *param3, void *param4 )
{
	return super::callPlatformFunction(functionName, waitForFunction,
					   param1, param2, param3, param4);
}

long
MolPE::getGMTTimeOfDay()
{
	//printm("getGMTTimeOfDay()\n");
	return OSI_GetGMTTime();
}

void
MolPE::setGMTTimeOfDay( long secs )
{
	// Time machine needed...
	//printm("setGMTTimeOfDay()\n");
}

bool
MolPE::getMachineName( char *name, int maxLength )
{
	strncpy( name, "Power Macintosh", maxLength );
	return true;
}

int
MolPE::haltRestart( unsigned int type )
{
	if( type == kPEHaltCPU || type == kPERestartCPU )
		OSI_Exit();
	return super::haltRestart( type );
}
