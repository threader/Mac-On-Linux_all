/* 
 *   Creation Date: <1999/03/20 07:30:18 samuel>
 *   Time-stamp: <2002/10/26 17:07:15 samuel>
 *   
 *	<blk_shared.h>
 *	
 *	Native MOL Block Driver MacOS - linux shared header
 *   
 *   Copyright (C) 1999, 2000, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 */
 
#ifndef _BLK_SHARED_
#define _BLK_SHARED_

/* information about the drive (osi_blk driver) */ 
typedef struct DriveInfoOSI {
	unsigned long	num_blocks;		/* # 512 bytes blocks */
	unsigned long	linux_id;

	int		locked;
} DriveInfoOSI;

/* this obsoletes DriveInfo above, but we keep it due to backward compatibility issues */
typedef struct osi_blk_disk_info {
	unsigned long	nblks;
	int		linux_id;
	int		flags;
	int		res1;
} osi_blk_disk_info_t;

/* info.flags */
#define OSI_BLK_INFO_READ_ONLY	1
#define	OSI_BLK_INFO_REMOVABLE	2
#define	OSI_BLK_HAS_PARTABLE	4
#define OSI_BLK_BOOT_HINT	8

#endif



