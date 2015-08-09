/*								GetDeviceLogicalAddress.c							*/
/*
 * GetDeviceLogicalAddress.c
 * Copyright © 1994-95 Apple Computer Inc. All rights reserved.
 *
 */
/*	.___________________________________________________________________________________.
  	| This device retrieves the LogicalAddress and length of a PCI device "register"	|
  	| It is independent of the Framework driver. This function may only be called		|
  	| from the driver initialization functions.											|
  	| This may need extension for certain hardware configurations.						|
	.___________________________________________________________________________________.
*/
/*
 * Revised 95.04.07 to use the definitions in PCI.h
 */

/* #define DEBUG */
#include "logger.h"

#ifndef __VideoDriverPrivate_H__
#include "VideoDriverPrivate.h"
#endif

#ifndef __VideoDriverPrototypes_H__
#include "VideoDriverPrototypes.h"
#endif


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * GetDeviceLogicalAddress
 *
 * Retrieve the assigned-address property from the Name Registry and search it for
 * the specified addressSpaceSelector. This function uses the device base register
 * only: it ignores the I/O vs. Memory space selector. It will need modification if
 * the hardware supports 64-bit addressing or needs to understand address spaces.
 */
OSStatus
GetDeviceLogicalAddress(
	RegEntryIDPtr		regEntryIDPtr,			/* Driver's Name Registery ID	*/
	PCIRegisterNumber	deviceRegister,			/* Register in address property	*/
	LogicalAddress		*deviceLogicalAddress,		/* Gets the logical address	*/
	ByteCount		*deviceAreaLength )		/* Gets the area size		*/
{
	OSStatus		status;
	UInt32			i;				/* Vector index					*/
	UInt32			nAddresses;			/* Number of vector elements	*/
	RegPropertyValueSize	assignedAddressSize;
	RegPropertyValueSize	logicalAddressSize;
	RegPropertyValue	*assignedAddressProperty; 	/* Assigned Address property	*/
	PCIAssignedAddressPtr	pciAssignedAddressPtr;		/* Assigned Address element ptr	*/
	LogicalAddress		*logicalAddressVector;		/* AAPL,addresses property	*/
	StringPtr		failureMsg;
		
	Trace(GetDeviceLogicalAddress);
		
	failureMsg		= NULL;
	assignedAddressProperty	= NULL;
	logicalAddressVector	= NULL;
	
	if (deviceLogicalAddress == NULL || deviceAreaLength == NULL)
		status = paramErr;
	else {
		/*
		 * Fetch the assigned address and AAPL,address properties. Allocate memory
		 * for each.
		 */
		status = GetThisProperty( regEntryIDPtr,
					  kPCIAssignedAddressProperty,
					  &assignedAddressProperty,
					  &assignedAddressSize);
		CheckStatus(status, "No assigned-addresses property");
	}
	if (status == noErr) {
		status = GetThisProperty(
					regEntryIDPtr,
					kAAPLDeviceLogicalAddress,
					&logicalAddressVector,
					&logicalAddressSize
				);
		CheckStatus(status, "No AAPL,address property");
	}
	if (status == noErr) {
		status = paramErr;
		nAddresses = assignedAddressSize / sizeof (PCIAssignedAddress);
		pciAssignedAddressPtr = (PCIAssignedAddressPtr) assignedAddressProperty;
		
		DBG(lprintf("nAddresses = %d\n", nAddresses ));
		for (i=0; i<logicalAddressSize/4; i++)
			DBG( lprintf("AAPL,address[%d] = %08lx\n", i, logicalAddressVector[i]) );
			
		for (i = 0; i < nAddresses; i++, pciAssignedAddressPtr++) {
			if (GetPCIRegisterNumber(pciAssignedAddressPtr) == deviceRegister) {
				DBG( lprintf("Matched phys: %08lx log: %08lx to reg: %x\n",
					     pciAssignedAddressPtr->address.lo, logicalAddressVector[i], deviceRegister) );
				
				if (pciAssignedAddressPtr->size.hi != 0		/* 64-bit area? */
				    || pciAssignedAddressPtr->size.lo == 0) {	/* Zero length */
					/*
					 * Open Firmware was unable to assign a valid address to this
					 * memory area. We must return an error to prevent the driver
					 * from starting up. Is there a better error status?
					 */
					status = paramErr;
				} else if (i >= (logicalAddressSize / sizeof (LogicalAddress))) {
					/*
					 * The logical address vector is too small -- this is a bug.
					 */
					status = paramErr;
				}
				else {
					status = noErr;
					*deviceLogicalAddress = logicalAddressVector[i];
					*deviceAreaLength = pciAssignedAddressPtr->size.lo;
					DBG( lprintf("logical address vector %d\n",i ) );
				}
				break; /* Exit loop when we find the desired register */
			}
		}
		CheckStatus(status, "No valid address space");
	}
#ifndef OS_X
	/* Will cause problem in OSX. Probably a bug in the NDRV glue (?) */
	DisposeThisProperty(assignedAddressProperty);
	DisposeThisProperty(logicalAddressVector);
#endif
	if (status != noErr)
		PublishInitFailureMsg(status, failureMsg);
	return (status);
}

