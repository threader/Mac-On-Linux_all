/* 
 *   Creation Date: <2001/05/12 17:39:24 samuel>
 *   Time-stamp: <2004/03/24 01:34:29 samuel>
 *   
 *	<blkdev.c>
 *	
 *	Determine which block devices to export.
 *	The disks are claimed by lower level drivers.
 *   
 *   Copyright (C) 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include "byteorder.h"
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/uio.h>
#include <sys/ioctl.h>

#include "res_manager.h"
#include "partition_table.h"
#include "hfs_mdb.h"
#include "disk.h"
#include "llseek.h"
#include "booter.h"
#include "driver_mgr.h"

/* Disk Type Includes */
#include "blk_raw.h"
#include "blk_qcow.h"
#include "blk_dmg.h"

#define BLKFLSBUF  _IO(0x12,97)		/* from <linux/fs.h> */

/* Volumes smaller than this must be specified explicitely
 * (and not through for instance 'blkdev: /dev/hda -rw').
 * The purpose of this is preventing any boot-strap partitions
 * from beeing mounted (and hence deblessed) in MacOS.
 */
#define BOOTSTRAP_SIZE_KB	20 * 1024

static opt_entry_t blk_opts[] = {
	{"-ro",			0 },
	{"-rw",			BF_ENABLE_WRITE },
	{"-force",		BF_FORCE },
	{"-cdrom",		BF_CD_ROM | BF_WHOLE_DISK | BF_REMOVABLE },
	{"-cd",			BF_CD_ROM | BF_WHOLE_DISK | BF_REMOVABLE },
	{"-cdboot",		BF_BOOT1 | BF_BOOT },
	{"-boot",		BF_BOOT },
	{"-boot1",		BF_BOOT1 | BF_BOOT },
	{"-whole",		BF_WHOLE_DISK },
	{"-removable",		BF_REMOVABLE },
	{"-drvdisk",		BF_DRV_DISK | BF_REMOVABLE },
	{"-ignore",		BF_IGNORE },
	{NULL, 0 }
};

enum { kMacVolumes, kLinuxDisks };

#define kDiskTypeUnknown	(1<<0)
#define kDiskTypeHFSPlus	(1<<1)
#define kDiskTypeHFS		(1<<2)
#define kDiskTypeHFSX		(1<<3)
#define kDiskTypeUFS		(1<<4)
#define kDiskTypePartitioned	(1<<5)
#define kDiskTypeRaw		(1<<6)
#define kDiskTypeQCow		(1<<7)
#define kDiskTypeDMG		(1<<8)


static bdev_desc_t 		*s_all_disks;


/************************************************************************/
/*	misc								*/
/************************************************************************/

static int
find_disk_type(int fd, char *name, bdev_desc_t *bdev)
{
	char buf[512];
	/* Always open as a RAW disk to ensure that read/write/seek are valid */
	raw_open(fd, bdev);	

	/* If we can't read 512 bytes, give up */
	if ( pread(fd, buf, 512, 0) != 512 ) 
		return kDiskTypeUnknown;

	/* Force handling driver disks as RAW disks */
	if (!strncmp(name, "images/moldisk.dmg", 18) ||
	    !strncmp(name, "images/moldiskX.dmg", 19)) 
		return kDiskTypeRaw;
	
	/* Check for QCow Disks */
	if ( QCOW_MAGIC == be32_to_cpu(((QCowHeader *)buf)->magic) ) {
		qcow_open(fd, bdev);
		return kDiskTypeQCow;
	}

	/* Check for dmg disks */
	if (strlen(name) > 4 && !strncmp(name + strlen(name) - 4, ".dmg", 4)) {
		dmg_open(fd, bdev);
		return kDiskTypeDMG;		
	}

	/* Otherwise, guess it's a raw disk */
	return kDiskTypeRaw;
}

/* volname is allocated */
static int
inspect_disk( bdev_desc_t *bdev, char **typestr, char **volname, int type)
{
	struct iovec vec;
	char buf[512];
	vec.iov_base = buf;
	vec.iov_len = 512;
	desc_map_t *dmap = (desc_map_t*)buf;
	hfs_mdb_t *mdb = (hfs_mdb_t*)buf;
	int signature;

	*typestr = "Disk";
	*volname = NULL;	

	if (type == kDiskTypeUnknown) {
		*volname = strdup("- Unknown -");
		return kDiskTypeUnknown;
	}

	/* Read the first 512 bytes of the disk for identification */
	bdev->seek(bdev, 0, 0);
	if( bdev->read(bdev, &vec, 1) != 512 ) 
		return kDiskTypeUnknown;

	if( dmap->sbSig == DESC_MAP_SIGNATURE ) {
		*volname = strdup("- Partioned -");
		return kDiskTypePartitioned;
	}

	bdev->seek(bdev, 2, 0);
	if( bdev->read(bdev, &vec, 1) != 512 ) 
		return kDiskTypeUnknown;

	signature = hfs_get_ushort(mdb->drSigWord);

	if( signature == HFS_PLUS_SIGNATURE ) {
		*typestr = "Unembedded HFS+";
		return kDiskTypeHFSPlus;
	}

	if( signature == HFSX_SIGNATURE ) {
		*typestr = "HFSX";
		return kDiskTypeHFSX;
	}

	if( signature == HFS_SIGNATURE ) {
		char vname[256];
		memcpy( vname, &mdb->drVN[1], mdb->drVN[0] );
		vname[mdb->drVN[0]] = 0;
		*volname = strdup( vname );

		if( hfs_get_ushort(mdb->drEmbedSigWord) == HFS_PLUS_SIGNATURE ) {
			*typestr = "HFS+";
			return kDiskTypeHFSPlus;
		}
		*typestr = "HFS";
		return kDiskTypeHFS;
	}
	return kDiskTypeUnknown;
}

/************************************************************************/
/*	register disk							*/
/************************************************************************/

static void
report_disk_export( bdev_desc_t *bd, const char *type )
{
	char buf[80];

	if( bd->flags & BF_DRV_DISK )
		return;

	strnpad( buf, bd->dev_name, 17 );
	if( buf[15] != ' ')
		buf[14] = buf[15] = '.';
	strncat3( buf, " ", bd->vol_name ? bd->vol_name : "", sizeof(buf));
	strnpad( buf, buf, 32 );

	printm("    %-5s %s <r%s ", type, buf, (bd->flags & BF_ENABLE_WRITE)? "w>" : "ead-only> ");
	
	if( bd->size )
		printm("%4ld MB ", (long)(bd->size >> 20) );
	else	
		printm(" ------ ");

	printm("%s%s\n", (bd->flags & BF_BOOT)? "BOOT" : "",
			 (bd->flags & BF_BOOT1)? "1" : "" );
}

static void
register_disk(bdev_desc_t *bdev, const char *typestr, const char *name,
	      const char *vol_name, int fd, long flags, ulong blocks )
{
	if(NULL == bdev) {
		bdev = malloc( sizeof(bdev_desc_t) );
		CLEAR( *bdev );
		/* If bdev isn't allocated already, open as raw disk */
		raw_open(fd, bdev);
	}

	bdev->dev_name = strdup(name);
	bdev->vol_name = vol_name ? strdup(vol_name) : NULL;

	bdev->fd = fd;
	bdev->flags |= flags;
	bdev->size = 512ULL * blocks;

	bdev->priv_next = s_all_disks;
	s_all_disks = bdev;

	report_disk_export( bdev, typestr );
}


/************************************************************************/
/*     	mac disk support						*/
/************************************************************************/

static int
open_mac_disk( char *name, int flags, int constructed )
{
	bdev_desc_t *bdev;
	int type, fd, ro_fallback, ret=0;
	char *volname, *typestr;
	
	if( (fd=disk_open(name, flags, &ro_fallback, constructed)) < 0 )
		return -1;

	/* Alloc the block device */
	bdev = (bdev_desc_t *)malloc( sizeof(bdev_desc_t) );
	if(!bdev) {
		close(fd);
		return -1;
	}
	CLEAR( *bdev );

	/* Finds disk type and performs init for bdev pointers */
	type = find_disk_type(fd, name, bdev);
	
	/* Inspect the disk to ensure it's valid */
	type |= inspect_disk(bdev, &typestr, &volname, type);
	
	if( ro_fallback )
		flags &= ~BF_ENABLE_WRITE;

	if( type & (kDiskTypeHFSPlus | kDiskTypeHFSX | kDiskTypeHFS | kDiskTypeUFS) ) {
		/* Standard Mac Volumes, nothing to do  */
	} 
	else if( type & kDiskTypePartitioned ) {
		if( constructed )
			ret = -1;
	}
	else {
		/* unknown disk type */
		if( !(flags & BF_FORCE) ) {
			if( !constructed )
				printm("----> %s is not a HFS disk (use -force to override)\n", name );
			ret = -1;
		}
	}

	/* detect boot-strap partitions by the small size */
	if( (flags & BF_ENABLE_WRITE) && (type & (kDiskTypeHFSPlus | kDiskTypeHFS)) ) {

		if( bdev->size/1024 < BOOTSTRAP_SIZE_KB && !(flags & BF_FORCE) ) {
			printm("----> %s might be a boot-strap partition.\n", name );
			ret = -1;
		}
	}

	if( !ret ) {
		/* Register the disk with MOL */
		register_disk( bdev, typestr, name, volname, bdev->fd, flags, bdev->size / 512ULL );
	}
	else {
		/* Close the block device */
		close( bdev->fd );
		/* Free the block dev struct */
		free(bdev);
	}

	if( volname )
		free( volname );
	return ret;
}

static void
setup_mac_disk( char *name, int flags )
{
	static char *dev_list[] = { "/dev/sd", "/dev/hd", NULL };
	char buf[32], **pp;
	int i, n;
	
	/* Check for entire device (/dev/hda) and substitute with /dev/hdaX */
	for( pp=dev_list; !(flags & BF_WHOLE_DISK) && *pp; pp++ ) {
		if( strncmp(*pp, name, strlen(*pp)) || strlen(name) != strlen(*pp)+1 )
			continue;

		flags |= BF_SUBDEVICE;
		flags &= ~BF_FORCE;
		for( n=0, i=1; i<32; i++ ) {
			sprintf( buf, "%s%d", name, i );
			n += !open_mac_disk( buf, flags, 1 );
		}
		if( !n )
			printm("No volumes found in '%s'\n", name );
		return;
	}
	open_mac_disk( name, flags, 0 );
}


static void
setup_linux_disk( char *name, int flags )
{
	int fd, ro_fallback=0;
	printm("FIXME: non RAW disks won't work with Linux yet!\n");
	if( (fd=disk_open( name, flags, &ro_fallback, 0 )) >= 0 ) {
		if( ro_fallback )
			flags &= ~BF_ENABLE_WRITE;
		register_disk(NULL, "Disk", name, NULL, fd, flags, get_file_size_in_blks(fd) );
	}
}
	
/************************************************************************/
/*	setup disks							*/
/************************************************************************/

static void
setup_disks( char *res_name, int type )
{
	int fd, i, boot1, flags, ro_fallback=0;
	bdev_desc_t *bdev;
	char *name;
	
	for( i=0; (name=get_filename_res_ind( res_name, i, 0)) ; i++ ) {
		flags = parse_res_options( res_name, i, 1, blk_opts, "Unknown disk flag");

		if( flags & BF_IGNORE )
			continue;

		/* handle CD-roms */
		if( flags & BF_CD_ROM ) {
			flags &= ~BF_ENABLE_WRITE;
			if( (fd=disk_open( name, flags, &ro_fallback, 0 )) < 0 )
				continue;
			register_disk(NULL, "CD", name, "CD/DVD", fd, flags, 0 );
			continue;
		}

		switch( type ) {
		case kMacVolumes:
			setup_mac_disk( name, flags );
			break;

		case kLinuxDisks:
			setup_linux_disk( name, flags );
			break;
		}
	}

	/* handle boot override flag */
	for( bdev=s_all_disks; bdev && !(bdev->flags & BF_BOOT1); bdev=bdev->priv_next )
		;
	boot1 = bdev ? 1:0;
	for( bdev=s_all_disks; bdev && boot1; bdev=bdev->priv_next )
		if( !(bdev->flags & BF_BOOT1) )
			bdev->flags &= ~BF_BOOT;
}

/************************************************************************/
/*	Global interface						*/
/************************************************************************/

bdev_desc_t *
bdev_get_next_volume( bdev_desc_t *last )
{
	bdev_desc_t *bdev = last ? last->priv_next : s_all_disks;
	for( ; bdev && bdev->priv_claimed; bdev=bdev->priv_next )
		;
	return bdev;
}

void
bdev_claim_volume( bdev_desc_t *bdev )
{
	bdev_desc_t *p;
	for( p=s_all_disks; p && p != bdev; p=p->priv_next )
		;
	p->priv_claimed = 1;
}

void
bdev_close_volume( bdev_desc_t *dev )
{
	bdev_desc_t **bd;

	for( bd=&s_all_disks; *bd; bd=&(**bd).priv_next ){
		if( *bd != dev )
			continue;

		/* unlink */
		*bd = (**bd).priv_next;

		free( dev->dev_name );
		if( dev->vol_name )
			free( dev->vol_name );

		if( dev->close != NULL)
			dev->close(dev);
		
		if( dev->fd != -1 ) {
			/* XXX: the ioctl should only be done for block devices! */
			/* ioctl( dev->fd, BLKFLSBUF ); */
			close( dev->fd );
		}
		free( dev );
		return;
	}
	printm("bdev_close_volume: dev is not in the list!\n");
}

static int
blkdev_init( void )
{
	printm("\nAvailable Disks:\n");
	if( is_classic_boot() || is_osx_boot() ) {
		setup_disks("blkdev_mol", kMacVolumes );
		setup_disks("blkdev", kMacVolumes );
	}
	if( is_linux_boot() )
		setup_disks("blkdev", kLinuxDisks );
	printm("\n");
	return 1;
}

static void
blkdev_cleanup( void )
{
	bdev_desc_t *dev;

	for( dev=s_all_disks; dev; dev=dev->priv_next ) {
		if( dev->priv_claimed ){
			printm("Claimed disk not released properly!\n");
			continue;
		}

		/* Unclaimed disk */
		printm("Unclaimed disk '%s' released\n", dev->dev_name );
		bdev_close_volume( dev );
		blkdev_cleanup();
		break;
	}
}

driver_interface_t blkdev_setup = {
	"blkdev", blkdev_init, blkdev_cleanup
};
