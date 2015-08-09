/* 
 *   Creation Date: <2002/12/22 12:58:13 samuel>
 *   Time-stamp: <2003/08/09 17:02:52 samuel>
 *   
 *	<ablk_cd.c>
 *	
 *	CD support
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include "disk.h"
#include "ablk_sh.h"
#include "timer.h"
#include "ablk.h"
#include "llseek.h"

static void	poll_for_cd( int id, void *usr, int latency );

static void
poll_for_nocd( int id, void *usr, int latency )
{
	ablk_device_t *d = (ablk_device_t*)usr;
	int ret;
	
	if( (ret=ioctl(d->ioctl_fd, CDROM_DRIVE_STATUS)) < 0 )
		return;

	if( ret == CDS_DISC_OK )
		usec_timer( 700000, poll_for_nocd, d );
	else 
		usec_timer( 700000, poll_for_cd, d );	
}

static void
cd_detection( ablk_device_t *d, int at_boot ) 
{
	bdev_desc_t *bdev = d->bdev;
	int ret;
	
	if( (ret=ioctl(d->ioctl_fd, CDROM_DRIVE_STATUS)) < 0 ) {
		perrorm("ioctl: CDROM_DRIVE_STATUS\n");
		return;
	}
	if( ret != CDS_DISC_OK ) {
		d->ablk_flags &= ~ABLK_INFO_MEDIA_PRESENT;
		usec_timer( 700000, poll_for_cd, d );
		return;
	}
	if( (ret=ioctl(d->ioctl_fd, CDROM_DISC_STATUS)) < 0 ) {
		perrorm("CDROM_DISC_STATUS:");
		usec_timer( 10000000, poll_for_cd, d );
		return;
	}
	switch( ret ) {
	case CDS_AUDIO:
	case CDS_XA_2_1:	/* photo-cd (among others) */
	case CDS_XA_2_2:	/* Can we treat it as a data-cd? */
		usec_timer( 700000, poll_for_nocd, d );
		return;
	}

	if( !at_boot )
		printm("--- CD Inserted ---\n");

	if( (bdev->fd=open(bdev->dev_name, O_RDONLY)) < 0 ) {
		perrorm("CD open");
		return;
	}
	bdev->size = 512ULL * get_file_size_in_blks( bdev->fd );
	d->ablk_flags |= ABLK_INFO_MEDIA_PRESENT;

	if( !at_boot )
		ablk_post_event( d, ABLK_EVENT_DISK_INSERTED );
}

static void
poll_for_cd( int id, void *usr, int latency )
{
	ablk_device_t *d = (ablk_device_t*)usr;
	cd_detection( d, 0 );
}

iofunc_t *
ablk_cd_request( ablk_device_t *d, int cmd, int param ) 
{
	bdev_desc_t *bdev = d->bdev;

	switch( cmd ) {
	case ABLK_EJECT_REQ: 		
		printm("--- CD Eject ---\n");
		if( bdev->fd != -1 ) {
			close( bdev->fd );
			bdev->fd = -1;
			ioctl( d->ioctl_fd, CDROMEJECT );
			cd_detection( d, 0 );
		}
		break;
	case ABLK_LOCKDOOR_REQ:
		ioctl( d->ioctl_fd, CDROM_LOCKDOOR, param );
		printm("kAblkLockDoor\n");
		break;
	default:
		printm("Unknown ablk control request\n");
		break;
	}
	return NULL;
} 

void
ablk_cd_registered( ablk_device_t *d )
{
	bdev_desc_t *bdev = d->bdev;

	d->ioctl_fd = bdev->fd;
	bdev->fd = -1;

	/* ioctl( d->ioctl_fd, CDROM_LOCKDOOR, 0 ); */
	cd_detection( d, 1 );
}

void
ablk_cd_cleanup( ablk_device_t *d )
{
	bdev_desc_t *bdev = d->bdev;

	if( bdev->fd != -1 )
		close( bdev->fd );
	bdev->fd = d->ioctl_fd;
}
