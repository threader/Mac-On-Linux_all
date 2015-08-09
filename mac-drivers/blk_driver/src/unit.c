/* 
 *   Creation Date: <2002/12/28 17:50:06 samuel>
 *   Time-stamp: <2003/03/09 14:43:10 samuel>
 *   
 *	<unit.c>
 *	
 *	Handle insertion / removal of units
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include <DriverServices.h>
#include <Disks.h>
#include "logger.h"
#include "unit.h"
#include "util.h"
#include "LinuxOSI.h"

static unit_t *
setup_unit_record( channel_t *ch, int unit, int nblks, int startblk, int parnum )
{
	unit_t *d, *first = get_first_unit(ch);
	ablk_disk_info_t info;
	int chnum = get_channel_num( ch );
	ulong flags;
	
	OSI_ABlkDiskInfo( chnum, unit, &info );

	/* reuse old slots */
	for( d=first; d; d=d->next )
		if( d->unit == unit && d->no_disk )
			break;

	/* allocate a new slot if necessary */
	if( !d ) {
		if( !(d=PoolAllocateResident( sizeof(*d), TRUE )) )
			return NULL;

		d->no_disk = 1;
		d->drv_num = -1;
		d->unit = unit;
		d->channel = chnum;
		d->ch = ch;
		add_unit( ch, d );
	}
	d->mounted = 0;

	d->first_blk = startblk;
	d->parnum = parnum;

	/* might be a partition */
	d->info = info;
	d->info.nblks = nblks;

	/* add unit to MacOS driver queue */
	flags = 0;
	if( !(info.flags & ABLK_INFO_REMOVABLE) )
		flags = 0x00080000;	/* not ejectable */
	
	if( (info.flags & ABLK_INFO_READ_ONLY) )
		flags |= 0x80000000; 	/* locked */

	/* add driver queue element */
	if( d->drv_num < 0 ) {
		if( AddDriveToQueue( flags, nblks, GLOBAL.refNum, &d->drv_num ))
			return NULL;
	} else {
		UpdateDriveQueue( flags, nblks, GLOBAL.refNum, d->drv_num );
	}
	d->no_disk = 0;
	return d;
}

void
eject_unit_records( channel_t *ch, int unit )
{
	unit_t *d, *first = get_first_unit(ch);
	
	/* eject all partitions in unit */
	for( d=first; d; d=d->next ) {
		if( d->unit == unit && !d->no_disk ) {
			d->no_disk = 1;
			d->mounted = 0;
		}
	}
}

void
register_units( channel_t *ch, int unit ) 
{
	int parnum, chnum = get_channel_num(ch);
	ablk_disk_info_t info;
	unit_t *d;

	OSI_ABlkDiskInfo( chnum, unit, &info );
	
	for( parnum=0 ;; parnum++ ) {
		int startblk, nblks, type;

		nblks = info.nblks;
		if( find_partition( chnum, unit, parnum, &startblk, &nblks, &type ) < 0 ) {
			if( !parnum )
				continue;
			break;
		}
		/* lprintf("Partion [%d] found: %d %d type: %d\n", parnum, startblk, nblks, type ); */
		if( type != kPartitionTypeHFS )
			continue;
			
		/* get/allocate unit record (and add drive to queue) */
		d = setup_unit_record( ch, unit, nblks, startblk, parnum );
	}
	/* lprintf("Unit %d (drv_num %d), size %x\n", i, drv_num, unit->info.nblks ); */

	/* remove stale unit records (might happen if the previous disk had multiple partitions) */
	for( d=get_first_unit(ch); d; d=d->next ) {
		if( d->unit != unit || !d->no_disk || d->drv_num == -1 )
			continue;
		if( RemoveDriveFromQueue( d->drv_num ) )
			lprintf("RemoveDriveFromQueue failed\n" );

		d->drv_num = -1;
	}
	
	/* post disk inserted events */
	for( d=get_first_unit(ch); d; d=d->next )
		if( d->unit == unit && !d->no_disk )
			PostEvent( diskEvt, d->drv_num );
}

int
setup_units( channel_t *ch, int boot )
{
	ablk_disk_info_t info;
	int i, chnum;

	chnum = get_channel_num(ch);

	for( i=0 ; !OSI_ABlkDiskInfo(chnum, i, &info) ; i++ ) {
		if( !!(info.flags & ABLK_BOOT_HINT) != boot && boot != -1 )
			continue;
		register_units( ch, i );
	}
	return 0;
}

