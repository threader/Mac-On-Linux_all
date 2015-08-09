/* 
 *   Creation Date: <1999/02/04 07:29:33 samuel>
 *   Time-stamp: <2001/05/09 22:37:40 samuel>
 *   
 *	<scsi-unit.h>
 *	
 *	SCSI-device
 *   
 *   Copyright (C) 1999, 2001 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_SCSI_UNIT
#define _H_SCSI_UNIT

/* flags for register_scsi_unit() */
enum {
	f_cd_rom 		= 0x1,
	f_write_protect		= 0x2
};

extern void scsi_disks_cleanup( void );
extern int register_scsi_disk( struct pdisk *d, int scsi_id, int unit_flags );

#endif   /* _H_SCSI_UNIT */

