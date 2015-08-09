/* 
 *   Creation Date: <1999-02-04 06:04:52 samuel>
 *   Time-stamp: <2004/06/12 21:32:44 samuel>
 *   
 *	<scsi-bus.h>
 *	
 *	SCSI bus emulation
 *   
 *   Copyright (C) 1999, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_SCSI_BUS
#define _H_SCSI_BUS

#include <sys/types.h>

/* SCSI-phases. Do NOT change the constants of the first eight
 * entries (follows MSG C/D I/O bits)
 */
enum {
	sphase_data_out=0, sphase_data_in, sphase_command, sphase_status,
	sphase_res1, sphase_res2, sphase_mes_out, sphase_mes_in,
	/* misc phases */
	sphase_bus_free, sphase_arbitration, sphase_reselection, sphase_selection
};

/* error codes of transfer_fp: */
#define ERR_MAY_BLOCK	-1
#define ERR_NO_DATA	-2

typedef int (*transfer_fp)( int req_count, char *buf, int may_block, int sphase, void *usr );

/* Target procs */
typedef int (*handle_message_fp )( int mes_len, unsigned char *mesbuf, void *usr );
typedef int (*handle_command_fp )( int cmd_len, unsigned char *cmdbuf, int may_block, void *usr );
typedef int (*next_sphase_fp)( int sphase, void *usr );

typedef void (*sphase_changed_fp)( int old_sphase, int new_sphase , void *usr );

typedef struct scsi_unit_pb
{
	int 	can_be_target;
	int	can_be_initiator;

	size_t	block_size;	/* (0 for default) */

	sphase_changed_fp sphase_changed_proc;		/* I */

	transfer_fp	mes_req_proc;			/* I */
	transfer_fp	mes_prov_proc;			/* I */
	transfer_fp	cmd_req_proc;			/* I */
	transfer_fp	status_prov_proc;		/* I */

	transfer_fp	data_req_proc;			/* T/I */
	transfer_fp	data_prov_proc;			/* T/I */

	handle_command_fp	handle_cmd_proc;	/* T */
	handle_message_fp	handle_mes_proc;	/* T (not req) */
	next_sphase_fp		next_sphase_proc;	/* T (not req) */
} scsi_unit_pb_t;


void scsi_bus_init( void );
void scsi_bus_cleanup( void );

struct scsi_unit *scsi_register_unit( scsi_unit_pb_t *pb, int scsi_num, void *usr );

/* INITIATOR functions */
int scsi_arbitration( struct scsi_unit *unit );
int scsi_selection( struct scsi_unit *initiator, int target, int atn );
void scsi_set_atn( int atn );

/* TARGET functions */
void scsi_queue_mes( int len, char *buf, int put_first );
void scsi_flush_mes_queue( void );

void scsi_set_data_request( int req_count, int from_target );
void scsi_set_status( int ret_status, int ending_msg );

/* TARGET/INITIATOR functions */
void scsi_reset( void );
void scsi_xfer_status( struct scsi_unit *unit, int data_available );

/* handle_message_fp return codes (0=mes handled ok) */
#define SCSI_DEF_MES_HANDLER	1
#define SCSI_MES_REJECT		2
#define SCSI_FREE_BUS		3


#endif   /* _H_SCSI_BUS */
