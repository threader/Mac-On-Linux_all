/* 
 *   Creation Date: <2004/03/21 23:45:39 samuel>
 *   Time-stamp: <2004/03/25 17:20:33 samuel>
 *   
 *	<cd-scsi.c>
 *	
 *	SCSI backend (through CDROM_SEND_PACKET)
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */

#include "mol_config.h"
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include <sys/param.h>

#include "thread.h"
#include "async.h"
#include "poll_compat.h"
#include "memory.h"
#include "booter.h"
#include "res_manager.h"
#include "disk.h"
#include "scsi-client.h"

typedef struct idev {
	bdev_desc_t		*bdev;
	scsi_dev_t		*scsi_dev;
	int			fd;		/* copy of bdev->fd */
	struct idev		*next;

	pthread_mutex_t		lock;		/* queue lock */
	volatile int		running;
	scsi_ureq_t		*rqueue;
	scsi_ureq_t		*done_queue;
} idev_t;

static struct {
	idev_t			*devs;
	int			completion_aev;
} x;


#define LOCK(idev)		pthread_mutex_lock( &idev->lock );
#define UNLOCK(idev)		pthread_mutex_unlock( &idev->lock );


/************************************************************************/
/*	I/O thread							*/
/************************************************************************/

static void
do_execute( scsi_ureq_t *u, idev_t *idev )
{
	char *buffer = 0;
	struct cdrom_generic_command cgc;
	scsi_ureq_t **qdp;
	
	u->scsi_status = SCSI_STATUS_CHECK_CONDITION;

	if( u->size > 0 && !(buffer=malloc(u->size)) )
		goto out;

	/* linearize the S&G buffer */
	if( u->is_write ) {
		char *p = buffer;
		int i, ss, s=u->size;
		for( i=0 ; s > 0 ; i++, p+=ss, s-=ss ) {
			ss = MIN( s, u->iovec[i].iov_len );
			memcpy( p, u->iovec[i].iov_base, ss );
		}
	}

	memset( &cgc, 0, sizeof(cgc) );
	cgc.sense = (struct request_sense*)&u->sb;
	cgc.data_direction = u->is_write ? CGC_DATA_WRITE :
		u->size ? CGC_DATA_READ : CGC_DATA_NONE;
	cgc.buffer = (unsigned char *) buffer;
	cgc.buflen = u->size;
	cgc.quiet = 1;
	cgc.timeout = 30000;
	memcpy( cgc.cmd, u->cdb, 12 );

	if( ioctl(idev->fd, CDROM_SEND_PACKET, &cgc) < 0 || cgc.stat ) {
		struct request_sense *sense = (struct request_sense*)&u->sb;
		if( sense->sense_key ) {
			u->scsi_status = sense->sense_key;
			u->sb_actlen = sense->add_sense_len + 8;
			if( u->sb_actlen > sizeof(u->sb) )
				u->sb_actlen = sizeof(u->sb);
		}
		goto out;
	}
	u->act_size = u->size - cgc.buflen;

	/* copy liner buffer back to S&G */
	if( !u->is_write && u->size ) {
		char *p = buffer;
		int i, ss, s=u->size;
		for( i=0 ; s > 0 ; i++, p+=ss, s-=ss ) {
			ss = MIN( s, u->iovec[i].iov_len );
			memcpy( u->iovec[i].iov_base, p, ss );
		}
	}
	u->scsi_status = SCSI_STATUS_GOOD;
	u->sb_actlen = 0;
	
out:
	if( buffer )
		free( buffer );

	/* complete request from main thread */
	LOCK( idev );
	u->next = NULL;
	for( qdp=&idev->done_queue; *qdp; qdp=&(**qdp).next )
		;
	*qdp = u;
	UNLOCK( idev );

	send_aevent( x.completion_aev );
}

static void
workloop( void *x )
{
	idev_t *idev = (idev_t*)x;
	scsi_ureq_t *u;

	for( ;; ) {
		LOCK( idev );
		if( !(u=idev->rqueue) )
			break;
		idev->rqueue = u->next;
		UNLOCK( idev );

		do_execute( u, idev );
	}

	idev->running = 0;
	UNLOCK( idev );
}


/************************************************************************/
/*	main thread							*/
/************************************************************************/

static void
complete( int dummy_aevtoken )
{
	scsi_ureq_t *u;
	idev_t *idev;
	
	for( idev=x.devs; idev ; idev=idev->next ) {
		LOCK( idev );

		while( (u=idev->done_queue) ) {
			idev->done_queue = u->next;
			complete_scsi_req( idev->scsi_dev, u );
		}
		UNLOCK( idev );
	}
}

static void
execute( scsi_ureq_t *u, void *refcon )
{
	idev_t *idev = (idev_t*)refcon;	
	int start_thread = 0;
	scsi_ureq_t **qp;
	
	u->next = NULL;

	LOCK( idev );
	for( qp=&idev->rqueue; *qp ; qp=&(**qp).next )
		;
	*qp = u;
	if( !idev->running ) {
		idev->running = 1;
		start_thread = 1;
	}
	UNLOCK( idev );

	if( start_thread )
		create_thread( workloop, idev, "SCSI" );
}

static scsi_ops_t scsi_ops = {
	.execute	= execute,
};

static void
add_device( bdev_desc_t *bdev )
{
	idev_t *idev;
	int fd = bdev->fd;
	
	if( !(idev=malloc(sizeof(*idev))) )
		return;
	memset( idev, 0, sizeof(*idev) );
	idev->bdev = bdev;
	idev->fd = fd;
	idev->scsi_dev = register_scsidev( &scsi_ops, 0, idev );

	pthread_mutex_init( &idev->lock, NULL );
	idev->next = x.devs;
	x.devs = idev;
}

void
cd_scsi_init( void )
{
	bdev_desc_t *bdev = NULL;

	if( !get_bool_res("generic_scsi_for_cds") ) {
		printm("    Generic SCSI for CD/DVDs disabled\n");
		return;
	}
	if( is_classic_boot() && get_str_res("cdboot") ) {
		printm("    CD-SCSI disabled (CD-booting)\n");
		return;
	}
	
	while( (bdev=bdev_get_next_volume(bdev)) ) {
		int flags;

		if( !(bdev->flags & BF_CD_ROM) )
			continue;

		flags = ioctl( bdev->fd, CDROM_GET_CAPABILITY );
		if( flags == -1 || !(flags & CDC_GENERIC_PACKET) )
			continue;

		bdev_claim_volume( bdev );
		add_device( bdev );

		printm("    SCSI  %-16s [CDROM/DVD driver]\n", bdev->dev_name );
	}
	x.completion_aev = add_aevent_handler( complete );
}

void
cd_scsi_cleanup( void )
{
	idev_t *idev;
	struct timespec tv;
	tv.tv_sec = 0;
	tv.tv_nsec = 1000000;

	while( (idev=x.devs) ) {
		x.devs = idev->next;

		if( idev->running )
			printm("Waiting upon SCSI completion\n");
		while( idev->running )
			nanosleep(&tv, NULL);
		
		pthread_mutex_destroy( &idev->lock );
		unregister_scsidev( idev->scsi_dev );

		bdev_close_volume( idev->bdev );
		free( idev );
	}
}
