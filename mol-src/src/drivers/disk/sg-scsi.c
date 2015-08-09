/* 
 *   Creation Date: <2003/07/09 23:40:42 samuel>
 *   Time-stamp: <2004/03/25 17:11:12 samuel>
 *   
 *	<sg-scsi.c>
 *	
 *	SCSI interface (through /dev/sg)
 *   
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#include "mol_config.h"
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <sys/ioctl.h>

#include "scsi-client.h"
#include "async.h"
#include "poll_compat.h"
#include "memory.h"
#include "booter.h"
#include "res_manager.h"
#include "disk.h"


typedef struct ndev {
	scsi_dev_t		*scsi_dev;
	int			async_id;
	int			fd;

	scsi_ureq_t		*queue;
	scsi_ureq_t		**queue_tp;

	struct ndev		*next;
} ndev_t;

typedef struct {
	ndev_t			*dev;
} priv_data_t;

static ndev_t 			*devs;


/************************************************************************/
/*	DEBUG								*/
/************************************************************************/

static inline void
print_dbg_info( sg_io_hdr_t *io )
{
#ifdef CONFIG_SCSIDEBUG
	if( io->resid  )
		printm("Residue %d\n", io->resid );

	if( io->masked_status && io->masked_status != 1 )
		printm("SCSI-STATUS: %x %x\n", io->status, io->masked_status );

	if( io->msg_status )
		printm("MSG-STATUS: %d\n", io->msg_status );

	if( io->driver_status && (io->driver_status & 0x1f) != 0x8 )
		printm("DRIVER_STATUS %x\n", io->driver_status );
#endif
}


/************************************************************************/
/*	Engine								*/
/************************************************************************/

static void
unlink_req( ndev_t *dev, scsi_ureq_t *u )
{
	scsi_ureq_t **pp;

	for( pp=&dev->queue; *pp && *pp != u ; pp=&(**pp).next )
		;
	*pp = u->next;
	for( ; *pp ; pp=&(**pp).next )
		;
	dev->queue_tp = pp;
}

static void
async_completion( int fd, int events )
{
	sg_io_hdr_t io;
	scsi_ureq_t *u;
	ndev_t *dev;
	
	if( read(fd, &io, sizeof(io)) < 0 ) {
		perrorm("SCSI read");
		return;
	}
	u = io.usr_ptr;
	dev = ((priv_data_t*)u->pdata)->dev;
	unlink_req( dev, u );

	u->scsi_status = io.masked_status << 1;

	if( (io.driver_status & 0x1f) == 0x8 )
		u->scsi_status = SCSI_STATUS_CHECK_CONDITION;

	if( io.sb_len_wr > 2 ) {
		u->sb_actlen = io.sb_len_wr;
		u->scsi_status = SCSI_STATUS_CHECK_CONDITION;

		/* The Iomega CD burner (USB) returns CHECK_CONDITION but without an
		 * error in the autosense buffer. The returned data is valid though.
		 * We just ignore this condition.
		 */
		if( !u->sb[2] ) {
			printm("Zero SCSI sense ignored (cmd 0x%x)\n", u->cdb[0] );
			u->sb_actlen = u->scsi_status = 0;
		}
	}
	u->act_size = u->size - io.resid;

	print_dbg_info( &io );
	complete_scsi_req( dev->scsi_dev, u );
}

static void
execute( scsi_ureq_t *u, void *refcon )
{
	priv_data_t *pd = (priv_data_t*)u->pdata;
	ndev_t *dev = (ndev_t*)refcon;
	sg_io_hdr_t io;

	pd->dev = dev;

	/* queue to tail */
	u->next = NULL;
	*dev->queue_tp = u;
	dev->queue_tp = &u->next;

	/* issue */
	memset( &io, 0, sizeof(io) );
	io.interface_id = 'S';
	io.dxfer_direction = u->is_write ? SG_DXFER_TO_DEV : SG_DXFER_FROM_DEV;
	io.cmd_len = u->cdb_len;
	io.mx_sb_len = sizeof(u->sb);
	io.iovec_count = u->n_sg;
	io.dxfer_len = u->size;
	io.dxferp = (void*)u->iovec;
	io.cmdp = (unsigned char *)u->cdb;
	io.sbp = (unsigned char *)u->sb;
	io.timeout = 1000000; 
	/* io.flags = 0; */
	io.usr_ptr = u;

	if( write(dev->fd, &io, sizeof(io)) < 0 ) {
		perrorm("SCSI queue cmd");

		/* FIXME */
		unlink_req( dev, u );
		complete_scsi_req( dev->scsi_dev, u );
		return;
	}
}


/************************************************************************/
/*	reservation							*/
/************************************************************************/

/* checks whether fd and scsi_id corresponds to the same SCSI device */
static int
same_device( int fd, u32 scsi_id )
{
	struct {			/* not in a public headerfile :-( */
		u32 dev_id;		/* host, channel, lun, id */
		u32 host_unique_id;
	} idlun;
	struct stat sb;
	u32 major;

	if( fstat(fd, &sb) < 0 )
		return 0;

	/* is this a /dev/scdN or /dev/sd[a-z] device? */
	major = ((((u32)sb.st_rdev) >> 8) & 0xff);
	if( major != 0xb && major != 8 )
		return 0;
	
	if( ioctl(fd, SCSI_IOCTL_GET_IDLUN, &idlun) < 0 ) {
		perrorm("GET_IDLUN");
		return 0;
	}
	return (idlun.dev_id & 0xffff00ff) == scsi_id;
}

static int
reserve_bdevs( int host, int channel, int unit_id )
{
	u32 id = (host << 24) | ((channel & 0xff)<<16) | (unit_id & 0xff);
	bdev_desc_t *bdev = 0;

	/* drop any bdev devices that refers to this SCSI device */
	while( (bdev=bdev_get_next_volume(bdev)) ) {
		if( !same_device(bdev->fd, id) )
			continue;
		printm("    SCSI %s  [handled as a generic SCSI device]\n", bdev->dev_name );
		bdev_close_volume( bdev );
		bdev = 0;
	}
	return 0;
}


/************************************************************************/
/*	init / cleanup / probing					*/
/************************************************************************/

static scsi_ops_t scsi_ops = {
	.execute	= execute,
};

static char *types[16] = {
	"DISK", "TAPE", "PRINTER", "PROCESSOR", "WORM", "CD/DVD", "SCANNER", "OPTICAL", "CHANGER",
	"COMM-LINK", "GARTS_0A", "GARTS_0B", NULL, "ENCLOSURE"
};

static int
probe_device( int n, int auto_ok ) 
{
	int i, fd, match;
	char *p, name[20], buf[128], retbuf[36];
	char inq_cmd[6] = { 0x12, 0, 0, 0, sizeof(retbuf), 0 };
	struct sg_scsi_id id;
	sg_io_hdr_t io;
	
	sprintf( name, "/dev/sg%d", n );
	if( (fd=open(name, O_RDWR | O_NONBLOCK)) < 0 )
		return -1;

	if( ioctl(fd, SG_GET_SCSI_ID, &id) < 0 ) {
		close( fd );
		return -1;
	}

	/* explicitly exported? */
	sprintf( buf, "%d:%d:%d", id.host_no, id.channel, id.scsi_id );
	for( match=0, i=0; (p=get_str_res_ind("scsi_dev", i, 0)) ; i++ )
		match |= !strcmp( p, buf );

	switch( id.scsi_type ) {
	case 1: /* TAPE */
	case 2: /* PRINTER */
	case 3: /* PROCESSOR */
	case 4: /* WORM */

	case 6: /* SCANNER */
	case 8: /* Changer (jukebox) */
	case 9: /* communication link */
		break;
	case 5: /* CD-ROM, DVD, CD burners etc */
		if( is_classic_boot() && get_str_res("cdboot") ) {
			printm("    SCSI-<%d:%d:%d> ignored (CD-booting)\n",
			       id.host_no, id.channel, id.scsi_id );
			goto bad;
		}
		if( !match && !get_bool_res("generic_scsi_for_cds") ) {
			printm("    Generic SCSI for CD/DVDs disabled\n");
			auto_ok = 0;
		}
		break;
	default:
	case 0: /* DISK */
	case 7: /* OPTICAL (as DISK) */
		auto_ok = 0;
		break;
	}
	if( !match && !auto_ok )
		goto bad;

	/* send INQUIRY */
	memset( &io, 0, sizeof(io) );
	io.interface_id = 'S';
	io.dxfer_direction = SG_DXFER_FROM_DEV;
	io.cmd_len = sizeof(inq_cmd);
	io.cmdp = (unsigned char *) inq_cmd;
	io.dxfer_len = sizeof(retbuf);
	io.dxferp = retbuf;
	io.timeout = -1;
	if( ioctl(fd, SG_IO, &io) < 0 )
		goto bad;

	p = NULL;
	if( (uint)id.scsi_type < sizeof(types)/sizeof(types[0]) )
		p = types[ id.scsi_type ];
	if( !p ) {
		sprintf( buf, "TYPE-0x%x", id.scsi_type );
		p = buf;
	}
	printm("    %s SCSI-<%d:%d:%d> %s ", name, id.host_no, id.channel, id.scsi_id, p );
	strncpy0( buf, &retbuf[8], 9 );
	printm("%s ", buf);
	strncpy0( buf, &retbuf[16], 17 );
	printm("%s ", buf);
	strncpy0( buf, &retbuf[32], 5 );
	printm("%s\n", buf);

	/* claim any bdevs corresponding to this device */
	if( reserve_bdevs(id.host_no, id.channel, id.scsi_id) ) {
		printm("    scsidev already in use!\n");
		goto bad;
	}
	return fd;
bad:
	close( fd );
	return -1;
}


void
sg_scsi_init( void )
{
	int i, fd, auto_ok = get_bool_res("autoprobe_scsi");
	ndev_t *d;

	if( !auto_ok )
		printm("    [SCSI auto-probing disabled]\n\n");

	for( i=0; i<=16; i++ ) {
		if( (fd=probe_device(i, auto_ok)) < 0 )
			continue;

		if( !(d=malloc(sizeof(ndev_t))) )
			break;
		memset( d, 0, sizeof(*d) );
	
		d->fd = fd;
		d->queue_tp = &d->queue;
		d->async_id = add_async_handler( fd, POLLIN | POLLHUP, async_completion, 0 );
		d->scsi_dev = register_scsidev( &scsi_ops, sizeof(priv_data_t), d );

		/* the transform stuff in ide-scsi.c is not needed */
		ioctl( fd, SG_SET_TRANSFORM, 0 );
		//ioctl( fd, SG_SET_TIMEOUT, &timeout );
		
		d->next = devs;
		devs = d;
	}
}

void
sg_scsi_cleanup( void )
{
	ndev_t *d;
	
	while( (d=devs) ) {
		devs = d->next;

		unregister_scsidev( d->scsi_dev );
		close( d->fd );
		free( d );
	}
}
