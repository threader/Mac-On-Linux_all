/* 
 *   MOL adaption (2001/01/03)
 *
 *   (C) 2001-2003 Samuel Rydh, <samuel@ibrium.se>
 *
 */
/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
 *  drivers.c - Driver Loading Functions.
 *
 *  Copyright (c) 2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include "sl.h"

#define kDriverPackageSignature1 0x4d4b5854	/* 'MKXT' */
#define kDriverPackageSignature2 0x4d4f5358	/* 'MOSX' */

struct DriversPackage {
	unsigned long signature1;
	unsigned long signature2;
	unsigned long length;
	unsigned long alder32;
	unsigned long version;
	unsigned long numDrivers;
	unsigned long reserved1;
	unsigned long reserved2;
};
typedef struct DriversPackage DriversPackage;


// Assumes mkext driver has been loaded to kLoadAddr already
long 
AddDriverMKext( void )
{
	long driversAddr, driversLength;
	char segName[32];

	DriversPackage *package = (DriversPackage *)kLoadAddr;

	// Verify the MKext.
	if( (package->signature1 != kDriverPackageSignature1) ||
	    (package->signature2 != kDriverPackageSignature2)) return -1;
	if( package->length > kLoadSize) return -1;
	if( package->alder32 != Alder32((unsigned char *)&package->version,
					package->length - 0x10)) return -1;

	// Make space for the MKext.
	driversLength = package->length;
	driversAddr = AllocateKernelMemory(driversLength);

	// Copy the MKext.
	memcpy((void *)driversAddr, (void *)kLoadAddr, driversLength);
	
	// Add the MKext to the memory map.
	sprintf(segName, "DriversPackage-%lx", driversAddr);
	//printm("DriversPackage-%lx\n", driversAddr);
	AllocateMemoryRange(segName, driversAddr, driversLength);
	
	return 0;
}
