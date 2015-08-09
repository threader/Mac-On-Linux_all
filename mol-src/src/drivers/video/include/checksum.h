/* 
 *   Creation Date: <2002/04/27 20:23:37 samuel>
 *   Time-stamp: <2002/04/28 20:30:15 samuel>
 *   
 *	<checksum.h>
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

#ifndef _H_CHECKSUM
#define _H_CHECKSUM

#include "video_module.h"

typedef struct video_cksum video_cksum_t;
typedef void 		cksum_redraw_func( int x, int y, int w, int h );

extern video_cksum_t 	*alloc_vcksum( video_desc_t *vmode );
extern void		free_vcksum( video_cksum_t *csum );
extern void 		vcksum_calc( video_cksum_t *csum, int y, int height );
extern void		vcksum_redraw( video_cksum_t *csum, int y, int height, cksum_redraw_func *func );


#endif   /* _H_CHECKSUM */
