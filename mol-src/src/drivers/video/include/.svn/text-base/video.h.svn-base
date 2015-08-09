/* 
 *   Creation Date: <2000/12/31 04:10:03 samuel>
 *   Time-stamp: <2004/02/07 18:15:05 samuel>
 *   
 *	<video.h>
 *	
 *	Video related functions	
 *   
 *   Copyright (C) 2000, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_VIDEO
#define _H_VIDEO

struct video_module;
struct video_desc;

extern void	use_hw_cursor( int flag );
extern int	supports_vmode( struct video_module *mod, struct video_desc *vm );

/* module functions */
extern int	switch_to_fullscreen( void );
#ifdef CONFIG_FBDEV
extern int 	switch_to_fbdev_video( void );
#else
static inline int switch_to_fbdev_video( void ) { return -1; }
#endif

#ifdef CONFIG_XDGA
extern int	switch_to_xdga_video( void );
#else
static inline int switch_to_xdga_video( void ) { return -1; }
#endif


#endif   /* _H_VIDEO */
