/* 
 *   Creation Date: <1999-12-31 20:32:35 samuel>
 *   Time-stamp: <2003/02/28 14:16:41 samuel>
 *   
 *	<video_module.h>
 *	
 *	
 *   
 *   Copyright (C) 1999, 2000, 2003, 2004 Samuel Rydh
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_VIDEO_MODULE
#define _H_VIDEO_MODULE

typedef struct video_module 	video_module_t;
typedef struct video_desc	video_desc_t;

struct video_module {
	const char	*name;
	
	int		(*vinit)( video_module_t *m );
	void		(*vcleanup)( video_module_t *m );

	int		(*vopen)( video_desc_t *vmode );
	void		(*vclose)( void );
	void		(*vrefresh)( void );
	void		(*setcmap)( char *cmap );
	void		(*vbl)( void );
	void		(*vpowermode)( int mode );

	video_desc_t	*modes;		/* modes supported by this driver */
	video_module_t	*next;
};

extern video_module_t	xvideo_module;
extern video_module_t	fbdev_module;
extern video_module_t 	xdga_module;
extern video_module_t	vnc_module;
extern video_module_t	offscreen_module;

struct video_desc {
	int		offs;			/* offs to actual fb data (from page boundary) */
        int 		rowbytes;
        int 		w,h;			/* width, height */
	int 		depth;
	int		refresh;		/* Hz * 65536 (for verbose information only) */
	void		*module_data;		/* private field (fbdev info for instance) */

	/* fields to be filled in by vopen */
	char		*lvbase;		/* page-aligned linux address of mapping */
	ulong		map_base;		/* mapping address to be used instead of lvbase if nonzero */
	int		mmu_flags;		/* "special" MMU options (for acceleration etc) */

	/* next supported video mode */
	video_desc_t	*next;
};

extern void 	video_module_become( video_module_t *m );
extern int 	video_module_yield( video_module_t *yielder, int may_fail );

extern char 	*get_vmode_str( video_desc_t *vm, char *buf, int size );

static inline int
std_depth( int depth ) {
	switch( depth ){
	case 16: 
		return 15;
	case 24: 
		return 32; }
	return depth;
}

#define FBBUF_SIZE( vm ) \
	(((vm)->rowbytes * (vm)->h + (vm)->offs + 0xfff) & ~0xfff)

#endif   /* _H_VIDEO */
