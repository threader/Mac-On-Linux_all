/* 
 *   Creation Date: <2003/06/27 21:27:06 samuel>
 *   Time-stamp: <2004/01/05 21:15:51 samuel>
 *   
 *	<usbdev.c>
 *	
 *	Native USB device
 *   
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef __user
#define __user
#endif

#include "mol_config.h"
#include "usb-client.h"
#include <signal.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <linux/usbdevice_fs.h>
#include "poll_compat.h"
#include "async.h"
#include "byteorder.h"
#include "timer.h"
#include "res_manager.h"

typedef struct ndev ndev_t;

/* extended urb (we need to tuck on a few fields) */
typedef struct eurb {
	struct usbdevfs_urb	x;			/* must go first */
	ndev_t			*ndev;
	urb_t			*u;
	struct eurb		*next;
} eurb_t;

struct ndev {
	usb_device_t		*usb_device;
	eurb_t			*live_urbs;		/* live urbs */
	int			fd;
	int			async_id;

	ndev_t			*next;
};

static struct {
	int			probe_timer_id;
	int			disc_aevent;		/* disconnect async event */
	ndev_t			*devs;

	int			dbg_delayed_cnt;	/* release */
} s;


#define USB_DEBUG		0
#define USB_DISCONNECT_SIGNAL	42


static void
abort_all_urbs( ndev_t *d )
{
	eurb_t *e;
	int cnt = 0;

	/* complete outstanding requests */
	for( e=d->live_urbs; e; e=e->next ) {
		if( !ioctl(d->fd, USBDEVFS_DISCARDURB, &e->x) )
			cnt++;
		else if( errno != ENODEV )
			perrorm("DISCARDURB");
	}
	while( !ioctl(d->fd, USBDEVFS_REAPURBNDELAY, &e) )
		cnt--;

	while( (e=d->live_urbs) ) {
		complete_urb( e->u, CC_DEV_NOT_RESPONDING );
		d->live_urbs = e->next;
		free( e );
	}
	if( cnt > 0 )
		printm("abort_all_urbs: unexpected ERROR\n");
}

static void
device_disconnect( ndev_t *d )
{
	ndev_t **pp;
	
	printm("USB disconnect\n" );

	abort_all_urbs( d );
	unregister_usb_device( d->usb_device );
	delete_async_handler( d->async_id );
	if( close(d->fd) )
		perrorm("close");

	for( pp=&s.devs; *pp && *pp != d; pp=&(**pp).next )
		;
	if( !*pp )
		fatal("ndev missing!\n");
	*pp = (**pp).next;
}

static void
disc_aevent( int dummy_token )
{
	struct usbdevfs_connectinfo ci;
	ndev_t *nd = nd=s.devs;

	/* poll all devices in order to find the disconnected one... */
	while( nd ) {
		if( !ioctl(nd->fd, USBDEVFS_CONNECTINFO, &ci ) )
			nd = nd->next;
		else {
			device_disconnect( nd );
			nd = s.devs;
		}
	}
}

static void
handle_reaped_urb( eurb_t *e ) 
{
	int status = CC_NOERROR;
	ndev_t *d = e->ndev;
	eurb_t **ee;

	/* unlink eurb */
	for( ee=&d->live_urbs; *ee && *ee != e ; ee=&(**ee).next )
		;
	*ee = e->next;
	e->u->actcount = e->x.actual_length;

	if( e->x.status ) {
		switch( e->x.status ) {
		case -EPIPE:
			status = CC_STALL;
			break;
		case -EILSEQ:
			status = CC_CRC;
			break;
		case -EPROTO:
			status = CC_BIT_STUFFING;
			break;
		case -ETIMEDOUT:
			status = CC_DEV_NOT_RESPONDING;
			break;
		case -EOVERFLOW:
			status = CC_DATA_OVERRUN;
			break;
		case -EREMOTEIO:
			status = CC_DATA_UNDERRUN;
			break;
		case -ECOMM:
			status = CC_BUFFER_OVERRUN;
			break;
		case -ENOSR:
			status = CC_BUFFER_UNDERRUN;
			break;
		case -ENODEV:
			complete_urb( e->u, CC_DEV_NOT_RESPONDING );
			device_disconnect( d );
			free( e );
			return;
		case -ENOENT:
			/* user killed */
			status = CC_STALL;
			break;
		default:
			printm("unknown USB error %d\n", e->x.status );
			/* don't know about this one, but stall seems appropriate */
			status = CC_STALL;
			break;
		}
	}
	if( USB_DEBUG ) {
		int i;
		printm(" @ actlen: %d ", e->x.actual_length );
		printm(" status: %d\n", e->x.status );
		for( i=0; i<e->x.actual_length && i<16;  i++ )
			printm("%02x ", e->u->buf[i] );
		printm("\n");
	}
	complete_urb( e->u, status );
	free( e );
}

static void
async_ev( int fd, int events ) 
{
	eurb_t *e;

	while( !ioctl(fd, USBDEVFS_REAPURBNDELAY, &e) )
		handle_reaped_urb( e );

	if( errno != EAGAIN ) {
		if( errno == ENODEV )
			disc_aevent(0);
		else
			perrorm("reapurb");
	}
}

static int
bug_workaround( urb_t *u )
{
	int ret = CC_STALL;

	switch( u->buf[1] ) {
	case 1: /* CLEAR_FEATURE */
	case 3: /* SET_FEATURE */
		if( ld_le16((u16*)&u->buf[2]) & ~3 )
			printm("WARNING: unimplemented CLEAR/SET_FEATURE\n");
		/* CC_STALL is fine */
		break;
	case 0: /* GET_STATUS */
		if( u->size != 10 )
			return 1;
		u->buf[8] = u->buf[9] = 0;
		u->actcount = 2;
		ret = CC_NOERROR;
		break;
	default:
		printm("unexpected ctrl opcode\n");
		return 1;
	}
	return ret;
}

static void
submit_urb( urb_t *u, void *refcon )
{
	ndev_t *d = (ndev_t*)refcon;	
	eurb_t *e;

	e = (eurb_t*)malloc( sizeof(*e) );

	/* call fields */
	if( u->type == kCtrlType ) {
		e->x.endpoint = u->en;
		e->x.type = USBDEVFS_URB_TYPE_CONTROL;
	} else {
		e->x.endpoint = u->en | (u->is_read ? 0x80 : 0);
		e->x.type = (u->type == kBulkType)? 
			USBDEVFS_URB_TYPE_BULK : USBDEVFS_URB_TYPE_INTERRUPT;
	}

	e->x.flags = 0;
	e->x.buffer = u->buf;
	e->x.buffer_length = u->size;
	e->x.signr = 0;
	e->x.error_count = 0;
	e->x.status = 0;

	/* our fields */
	e->next = d->live_urbs;
	e->ndev = d;
	e->u = u;

	if( ioctl(d->fd, USBDEVFS_SUBMITURB, &e->x) < 0 ) {
		int status = CC_STALL;
		
		/* kernel bug: control commands targeting endpoint 0 are rejected */
		if( errno == ENOENT && !u->en && u->type == kCtrlType ) {
			/* target is control enpoint 0? */
			if( (u->buf[0] & ~0x80) == 2 && !ld_le16((u16*)&u->buf[4]) )
				status = bug_workaround(u);
		} else {
			perrorm("submiturb");
			printm("EN: %02x TYPE %d\n", e->x.endpoint, u->type );
		}
		complete_urb( u, status );
		free( e );
		if( errno == ENODEV )
			device_disconnect( d );
		return;
	}
	d->live_urbs = e;
}

/* abort (call completion when done) */
static void
abort_urb( urb_t *u, void *refcon )
{
	ndev_t *d = (ndev_t*)refcon;	
	eurb_t *e;

	for( e=d->live_urbs; e && e->u != u ; e=e->next )
		;
	if( !e ) {
		printm("cancel_urb: bad urb\n");
		return;
	}
	if( ioctl(d->fd, USBDEVFS_DISCARDURB, &e->x) < 0 )
		perrorm("DISCARDURB");
}

static void
reset( void *refcon )
{
	int i;
	ndev_t *d = (ndev_t*)refcon;

	abort_all_urbs( d );

	/* unfortunately this only fakes reset */
	if( ioctl(d->fd, USBDEVFS_RESET) < 0 )
		perrorm("USBDEVFS_RESET");

	for( i=0; i<=0xff; i++ ) {
		if( ioctl(d->fd, USBDEVFS_RESETEP, &i) )
			continue;
		if( ioctl(d->fd, USBDEVFS_CLEAR_HALT, &i) )
			perrorm("CLEARHALT");
		/* printm("Reset endpoint %x\n", i ); */
	}
}

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static usb_ops_t ops = {
	.submit		= submit_urb,
	.abort		= abort_urb,
	.reset		= reset,
};

static void
usb_connect_probe( int id, void *dummy, int dummy_lost )
{
	struct usbdevfs_disconnectsignal ds;
	struct dirent *de;
	char name[40];
	int bus, fd;
	ndev_t *nd;
	DIR *d;
	
	for( bus=1; bus<10; bus++ ) {
		sprintf(name, "/proc/bus/usb/%03d", bus );
		if( !(d=opendir(name)) )
			break;
		while( (de=readdir(d)) ) {
			int fn = strtol( de->d_name, NULL, 10 );
			uint iface = 0;

			sprintf(name, "/proc/bus/usb/%03d/%03d", bus, fn );
			if( (fd=open(name, O_RDWR)) < 0 )
				continue;
			for( iface=0; iface<10; iface++ )
				if( ioctl(fd, USBDEVFS_CLAIMINTERFACE, &iface) < 0 )
					break;
			if( !iface || !(nd=malloc(sizeof(*nd))) ) {
				close( fd );
				continue;
			}
			memset( nd, 0, sizeof(*nd) );
			nd->next = s.devs;
			s.devs = nd;
			nd->fd = fd;

			if( !(nd->usb_device=register_usb_device(&ops, nd)) ) {
				free( nd );
				close( fd );
				break;
			}
			nd->async_id = add_async_handler( fd, POLLIN | POLLOUT | POLLHUP, async_ev, 0 );

			printm("USB device connect (%d:%d)\n", bus, fn );
			if( ioctl(fd, USBDEVFS_RESET, 0) < 0 )
				perrorm("USBDEVFS_RESET");

			ds.context = nd;
			ds.signr = USB_DISCONNECT_SIGNAL;
			if( ioctl(fd, USBDEVFS_DISCSIGNAL, &ds) )
				perrorm("USBDEVFS_DISCSIGNAL");
		}
		closedir( d );
	}
}

static void
disconnect_signal( int sig_num, siginfo_t *sinfo, struct ucontext *puc, ulong rt_sf )
{
	/* Hrm... the sinfo->si_addr is 0. From the kernel source it appears
	 * it should be the the context we specified in the ioctl. Oh well...
	 */
	send_aevent( s.disc_aevent );
}

void
usbdev_init( void )
{
	struct sigaction act;
	sigset_t set;

	if( !get_bool_res("enable_usb") )
		return;
	
	/* signal */
	memset( &act, 0, sizeof(act) );
	act.sa_sigaction = (void*)disconnect_signal;
	act.sa_flags = SA_RESTART | SA_SIGINFO;
	sigaction( USB_DISCONNECT_SIGNAL, &act, NULL );
	sigemptyset( &set );
	sigaddset( &set, USB_DISCONNECT_SIGNAL );
	pthread_sigmask( SIG_UNBLOCK, &set, NULL );

	s.disc_aevent = add_aevent_handler( disc_aevent );
	s.probe_timer_id = new_ptimer( usb_connect_probe, NULL, 1 /* autoresume */ );
	set_ptimer( s.probe_timer_id, 1000000 );
}

void
usbdev_cleanup( void )
{
	if( s.dbg_delayed_cnt )
		printm("USBDEV: delayed count = %d\n", s.dbg_delayed_cnt );

	while( s.devs )
		device_disconnect( s.devs );
}
