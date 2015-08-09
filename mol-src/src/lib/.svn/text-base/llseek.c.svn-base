/* 
 *   Creation Date: <1999/03/29 05:22:45 samuel>
 *   Time-stamp: <2004/03/15 20:26:13 samuel>
 *   
 *	<long_lseek.c>
 *	
 *	64-bit lseek (for block devices)
 *   
 *   Copyright (C) 1999, 2000, 2001, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/ioctl.h>
#include <sys/file.h>
#include "llseek.h"
#ifdef __darwin__
#include <sys/disk.h>
#endif
#ifdef __linux__
#define BLKGETSIZE _IO(0x12,96)		/* from <linux/fs.h> */
#endif

ulong
get_file_size_in_blks( int fd )
{
	struct stat st;
	ulong num_blocks;
	
	if( fstat(fd, &st) < 0 ) {
		perrorm("fstat");
		return 0;
	}
	num_blocks = st.st_size >> 9;
	if( S_ISBLK(st.st_mode) ) {
#ifdef __linux__
		/* note: certain devices (e.g. /dev/sr0 does not implement BLKGETSIZE) */
		if( !ioctl(fd, BLKGETSIZE, &num_blocks) )
			return num_blocks;
#endif
#ifdef __darwin__
		int bs;
		u_int64_t nblks;
		if( !ioctl(fd, DKIOCGETBLOCKSIZE, &bs) && !ioctl(fd, DKIOCGETBLOCKCOUNT, &nblks) )
			return nblks / (bs/512);
#endif
		/* XXX fixme */
		return 0;
	}
	return num_blocks;
}

