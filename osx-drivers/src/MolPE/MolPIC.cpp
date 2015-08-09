/*
 * MolPIC, Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
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
 */

#include "mol_config.h"
#include <IOKit/IOPlatformExpert.h>
#include "MolPIC.h"
#include "osi_calls.h"

#undef super
#define super IOInterruptController

OSDefineMetaClassAndStructors( MolPIC, IOInterruptController );


bool
MolPIC::start( IOService *provider )
{
	OSObject *tmpObject;
	IOInterruptAction handler;
	int cnt;

	// If needed call the parents start.
	if( !super::start(provider) )
		return false;

	// Get the interrupt controller name from the provider's properties
	tmpObject = provider->getProperty("InterruptControllerName");
	interruptControllerName = OSDynamicCast( OSSymbol, tmpObject );

	if( !interruptControllerName )
		return false;

	// For now you must allocate storage for the vectors.
	// This will probably changed to something like: initVectors(numVectors).
	// In the mean time something like this works well.
	numVectors = 32;

	// Allocate the memory for the vectors.
	vectors = (IOInterruptVector *)IOMalloc( numVectors * sizeof(IOInterruptVector) );
	if (vectors == NULL) return false;
	bzero(vectors, numVectors * sizeof(IOInterruptVector));
	
	// Allocate locks for the vectors.
	for( cnt = 0; cnt < numVectors; cnt++ ) {
		vectors[cnt].interruptLock = IOLockAlloc();
		if( !vectors[cnt].interruptLock ) {
			for( cnt = 0; cnt < numVectors; cnt++ ) {
				if( vectors[cnt].interruptLock )
					IOLockFree( vectors[cnt].interruptLock );
			}
		}
	} 

	// If you know that this interrupt controller is the primary
	// interrupt controller, use this to set it nub properties properly.
	// This may be done by the nub's creator.
	getPlatform()->setCPUInterruptProperties( provider );
	
	// register the interrupt handler so it can receive interrupts.
	handler = getInterruptHandlerAddress();
	provider->registerInterrupt( 0, this, handler, 0 );
	
	// Just like any interrupt source, you must enable it to receive interrupts.
	provider->enableInterrupt(0);
	
	// Register this interrupt controller so clients can find it.
	getPlatform()->registerInterruptController( interruptControllerName, this );

	//printm("MolPIC::started\n");
	return true;
}

IOReturn
MolPIC::getInterruptType( IOService *nub, int source, int *interruptType )
{
	if( !interruptType ) 
		return kIOReturnBadArgument;
  
	*interruptType = kIOInterruptTypeLevel;
	return kIOReturnSuccess;
}

// Sadly this just has to be replicated in every interrupt controller.
IOInterruptAction
MolPIC::getInterruptHandlerAddress()
{
	return (IOInterruptAction)&MolPIC::handleInterrupt;
}

// Handle all current interrupts.
IOReturn
MolPIC::handleInterrupt( void *refCon, IOService *nub, int source )
{
	IOInterruptVector *vector;
	ulong irqbits;
	int i, vectorNumber;

	for( ;; ) {
		// Get vectorNumber from hardware some how and clear the event.
		irqbits = OSI_PICGetActiveIRQ();
		vectorNumber = 0;
		for( i=0; i<=31; i++ )
			if( irqbits & (1UL<<i) ) {
				vectorNumber = i;
				break;
			}

		// Break if there are no more vectors to handle.
		if( !vectorNumber )
			break;
		
		// Get the vector's date from the controller's array.
		vector = &vectors[vectorNumber];
		
		// Set the vector as active. This store must compleat before
		// moving on to prevent the disableInterrupt fuction from
		// geting out of sync.
		vector->interruptActive = 1;
		
		// If the vector is not disabled soft, handle it.
		if( !vector->interruptDisabledSoft ) {
			// Prevent speculative exacution as needed on your processor.
			//isync();
			
			// Call the handler if it exists.
			if( vector->interruptRegistered) {
				vector->handler( vector->target, vector->refCon,
						 vector->nub, vector->source );
			}
			if( vector->interruptDisabledSoft ) {
				// Hard disable the source
				vector->interruptDisabledHard = 1;
				disableVectorHard( vectorNumber, vector );
			}
		} else {
			// Hard disable the vector if is was only soft disabled.
			vector->interruptDisabledHard = 1;
			disableVectorHard( vectorNumber, vector );
		}
		
		// Done with this vector so, set it back to inactive.
		vector->interruptActive = 0;
	}
  
	return kIOReturnSuccess;
}

bool
MolPIC::vectorCanBeShared( long vectorNumber, IOInterruptVector *vector )
{
	// Given the vector number and the vector data, return if it can be shared.
	// printm("IRQ vectorCanBeShared %d\n", vectorNumber );
	//return true;
	return false;
}

void
MolPIC::initVector( long vectorNumber, IOInterruptVector *vector )
{
	// Given the vector number and the vector data,
	// get the hardware ready for the vector to generate interrupts.
	// Make sure the vector is left disabled.

	//printm("MolPIC::initVector %d\n", vectorNumber );
}

void
MolPIC::disableVectorHard( long vectorNumber, IOInterruptVector *vector )
{
	//printm("disableVectorHard %d\n", vectorNumber );

	// Given the vector number and the vector data,
	// disable the vector at the hardware.

	OSI_PICMaskIRQ( vectorNumber );
}

void
MolPIC::enableVector( long vectorNumber, IOInterruptVector *vector )
{
	//printm("enableVector %d\n", vectorNumber );

	// Given the vector number and the vector data,
	// enable the vector at the hardware
	OSI_PICUnmaskIRQ( vectorNumber );
}

void
MolPIC::causeVector( long vectorNumber, IOInterruptVector *vector )
{
	printm("MolPIC::causeVector %d\n", vectorNumber );

	// Given the vector number and the vector data,
	// Set the vector pending and cause an interrupt at the parent controller.
	
	// cause the interrupt at the parent controller.  Source is usually zero,
	// but it could be different for your controller.
	getPlatform()->causeInterrupt(0);
}
