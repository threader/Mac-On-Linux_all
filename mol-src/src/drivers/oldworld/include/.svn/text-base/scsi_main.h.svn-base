/* 
 *   Creation Date: <1999/05/01 02:59:20 samuel>
 *   Time-stamp: <2002/12/07 22:30:51 samuel>
 *   
 *	<scsi_main.h>
 *	
 *	
 *   
 *   Copyright (C) 1999, 2001, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_SCSI_MAIN
#define _H_SCSI_MAIN

extern int	s53c94_available( void );
extern int	mesh_available( void );

/********** dummy disk interface ******** */

typedef struct pdisk pdisk_t;

extern ssize_t 	p_read( pdisk_t *disk, void *buf, size_t count );
extern ssize_t 	p_write( pdisk_t *disk, const void *buf, size_t count );
extern int 	p_lseek( pdisk_t *disk, long block, long offs );
extern ulong	p_get_num_blocks( pdisk_t *d );
extern int	p_get_sector_size( pdisk_t *d );

#define 	p_long_lseek( d, offs_hi, offs_lo ) \
			p_lseek( (d), ((offs_hi)<<23)|((offs_lo)>>9), (offs_lo) & 0x1ff )

#endif   /* _H_SCSI_MAIN */


