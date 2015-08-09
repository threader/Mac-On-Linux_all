/* 
 *   Creation Date: <1999/05/01 02:27:39 samuel>
 *   Time-stamp: <2003/07/09 17:23:15 samuel>
 *   
 *	<scsi_main.c>
 *	
 *	Centralized SCSI initialization
 *   
 *   Copyright (C) 1999, 2001, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include "booter.h"
#include "scsi_main.h"
#include "scsi-bus.h"
#include "scsi-unit.h"
#include "driver_mgr.h"
#include "partition_table.h"

static int	init_scsi( void );
static void	cleanup_scsi( void );
static pdisk_t	*p_create_dummy_disk( void );

driver_interface_t old_scsi_driver = { 
	"scsi", init_scsi, cleanup_scsi 
};

static pdisk_t *dummy_disk = NULL;


/*
 * MacOS won't boot (in the oldworld setting) if the 53c94 or mesh chip
 * is not present. Moreover, there will usually be a spin-up delay (30-40 s)
 * if MacOS does not find a SCSI unit with ID 0. To circumvent these problems,
 * we construct an ID 0 unit (just consisting of two blocks).
 *
 * ============================================================================
 * 	EVERYTHING IN THIS NEIGHBORHOOD IS JUST HERE TO KEEP MAC OS HAPPY -
 *
 * 		   REAL DISK ACCESS HAPPENS ELSEWHERE 
 * ============================================================================
 */

static int
init_scsi( void )
{
	if( !is_oldworld_boot() )
		return 0;

	dummy_disk = p_create_dummy_disk();

	scsi_bus_init();
	register_scsi_disk( dummy_disk, 0, 0);
	return 1;
}

static void
cleanup_scsi( void )
{
	scsi_disks_cleanup();
	scsi_bus_cleanup();
	
	free( dummy_disk );
}


/************************************************************************/
/*	Dummy disk							*/
/************************************************************************/

#define DISK_SIZE	(512*2)
#define SIZE_MASK	(DISK_SIZE-1)

struct pdisk {
	char	buf[ DISK_SIZE ];
	int	pos;
};

static pdisk_t *
p_create_dummy_disk( void )
{
	part_entry_t 	*pm;
	desc_map_t 	*desc;
	pdisk_t		*d = malloc( sizeof(pdisk_t) );

	memset( d, 0, sizeof(pdisk_t) );

	pm = (part_entry_t*)(&d->buf[512]);
	desc = (desc_map_t*)&d->buf;

	desc->sbSig = 0x4552;
	desc->sbBlockSize = 512;
	desc->sbBlkCount = 2;

	pm->pmSig = 0x504d;
	pm->pmMapBlkCnt = 1;
	pm->pmDataCnt = 1;
	pm->pmPyPartStart = 1;
	pm->pmPartBlkCnt = 1;
	strcpy( pm->pmPartName, "Apple" );
	strcpy( pm->pmPartType, "Apple_partition_map" );
	pm->pmPartStatus = 0x37; /* or? */

	return d;
}

ulong	p_get_num_blocks( pdisk_t *d )	{ return 2; }
int	p_get_sector_size( pdisk_t *d ) { return 512; }
	
ssize_t
p_read( pdisk_t *d, void *buf, size_t count )
{
	if( count + d->pos > DISK_SIZE )
		count = DISK_SIZE - d->pos;

	memcpy( buf, d->buf + d->pos, count );

	d->pos = (d->pos + count) & SIZE_MASK;
	return count;
}

ssize_t
p_write( pdisk_t *d, const void *buf, size_t count )
{
	d->pos = (d->pos + count) & SIZE_MASK;
	return count;
}

int
p_lseek( pdisk_t *d, long block, long offs )
{
	d->pos = (block * 512 + offs) & SIZE_MASK;
	return 0;
}
