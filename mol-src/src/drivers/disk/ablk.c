/* 
 *   Creation Date: <2002/05/18 16:32:50 samuel>
 *   Time-stamp: <2004/03/24 00:29:42 samuel>
 *   
 *	<ablk.c>
 *	
 *	Async block driver
 *   
 *   Copyright (C) 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/uio.h>
#include <sys/resource.h>
#include <sys/param.h>
#include "prom.h"
#include "os_interface.h"
#include "disk.h"
#include "promif.h"
#include "disk.h"
#include "driver_mgr.h"
#include "memory.h"
#include "llseek.h"
#include "pic.h"
#include "mac_registers.h"
#include "thread.h"
#include "ablk_sh.h"
#include "timer.h"
#include "partition_table.h"
#include "pci.h"
#include "booter.h"
#include "osi_driver.h"
#include "ablk.h"


typedef struct {
	int		irq;
	int		ndevs;
	ablk_device_t	*devs;			/* array with ndevs elements */

	/* channel */
	int		running;		/* ready for action */
	int		active;			/* read thread is active */
	int		event;			/* an event condition is pending */
	int		ctrl_pipe[2];		/* thread control pipe */
	int		exiting;
	
	/* request ring */
	int		req_head;		/* index of head element in ring */
	int		ring_mask;		/* ring index mask */
	ablk_req_head_t	*ring;

	/* fields private to the i/o thread */
	int		n_requests;		/* #descriptors processed (used for completion) */
	int		cur_dev;		/* Current device */
	iofunc_t 	*iofunc;
} ablk_t;

static ablk_t		ablk;


void
ablk_post_event( ablk_device_t *d, int event )
{
	d->events |= event;
	ablk.event = 1;
	irq_line_hi( ablk.irq );
}

/************************************************************************/
/*	Engine								*/
/************************************************************************/

#define MAX_IOVEC	32

static int
dummy_io( bdev_desc_t *bdev, const struct iovec *vec, int n )
{
	int i, s;
	
	for( s=0, i=0; i<n; i++ )
		s += vec[i].iov_len;

	//printm("dummy_io: [%d] %d bytes\n", bdev->fd, s );
	return s;
}

static iofunc_t *
control_request( bdev_desc_t *bdev, int cmd, int param ) 
{
	int unit = bdev->fd;
	iofunc_t *ret = NULL;
	cntrlfunc_t *func = ablk.devs[unit].cntrl_func;

	switch( cmd ) {
	case ABLK_NOP_REQ:
		break;
	default:
		if( func )
			ret = (*func)( &ablk.devs[unit], cmd, param );
		break;
	}
	return ret ? ret : dummy_io;
} 

#define NEXT(ind)	(((ind)+1) & ablk.ring_mask)

static void
do_work( void )
{
	struct iovec vec[MAX_IOVEC];
	int n, count;

	union {
		void ** v;
		char ** c;
	} iovec_pun;

	n = count = 0;

	for( ;; ) {
		ablk_req_head_t *cur = &ablk.ring[ ablk.req_head ];
		int proceed = cur->proceed;
		int f, next = NEXT( ablk.req_head );
		ablk_req_head_t *r = &ablk.ring[next];

		/* execute request? */
		if( n && (!proceed || n == MAX_IOVEC || !(r->flags & ABLK_SG_BUF)) ) {
			int ret;

			while( (ret=(*ablk.iofunc)(ablk.devs[ablk.cur_dev].bdev, vec, n)) != count ) {
				int s, i;
				for( i=0; ret > 0; i++, ret -= s, count -= s ) {
					s = MIN( vec[i].iov_len, ret );
					vec[i].iov_len -= s;
					vec[i].iov_base += s;
				}
				if( ret < 0 && errno != EINTR ) {
					perrorm("ablk iofunc");
					goto error;
				}
			}
			ablk.n_requests += n;

			/* this takes into account the engine stall interrupt */
			if( (cur->flags & ABLK_RAISE_IRQ) && proceed )
				irq_line_hi( ablk.irq );
			n = 0;
			count = 0;
		}
		/* engine stall? */
		if( !proceed )
			break;

		/* flag old head slot for reuse */
		cur->flags = 0;

		/* advance pointer to next request */
		cur->proceed = 0;
		f = r->flags;
		ablk.req_head = next;

		/* process scatter & gather bufs */
		if( (f & ABLK_SG_BUF) ) {
			ablk_sg_t *p = (ablk_sg_t*)r;
			iovec_pun.v = &(vec[n].iov_base);
			if( mphys_to_lvptr( p->buf, (char**)iovec_pun.c ) ) {
				printm("ablk: bogus sg-buf");
				goto error;
			}
			vec[n++].iov_len = p->count;
			count += p->count;

			/* req_count is increased when the request is finished */
			continue;
		}

		/* must be a read/write/cntrol request */
		if( (unsigned int)r->unit >= ablk.ndevs ) {
			printm("ablk: bad unit");
			goto error;
		}
		ablk.cur_dev = r->unit;
		/* Read / Write Request */
		if( f & (ABLK_READ_REQ | ABLK_WRITE_REQ) ) {
			/* Schedule the next iofunc */
			ablk.iofunc = (f & ABLK_WRITE_REQ)? ablk.devs[r->unit].bdev->write : ablk.devs[r->unit].bdev->read;
			/* r->param contains first sector */
			if( ablk.devs[r->unit].bdev->seek( ablk.devs[r->unit].bdev, r->param, 0 ) < 0 ) {
				printm("ablk: bad lseek");
				goto error;
			}
		} else if( f & ABLK_CNTRL_REQ_MASK) {
			ablk.iofunc = control_request( ablk.devs[r->unit].bdev, (f & ABLK_CNTRL_REQ_MASK), r->param );
			if( f & ABLK_RAISE_IRQ )
				irq_line_hi( ablk.irq );
		} else {
			printm("bogus ablk command!\n");
			goto error;
		}
		/* n and count have already been set to zero */
		ablk.n_requests++;
	}

	/* engine stall */
	ablk.active = 0;
	irq_line_hi( ablk.irq );
	return;

 error:
	printm("ABlk engine error\n");
	ablk.running = 0;
	ablk.active = 0;

	irq_line_hi( ablk.irq );
}

static void
io_thread( void *dummy ) 
{
	char ch;
	/* setpriority( PRIO_PROCESS, getpid(), -19 ); */

	for( ;; ) {
		read( ablk.ctrl_pipe[0], &ch, 1 );

		if( ablk.exiting )
			break;
		do_work();
	}
}


/************************************************************************/
/*	OSI interface							*/
/************************************************************************/

/* kick( channel ) */
static int
osip_ablk_kick( int sel, int *params )
{
	int channel = params[0];
	char ch=1;
	
	if( channel )
		return -1;
	if( ablk.active )
		return 0;

	ablk.active = 1;
	write( ablk.ctrl_pipe[1], &ch, 1 );
	return 0;
}

/* ablk_irq_ack( channel, *request_count, *active, *event ) */
static int
osip_ablk_irq_ack( int sel, int *params )
{
	int channel = params[0];
	if( channel )
		return -1;

	if( !irq_line_low( ablk.irq ) )
		return 0;
	
	/* The client only needs to call kick if we return 0 in active; this 
	 * is atomic since the i/o thread clears the active field
	 * before the interrupt is raised.
	 */

	/* restart engine if there is work to do */
	if( !ablk.active && ablk.running && ablk.ring[ablk.req_head].proceed )
		osip_ablk_kick( sel, params );

	mregs->gpr[4] = ablk.n_requests;
	mregs->gpr[5] = ablk.active;
	mregs->gpr[6] = ablk.event;
	ablk.event = 0;

	return 1;
}

/* setup_ring( channel, mphys, n_el ) */
static int
osip_ablk_ring_setup( int sel, int *params )
{
	int channel = params[0];
	int mphys = params[1];
	int mask = params[2]-1;
	char *lvptr;

	if( channel )
		return -1;
	if( ablk.running || ((mask+1) & mask) || mphys_to_lvptr(mphys, &lvptr) )
		return -1;

	ablk.ring_mask = mask;
	ablk.ring = (ablk_req_head_t*)lvptr;
	return 0;
}

/* cntrl( int channel, int cmd ) */
static int
osip_ablk_cntrl( int sel, int *params )
{
	int channel=params[0], ret=0;
	uint unit;

	if( channel )
		return -1;

	switch( params[1] ) {
	case kABlkStart:
		if( ablk.running )
			break;

		ablk.cur_dev = -1;
		ablk.iofunc = dummy_io;
		if( ablk.ring ) {
			/* el. 1 is the first element to be processed */
			ablk.req_head = 0;
			ablk.ring[0].flags = ABLK_NOP_REQ;

			ablk.n_requests = 0;
			ablk.running = 1;
		} else
			ret = -1;
		break;

	default:
		printm("Bad ablk request\n");
		ret = -1;
		/* fall through */
	case kABlkReset:
		ablk.n_requests = 0;
		/* fall through */
	case kABlkStop:
		if( ablk.active ) {
			/* XXX: wait for thread to terminate */
		}
		ablk.running = 0;
		break;

	case kABlkRouteIRQ:
		oldworld_route_irq( params[2], &ablk.irq, "ablk" );
		break;
	case kABlkGetEvents:
		unit = params[2];
		if( unit >= ablk.ndevs )
			return -1;
		ret = ablk.devs[unit].events;
		ablk.devs[unit].events = 0;
		break;
	}
	if( !ablk.running )
		irq_line_low( ablk.irq );

	return ret;
}

/* disk_info( channel, index ). Info struct return in r4-r7 */
static int
osip_ablk_disk_info( int sel, int *params )
{
	int channel = params[0];
	uint index = params[1];
	ablk_device_t *d = &ablk.devs[index];
	ablk_disk_info_t info;

	if( channel )
		return -1;
	if( index >= ablk.ndevs )
		return -1;

	info.nblks = d->bdev->size / 512;	/* XXX fix sector size */
	info.flags = d->ablk_flags;
	info.res1 = info.res2 = 0;

	/* Return ablk_disk_info_t in r4-r5 */
	*(ablk_disk_info_t*)&mregs->gpr[4] = info;
	return 0;
}

/* bless_disk( channel, unit ) */
static int
osip_ablk_bless_disk( int sel, int *params )
{
	int i, channel=params[0];
	uint index = params[1];
	
	if( channel || index >= ablk.ndevs )
		return -1;

	for( i=0; i<ablk.ndevs; i++ )
		ablk.devs[i].ablk_flags &= ~ABLK_BOOT_HINT;
	ablk.devs[index].ablk_flags |= ABLK_BOOT_HINT;
	return 0;
}

/* sync_io( int channel, int unit, int blk, ulong mphys, int size ) */
static int
osip_ablk_sync_io( int sel, int *params )
{
	int cnt, channel=params[0], blk=params[2], size=params[4];
	ulong mphys = params[3];
	uint ind = params[1];
	ablk_device_t *d = &ablk.devs[ind];
	bdev_desc_t *bdev;
	char *buf;
	struct iovec vec;

	if( channel || ind >= ablk.ndevs || mphys_to_lvptr( mphys, &buf ) ) 
		return -1;

	bdev = d->bdev;

	if( bdev->fd < 0 || bdev->seek(bdev, blk, 0) < 0 ) 
		return -1;

	vec.iov_len = size;
	vec.iov_base = buf;
		
	cnt = (sel == OSI_ABLK_SYNC_WRITE) ?
		bdev->write(bdev, &vec, 1) : bdev->read(bdev, &vec, 1);

	return (cnt == size)? 0 : -1;
}

static const osi_iface_t osi_iface[] = {
	{ OSI_ABLK_RING_SETUP,	osip_ablk_ring_setup	},
	{ OSI_ABLK_CNTRL, 	osip_ablk_cntrl		},
	{ OSI_ABLK_DISK_INFO,	osip_ablk_disk_info	},
	{ OSI_ABLK_KICK,	osip_ablk_kick		},
	{ OSI_ABLK_IRQ_ACK,	osip_ablk_irq_ack	},
	{ OSI_ABLK_SYNC_READ,	osip_ablk_sync_io	},
	{ OSI_ABLK_SYNC_WRITE,	osip_ablk_sync_io	},
	{ OSI_ABLK_BLESS_DISK,	osip_ablk_bless_disk	},
};


/************************************************************************/
/*	misc								*/
/************************************************************************/

void
mount_driver_volume( void )
{
	uint i;
	
	for( i=0; i<ablk.ndevs; i++ ) {
		if( (ablk.devs[i].ablk_flags & ABLK_INFO_DRV_DISK) ) {
			ablk.devs[i].ablk_flags &= ~ABLK_INFO_DRV_DISK;
			ablk.devs[i].ablk_flags |= ABLK_INFO_MEDIA_PRESENT;
			ablk_post_event( &ablk.devs[i], ABLK_EVENT_DISK_INSERTED );
		}
	}
}

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

/* dn is NULL in the oldworld setting */
static void
add_drive( mol_device_node_t *dn, bdev_desc_t *bdev )
{
	ablk_device_t *d;
	int unit;

	d = realloc( ablk.devs, sizeof(ablk_device_t) * (ablk.ndevs+1) );
	fail_nil( d );
	
	ablk.devs = d;
	unit = ablk.ndevs++;
	d = &ablk.devs[unit];
	memset( d, 0, sizeof(*d) );
	
	d->bdev = bdev;
	d->ablk_flags = (bdev->flags & BF_ENABLE_WRITE)? 0 : ABLK_INFO_READ_ONLY;
	d->ablk_flags |= (bdev->flags & BF_REMOVABLE)? ABLK_INFO_REMOVABLE : 0;
	d->ablk_flags |= (bdev->flags & BF_CD_ROM)? ABLK_INFO_CDROM : 0;
	d->ablk_flags |= (bdev->flags & BF_BOOT)? ABLK_BOOT_HINT : 0;

	d->ablk_flags |= (is_osx_boot() && (bdev->flags & BF_DRV_DISK))?
		ABLK_INFO_DRV_DISK : ABLK_INFO_MEDIA_PRESENT;
	
	d->cntrl_func = NULL;
	
	if( bdev->flags & BF_CD_ROM ) {
		ablk_cd_registered( d );
		d->cntrl_func = ablk_cd_request;
	}
}

static int
ablk_init( void )
{
	static pci_dev_info_t pci_config = { 0x1000, 0x0003, 0x02, 0x0, 0x0100 };
	mol_device_node_t *dn = prom_find_devices("mol-blk");
	bdev_desc_t *bdev = NULL;
	
	memset( &ablk, 0, sizeof(ablk) );

	if( !is_oldworld_boot() && !dn )
		return 0;
	if( !register_osi_driver( "blk", "mol-blk", is_classic_boot()? &pci_config : NULL, &ablk.irq ) ) {
		printm("----> Failed to register the ablk driver!\n");
		return 0;
	}

	while( (bdev=bdev_get_next_volume(bdev)) ) {
		bdev_claim_volume( bdev );
		add_drive( dn, bdev );
	}
	
	pipe( ablk.ctrl_pipe );
	
	create_thread( io_thread, NULL, "blk-io" );
	register_osi_iface( osi_iface, sizeof(osi_iface) );

	return 1;
}

static void
ablk_cleanup( void )
{
	char dummy_ch=0;
	int i;

	ablk.exiting = 1;
	write( ablk.ctrl_pipe[1], &dummy_ch, 1 );
	
	close( ablk.ctrl_pipe[0] );
	close( ablk.ctrl_pipe[1] );

	for( i=0; i<ablk.ndevs; i++ ) {
		if( ablk.devs[i].bdev->flags & BF_CD_ROM )
			ablk_cd_cleanup( &ablk.devs[i] );
		bdev_close_volume( ablk.devs[i].bdev );
	}

	free( ablk.devs );
	ablk.ndevs = 0;
}

driver_interface_t ablk_driver = {
	"ablk", ablk_init, ablk_cleanup
};
