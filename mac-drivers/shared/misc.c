/* 
 *   Creation Date: <2002/11/23 18:17:20 samuel>
 *   Time-stamp: <2002/11/23 18:27:30 samuel>
 *   
 *	<misc.c>
 *	
 *	
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include <DriverServices.h>
#include "misc.h"
#include "logger.h"

/* This function returs the physical address of a resident buffer.
 * Note that this is not particularly useful unless the physcial 
 * memory is contiguous. (There probably is a simpler way to do this...)
 */
UInt32
GetPhysicalAddr( void *logical_addr )
{
	IOPreparationTable prept;
	UInt32 phys_addr;

	BlockZero(&prept, sizeof(prept));
	prept.options =	( (0 * kIOIsInput)
			| (0 * kIOIsOutput)
			| (0 * kIOMultipleRanges)	/* no scatter-gather list	*/
			| (1 * kIOLogicalRanges)	/* Logical addresses		*/
			| (0 * kIOMinimalLogicalMapping)/* Normal logical mapping	*/
			| (1 * kIOShareMappingTables)	/* Share with Kernel		*/
			| (1 * kIOCoherentDataPath)	/* Definitely!			*/
			);
	prept.addressSpace = kCurrentAddressSpaceID;
	prept.firstPrepared = 0;
	prept.logicalMapping = NULL;
	prept.rangeInfo.range.base = logical_addr;
	prept.rangeInfo.range.length = 1;
	prept.physicalMapping = (void**)&phys_addr;
	prept.mappingEntryCount = 1;

	if( PrepareMemoryForIO(&prept) != noErr )
		return NULL;
		
	/* release... we only needed the physical address */
	CheckpointIO( prept.preparationID, kNilOptions );

	return phys_addr;
}
