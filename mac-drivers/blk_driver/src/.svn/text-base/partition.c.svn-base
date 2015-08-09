/* 
 *   Creation Date: <2002/12/22 17:00:59 samuel>
 *   Time-stamp: <2002/12/30 21:56:25 samuel>
 *   
 *	<partition.c>
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


#include <AppleDiskPartitions.h>
#include <DriverServices.h>
#include "misc.h"
#include "util.h"
#include "logger.h"
#include "LinuxOSI.h"
#include "unit.h"

/* nblk is supposed to contain #blks on entry */
int
find_partition( int channel, int unit, int parnum, int *startblk, int *nblk, int *type ) 
{
	Block0 *dm;
	Partition *par;
	int sbSig, phys, bs, ret=-1;
	char *p;

	if( !(p=MemAllocatePhysicallyContiguous( 512, FALSE )) )
		return -1;
	phys = GetPhysicalAddr( p );

	dm = (Block0*) p;
	par = (Partition*) p;
	
	do {
		if( OSI_ABlkSyncRead( channel, unit, 0, phys, 512 ) )
			break;
		sbSig = dm->sbSig;

		if( OSI_ABlkSyncRead( channel, unit, 1, phys, 512 ) )
			break;
		
		/* certain CDs have a blank block0 before the partition table */
		bs = 0;
		if( (!sbSig || sbSig==sbSIGWord) && par->pmSig == pMapSIG ) {
			bs = 1;
		} else if( sbSig == sbSIGWord ) {
			bs = dm->sbBlkSize / 512;			
		}
		if( !bs ) {
			/* assume HFS if there is no partition table */
			*startblk = 0;
			*type = kPartitionTypeHFS;	
			ret = (!parnum)? 0 : -1;
			break;
		}
		/* partitioned disk, return error for index 0 (whole disk) */
		if( !parnum )
			break;
		
		OSI_ABlkSyncRead( channel, unit, bs, phys, 512 );
		if( parnum > par->pmMapBlkCnt || par->pmSig != pMapSIG /* 'PM' */ )
			break;
		OSI_ABlkSyncRead( channel, unit, parnum * bs, phys, 512 );

		*startblk = par->pmPyPartStart * bs;
		*nblk = par->pmPartBlkCnt * bs;

		*type = kPartitionTypeUnknown;
		if( !CStrCmp( (char*)par->pmParType, "Apple_HFS" ) )
			*type = kPartitionTypeHFS;
		else if( !CStrNCmp( (char*)par->pmParType, "Apple_Driver", 12 ) 
			 || !CStrCmp( (char*)par->pmParType, "Apple_Patches" ) 
			 || !CStrCmp( (char*)par->pmParType, "Apple_FWDriver" )
			 || !CStrCmp( (char*)&par->pmParType, "Apple_Free" )
			 || !CStrCmp( (char*)&par->pmParType, "Apple_partition_map" ))
			*type = kPartitionTypeDriver;
		
		/* lprintf("Partition @%d, len %d\n", *startblk, *nblk ); */
		ret = 0;

	} while(0);

	MemDeallocatePhysicallyContiguous( p );
	return ret;
}
