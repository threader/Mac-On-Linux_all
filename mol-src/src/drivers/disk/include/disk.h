/* 
 *   Creation Date: <2001/05/09 23:26:42 samuel>
 *   Time-stamp: <2004/03/24 01:24:35 samuel>
 *   
 *	<disk.h>
 *	
 *	Block-device related
 *   
 *   Copyright (C) 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_DISK
#define _H_DISK

enum { 
	BF_ENABLE_WRITE		= 1, 
	BF_FORCE		= 2,
	BF_CD_ROM		= 4,
	BF_BOOT			= 8,
	BF_BOOT1		= 16,
	
	BF_SUBDEVICE		= 32,
	BF_IGNORE		= 64,

	BF_WHOLE_DISK		= 256,
	BF_REMOVABLE		= 512,

	BF_DRV_DISK		= 1024,

	BF_ENCRYPTED		= 2048,
};

/* from disk_open.c */
extern int disk_open( char *name, int flags, int *ro_fallback, int silent );

/* from blkdev.c */
typedef struct bdev_desc {
	int			fd;
	int			flags;			/* BF_XXX */
	
	ullong 			size;			/* size of partition (multiple of 512) */

	char			*dev_name;
	char			*vol_name;

	ulong			mdb_checksum;
	ulong			name_checksum;

	int 			(*read)();		/* Read from disk image */
	int 			(*write)();		/* Write to disk image */
	int 			(*seek)();		/* Seek in disk image */
	int			(*real_read)();		/* Read to use if wrapped */
	int			(*real_write)();	/* Write to use if wrapped */
	void			(*close)();		/* Close device */

	void 			* priv;		/* Driver specific private structures */

	/* fields private to blkdev.c */
	int			priv_claimed;
	struct bdev_desc	*priv_next;
} bdev_desc_t;

extern void 		bdev_setup_drives( void );

extern bdev_desc_t	*bdev_get_next_volume( bdev_desc_t *last );
extern void		bdev_claim_volume( bdev_desc_t *bdev );
extern void		bdev_close_volume( bdev_desc_t *bdev );

extern int		reserve_scsidev( int host, int channel, int unit_id );


#endif   /* _H_DISK */


