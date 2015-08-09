/* 
 *   Creation Date: <2002/11/23 14:55:46 samuel>
 *   Time-stamp: <2002/12/28 20:28:28 samuel>
 *   
 *	<unit.h>
 *	
 *	
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_UNIT
#define _H_UNIT

#include "ablk_sh.h"

struct channel;
struct unit;
typedef struct channel channel_t;

/* per partition information */

typedef struct unit {
	int			no_disk;	/* no disk present (might have a drv_num though) */
	int			mounted;	/* hack: set at eject, cleared at read/write */
	
	int			drv_num;	/* MacOS driver number (or -1 if not installed) */
	int			channel;	/* linux channel num */
	int			unit;		/* unit number (shared among partitions) */
	int			first_blk;	/* first block of partition */
	int			parnum;		/* parnum of this disk (0=unpartitioned) */
	struct channel		*ch;		/* convenience */
	
	ablk_disk_info_t	info;		/* linux side info (for the whole disk) */
	struct unit 		*next;
} unit_t;

extern int			get_channel_num( channel_t *unit );

extern void			add_unit( channel_t *ch, unit_t *unit );
extern unit_t			*get_unit( int drv_num );
extern unit_t			*get_first_unit( channel_t *ch );
	
extern void			mount_drives_hack( void );

extern OSStatus			queue_req( ParamBlockRec *pb, unit_t *unit, int ablk_req, int param,
					   char *buf, int cnt, IOCommandID ioCmdID );

extern int			find_partition( int channel, int unit, int parnum,
						int *startblk, int *nblk, int *type );

extern int			setup_units( channel_t *ch, int boot );

extern void			eject_unit_records( channel_t *ch, int unit );
extern void			register_units( channel_t *ch, int unit );


enum { 
	kPartitionTypeHFS=0, 
	kPartitionTypeDriver,
	kPartitionTypeUnknown 
};



#endif   /* _H_UNIT */
