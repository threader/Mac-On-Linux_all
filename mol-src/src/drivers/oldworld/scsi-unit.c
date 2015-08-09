/* 
 *   Creation Date: <1999/02/04 07:28:11 samuel>
 *   Time-stamp: <2004/06/12 21:33:54 samuel>
 *   
 *	<scsi-unit.c>
 *	
 *	SCSI-unit
 *   
 *   Copyright (C) 1999, 2001, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

/* #define VERBOSE  */

#include <pthread.h>
#include <scsi/scsi.h>
#include <sys/ioctl.h>
//#include <linux/fs.h> 

#include "scsi_main.h"
#include "scsi-bus.h"
#include "scsi-unit.h"
#include "debugger.h"
#include "verbose.h"
#include "llseek.h"


SET_VERBOSE_NAME("scsi-unit")

/* #define VERBOSE_CMD */

#define SECTOR_SIZE	512		/* DON'T MODIFY */

typedef struct sense_pb {
	char	valid:1;		/* 0 */
	char	errcode:7;
	char	segnum;			/* 1 */
	char	res1:2;			/* 2 */
	char	ili:1;
	char	res2:1;
	char	key:4;
	char	info[4];		/* 3-6 */
	char	addional_len;		/* 7 */
	char	cmd_spec[4];		/* 8-11 */
	char	add_sense;		/* 12 */
	char	add_sense_qf;		/* 13 */
} sense_t;

/* add_sense constants */
#define INVALID_FIELD_IN_CDB	0x24


typedef struct unit_state {
	struct scsi_unit *unit;

	int	flags;			/* flags for this block unit */
	int	num_blocks;		/* #blocks (a 512 bytes) on disk */

	int	sector_size;		/* sector size (2048 for CD-ROM) */

	pdisk_t	*pdisk;			/* disk */

	/* data transfer parameters */
	int	req_count;		/* number of bytes to xfer */
	int	res_count;		/* number of bytes transfered */
	ulong	pos_hi, pos_lo;		/* file mark */
	char	*buf;			/* data to xfer / receive buffer (or NULL) */

	int	disk_write;		/* non-zero if DATA_OUT xfer should go to disk */

	/* misc */
	sense_t	sense;			/* SCSI sense (status information) */

	char	gbuf[32];		/* general buffer for data xfer */

	struct unit_state *next;	/* linked list */
} unit_state_t;


static unit_state_t *first_block_unit = NULL;

static int data_req( int req_count, char *buf, int may_block, int sphase, void *usr );
static int data_provided( int count, char *buf, int may_block, int sphase, void *usr );
static int do_cmd( int cmd_len, unsigned char *cmdbuf, int may_block, void *usr );

static void provide_data( unit_state_t *us, char *buf, size_t len, ulong pos_hi, ulong pos_lo );
static void request_data( unit_state_t *us, char *buf, size_t len, ulong pos_hi, ulong pos_lo );

static void set_sense( unit_state_t *us, ulong sense, ulong info );

#define S3( key, code, qualifier )  	(((qualifier)<<24)+((code)<<8)+(key))
#define S2( key, code )  		(((code)<<8)+(key))


#define INQUIRY_LEN	36
static char inquiry_buf[ INQUIRY_LEN ] ={
	0,0,2,2, 36-4,0,0, 0,
	/* Vendor (8 bytes) */
	'Q','u','a','n','t','u','m',' ',
	/* Product (16 bytes).  */
	'F','I','R','E','B','A','L','L','5','4','0','S',' ',' ',' ',' ',
	/* Revision (4 bytes) */
	'5','4','0','S'
};
static char inquiry_buf_cdrom[ INQUIRY_LEN ] ={
	5,0x80,2,2, 36-4,0,0, 0,
	/* Vendor (8 bytes) */
	'M','A','T','S','H','I','T','A',
	/* Product (16 bytes) */
	'C','D','-','R','O','M',' ','C','R','-','8','0','0','5','A',' ',
	/* Revision (4 bytes) */
	'4','.','0','i'
};
static char cdrom_toc_buf[] = {
	0,8,
	1,1,

	0,	/* res */
	0,	/* Adr | cntrl */
	1,	/* track# */
	0,	/* res */
	0,0,0,0	/* Abs addr */
};


static scsi_unit_pb_t pb = {
	1,0,0,
	NULL, NULL, NULL, NULL, NULL,		/* initiator fields */
	data_req,
	data_provided,
	do_cmd,
	NULL, 				/* message (not req) */
	NULL				/* phase (not req) */
};


/************************************************************************/
/*	FUNCTIONS							*/
/************************************************************************/

void
scsi_disks_cleanup( void )
{
	unit_state_t *next, *us = first_block_unit;
	
	for( ; us ; us=next ){
		next = us->next;
		free( us );
	}
	first_block_unit = NULL;
}


int
register_scsi_disk( pdisk_t *d, int scsi_id, int unit_flags )
{
	unit_state_t *us;

	if( scsi_id < 0 || scsi_id > 6 ){
		LOG("Illegal SCSI-id %d\n",scsi_id );
		return 1;
	}
	us = calloc( sizeof(unit_state_t),1 );
	us->pdisk = d;

	us->flags = unit_flags;
	us->unit = scsi_register_unit( &pb, scsi_id, (void*)us );
	if( us->unit == NULL ) {
		LOG("Could not register hw unit with SCSI-id %d\n", scsi_id );
		free(us);
		return 1;
	}

	us->num_blocks = p_get_num_blocks( d );
	us->sector_size = p_get_sector_size( d );

	/* always accept data */
	scsi_xfer_status( us->unit, 1 );

	/* prepare sense structure */
	us->sense.valid = 1;
	us->sense.addional_len = sizeof(struct sense_pb)-7;
	us->sense.errcode=0x70;

	/* Add unit to linked list */
	us->next = first_block_unit;
	first_block_unit = us;

	/* p_print_info( us->pdisk, "SCSI", "hw" ); */
	return 0;
}


static void
set_sense( unit_state_t *us, ulong sense, ulong info )
{
#if 1
	if( sense )
		printm("scsi-unit: ILLEGAL REQUEST sense key: %02lX, add. info %02lX\n", 
		       sense & 0xff, (sense>>8) & 0xff );
#endif

	us->sense.key = sense & 0xff;
	us->sense.add_sense = (sense>>8) & 0xff;
	us->sense.add_sense_qf = (sense>>24) & 0xff;
	*(long*)&us->sense.info[0] = info;
}


static int 
do_cmd( int cmd_len, unsigned char *cmd, int may_block, void *usr )
{
	unit_state_t *us = (unit_state_t *)usr;
	ulong laddr, *gptr;
	ulong pos_hi, pos_lo, size;
	int rsize;
	unsigned long long long_pos;
	
#ifdef VERBOSE_CMD
	int i;	
	printm("SCSI_COMMAND: ");
	for(i=0; i<cmd_len; i++ )
		printm("%02X ", cmd[i] );
	printm("\n");
#endif
	set_sense( us, NO_SENSE, 0 );
	us->disk_write = 0;
	us->buf = NULL;

	/* XXX we should reject commands with LUN!=0, *unless* a
	 * IDENTIFY message has been received (then LUN is ignored)
	 */
	if( cmd[cmd_len-1] ) {
		scsi_set_status( CHECK_CONDITION, COMMAND_COMPLETE );
		set_sense( us, ILLEGAL_REQUEST, 0 );
		return 0;
	}

	switch( cmd[0] ){
	case TEST_UNIT_READY:
		VPRINT("** TEST_UNIT_READY **\n");
		break;
	case INQUIRY:
		VPRINT("*** INQUIRY ***\n");
		if( cmd_len == 6 && cmd[4] )
			rsize = cmd[4];
		else
			rsize = INQUIRY_LEN;
		if( us->flags & f_cd_rom )
			provide_data( us, inquiry_buf_cdrom, rsize, 0,0 );
		else
			provide_data( us, inquiry_buf, rsize, 0,0 );
		break;
	case FORMAT_UNIT: 	/* 0x04 */
		LOG("Thou shall not format\n");
		break;
	case REZERO_UNIT:	/* 0x01 */
		LOG("Thou shall not rezero\n");
		break;
	case READ_6:		/* 0x08 */
		size = cmd[4] * us->sector_size;
		/* this operation won't overflow */
		pos_hi = 0;
		pos_lo = ( (cmd[1]&0x1f)*65536L+cmd[2]*256L + (ulong)cmd[3]) * us->sector_size;
		provide_data( us, NULL, size, pos_hi, pos_lo );
		VPRINT("*** READ_6 < %04lX @ %08lX > ***\n", size, pos_lo );
		break;
	case READ_10:		/* 0x28 */
		/* size could overflow - I don't believe this will be a problem though */
		size = (long)(*(short*)&cmd[7]) * us->sector_size;
		long_pos = *(long*)&cmd[2];
		long_pos *= us->sector_size;
		pos_hi = long_pos >> 32;
		pos_lo = long_pos;
		provide_data( us, NULL, size, pos_hi, pos_lo );
		VPRINT("*** READ_10 < %08lX @ %lX%08lX > ***\n", size, pos_hi, pos_lo );
		break;
	case READ_CAPACITY:	/* 0x25 */
		VPRINT("*** READ_CAPACITY ***\n");
		gptr = (ulong*)us->gbuf;
		laddr = *(long*)&cmd[2];
		if( !(cmd[8]&0x1) ) { /* PMI bit */ 
			if( laddr ){
				set_sense( us, S2( ILLEGAL_REQUEST, INVALID_FIELD_IN_CDB ), 0 );
				scsi_set_status( CHECK_CONDITION, COMMAND_COMPLETE );
				break;
			}
			*gptr++ = us->num_blocks / (us->sector_size / 512) - 1;
			*gptr++ = us->sector_size;
			provide_data( us, us->gbuf, 8, 0,0 );
		} else {
			*gptr++ = 0xffffffff;
			*gptr++ = 0;
			provide_data( us, us->gbuf, 8,0,0  );
		}
		break;
	case RELEASE:		/* 0x17 */
		VPRINT("*** RELEASE ***\n");
		/* Just return GOOD */
		break;
	case REQUEST_SENSE:	/* 0x03 */
		VPRINT("*** REQUEST_SENSE ***\n");
		provide_data( us, (char*)&us->sense, sizeof(struct sense_pb), 0,0 );
		break;
	case SEND_DIAGNOSTIC:	/* 0x1d */
		VPRINT("*** SEND_DIAGNOSTIC ***\n");
		break;
	case WRITE_6:
		if( us->flags & (f_write_protect | f_cd_rom) ) {
			scsi_set_status( CHECK_CONDITION, COMMAND_COMPLETE );
			set_sense( us, ILLEGAL_REQUEST, 0 );
			return 0;
		}
		size = cmd[4] * us->sector_size;
		pos_hi = 0;
		pos_lo = ( (cmd[1]&0x1f)*65536L+cmd[2]*256L + (ulong)cmd[3]) * us->sector_size;
		us->disk_write = 1;
		request_data( us, NULL, size, pos_hi, pos_lo );
		VPRINT("*** WRITE_6 < %04lX @ %08lX > ***\n", size, pos_lo );
		break;
	case WRITE_10:
		if( us->flags & (f_write_protect | f_cd_rom) ) {
			scsi_set_status( CHECK_CONDITION, COMMAND_COMPLETE );
			set_sense( us, ILLEGAL_REQUEST, 0 );
			return 0;
		}
		size = (long)(*(short*)&cmd[7]) * us->sector_size;
		long_pos = *(long*)&cmd[2];
		long_pos *= us->sector_size;
		pos_hi = long_pos >> 32;
		pos_lo = long_pos;
		request_data( us, NULL, size, pos_hi, pos_lo );
		us->disk_write = 1;
		VPRINT("*** WRITE_10 < %08lX @ %lX%08lX > ***\n", size, pos_hi, pos_lo );
		break;

	case SEEK_6:
	case SEEK_10:
		/* we really don't need to do anything since linked commands are not supported */
		break;
	case VERIFY:
		/* Here we should perhaps do a real verification... */
		break;
	case READ_TOC: /* 0x43 */
		VPRINT("*** READ_TOC ***\n");
		provide_data( us, cdrom_toc_buf, sizeof(cdrom_toc_buf), 0,0 );
		break;
	case ALLOW_MEDIUM_REMOVAL: /* 0x1e */
		VPRINT("*** ALLOW_MEDIUM_REMOVAL (%s) ****\n", cmd[4] & 1 ? "prevent" : "allow" );
		break;
	default:
		printm("*** Default SCSI command [0x%02x] ***\n", cmd[0]);
		scsi_set_status( CHECK_CONDITION, COMMAND_COMPLETE );
		set_sense( us, ILLEGAL_REQUEST, 0 );
		return 0;
		break;
	}

	scsi_set_status( GOOD, COMMAND_COMPLETE );
	return 0;
}


/* Data from us (DATA_IN scsi phase) */
static void
provide_data( unit_state_t *us, char *buf, size_t len, ulong pos_hi, ulong pos_lo )
{
	us->req_count = len;
	us->res_count = 0;
	us->buf = buf;
	us->pos_hi = pos_hi;
	us->pos_lo = pos_lo;
	scsi_set_data_request( len, 1 /* from target */);
}

/* Data to us (DATA_OUT scsi phase) */
static void
request_data( unit_state_t *us, char *buf, size_t len, ulong pos_hi, ulong pos_lo )
{
	us->req_count = len;
	us->res_count = 0;
	us->buf = buf;
	us->pos_hi = pos_hi;
	us->pos_lo = pos_lo;
	scsi_set_data_request( len, 0 /* to target */);
}

static int 
data_req( int req_count, char *buf, int may_block, int sphase, void *usr )
{
	unit_state_t *us = (unit_state_t *)usr;
	int len;
	unsigned long long pos;
#if 0
	printm("*UNIT* data_request %d (req_count %X, res_count %X)\n", 
	       req_count, us->req_count, us->res_count );
#endif
	len = us->req_count - us->res_count;
	if( len > req_count )
		len = req_count;

	/* From buffer or from device */
	if( us->buf ){
		memcpy( buf, us->buf+us->res_count, len );
		us->res_count += len;
		return len;
	} else {
		if( !may_block )
			return ERR_MAY_BLOCK;
		
		pos = (((unsigned long long)us->pos_hi)<<32) | us->pos_lo;
		pos += us->res_count;

		p_long_lseek( us->pdisk, (ulong)(pos>>32), (ulong)pos );
		len = p_read( us->pdisk, buf, len );
		if( len < 0 ) {
			LOG("%X @ %lX%08lX, read: %s\n",len,us->pos_hi, us->pos_lo, strerror(errno) );
			len = 0;
		}
	}
	us->res_count += len;
	return len;
}

static int 
data_provided( int count, char *buf, int may_block, int sphase, void *usr )
{
	unit_state_t *us = (unit_state_t *)usr;
	int len;
	unsigned long long pos;
#if 0
	printm("*UNIT* data_provided %d\n", count);
	stop_emulation();
#endif
	len = us->req_count - us->res_count;
	if( len > count )
		len = count;

	/* From buffer or from device */
	if( us->buf ){
		memcpy( us->buf+us->res_count, buf, len );
		us->res_count += len;
		return len;
	} else {
		if( !us->disk_write ) {
			LOG("data_provided called but disk_write is FALSE!\n");
			return len;
		}			
		if( !may_block )
			return ERR_MAY_BLOCK;
		
		pos = (((unsigned long long)us->pos_hi)<<32) | us->pos_lo;
		pos += us->res_count;

		p_long_lseek( us->pdisk, (ulong)(pos>>32), (ulong)pos );

		len = p_write( us->pdisk, buf, len );
		if( len < 0 ) {
			perrorm("SCSI-UNIT: write\n");
			len = 0;
		}
	}
	us->res_count += len;
	return len;	
}

