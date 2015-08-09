/* 
 *   Creation Date: <2002/12/27 22:02:41 samuel>
 *   Time-stamp: <2002/12/28 13:48:07 samuel>
 *   
 *	<ablk.h>
 *	
 *	
 *   
 *   Copyright (C) 2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_ABLK
#define _H_ABLK

#include "disk.h"
#include <sys/uio.h>

struct ablk_device;

typedef int		iofunc_t( bdev_desc_t *bdev, const struct iovec *vec, int count );
typedef iofunc_t	*cntrlfunc_t( struct ablk_device *d, int cmd, int param );

typedef struct ablk_device {
	bdev_desc_t	*bdev;
	int		ablk_flags;
	int		events;

	cntrlfunc_t	*cntrl_func;

	/* private to the CD module */
	int		ioctl_fd;
} ablk_device_t;

/* interface */
extern void		ablk_post_event( ablk_device_t *d, int event );

/* CD */
#ifdef __linux__
extern iofunc_t		*ablk_cd_request( ablk_device_t *d, int cmd, int param );
extern void		ablk_cd_registered( ablk_device_t *d );
extern void		ablk_cd_cleanup( ablk_device_t *d );
#else
static inline iofunc_t	*ablk_cd_request( ablk_device_t *d, int cmd, int param ) {
	return NULL; }
static inline void	ablk_cd_registered( ablk_device_t *d ) {}
static inline void	ablk_cd_cleanup( ablk_device_t *d ) {}
#endif

#endif   /* _H_ABLK */

