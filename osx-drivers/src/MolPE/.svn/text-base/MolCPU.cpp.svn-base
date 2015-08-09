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

#include "mol_config.h"
extern "C" {
#include <ppc/proc_reg.h>
}
#include <IOKit/IODeviceTreeSupport.h>
#include <IOKit/IOPlatformExpert.h>

#include "MolCPU.h"
#include "MolPE.h"
#include "osi_calls.h"

#define super IOCPU
OSDefineMetaClassAndStructors( MolCPU, IOCPU );


bool
MolCPU::start(IOService *provider)
{
	ml_processor_info_t processor_info;
	kern_return_t result;
	MolPE *molBoard;

	// printm("MolCPU::start\n");

	// Checks the board:
	molBoard = OSDynamicCast( MolPE, getPlatform() );
	if( !molBoard ) {
		printm("MolCPU::start this is not a MolPE\n");
		return false;
	}

	if( !super::start(provider) ) {
		printm("MolCPU::start can't init super\n");
		return false;
	}

	setCPUNumber(0);

	cpuIC = new IOCPUInterruptController;
	if( !cpuIC ) {
		printm("MolCPU::start Can't instanciate IOCPUInterruptController\n");
		return false;
	}

	if( cpuIC->initCPUInterruptController(1) != kIOReturnSuccess ) {
		printm("MolCPU::start Can't init IOCPUInterruptController\n");
		return false;
	}

	cpuIC->attach( this );

	cpuIC->registerCPUInterruptController();

	setCPUState( kIOCPUStateUninitalized );

	memset( &processor_info, 0, sizeof(processor_info) );
	processor_info.cpu_id = (cpu_id_t)this;
	processor_info.boot_cpu = true;
	processor_info.start_paddr = 0x100;
	processor_info.l2cr_value = mfl2cr() & 0x7FFFFFFF;	// cache-disabled value
	processor_info.supports_nap = false;			// doze, do not nap
	processor_info.time_base_enable = 0;

	// Register this CPU with mach.
	result = ml_processor_register(&processor_info, &machProcessor, &ipi_handler);
	if( result == KERN_FAILURE ) {
		printm("MolCPU::start Can't register processor\n");
		return false;
	}
	
	processor_start(machProcessor);

	//printm("MolCPU::start successfull\n");
    
	registerService();
	return true;
}

void
MolCPU::ipiHandler(void *refCon, void *nub, int source)
{
	// Call mach IPI handler for this CPU.
	if( ipi_handler )
		ipi_handler();
}

void
MolCPU::initCPU(bool boot)
{
	// Init the interrupts.
	if( boot )
		cpuIC->enableCPUInterrupt(this);

	setCPUState(kIOCPUStateRunning);
}

void
MolCPU::quiesceCPU()
{
	printm("MolCPU::quiesceCPU calling ml_ppc_sleep\n");
	ml_ppc_sleep();
}

const OSSymbol *
MolCPU::getCPUName()
{
	return (OSSymbol*)OSSymbol::withCStringNoCopy("Primary0");
}

kern_return_t
MolCPU::startCPU( vm_offset_t /*start_paddr*/, vm_offset_t /*arg_paddr*/ )
{
	return KERN_SUCCESS;
}

void
MolCPU::haltCPU()
{
	printm("MolCPU::haltCPU Here!\n");

	setCPUState(kIOCPUStateStopped);
	processor_exit(machProcessor);
}

void
MolCPU::enableCPUTimeBase( bool enable )
{
	printm("MolCPU::enableCPUTimeBase: %d\n", enable );
}

void 
MolCPU::signalCPU( IOCPU *target )
{
	printm("MolCPU::signalCPU\n");
}
