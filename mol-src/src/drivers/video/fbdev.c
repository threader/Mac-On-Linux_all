/* 
 *   Creation Date: <1999/03/13 23:09:47 samuel>
 *   Time-stamp: <2003/09/02 23:02:37 samuel>
 *   
 *	<fbdev.c>
 *	
 *	Linux part of MacOS based video driver
 *   
 *   Copyight (C) 2003, 2004 Samuel Rydh <samuel@ibrium.se>
 *
 *   Gamma implemented by Takashi Oe <toe@unlserve.unl.edu>
 *
 *   Copyright (C) 1999, 2000 Benjamin Herrenschmidt <bh40@calva.net>
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <math.h>

#include "verbose.h"
#include "console.h"
#include "memory.h"
#include "wrapper.h"
#include "res_manager.h"
#include "mouse_sh.h"
#include "video_module.h"
#include "booter.h"
#include "input.h"
#include "keycodes.h"
#include "video.h"
#include "video_sh.h"

/*
 * Ideally, we'd want to include a kernel header like linux/console.h
 * with proper defines and all and be done with it, but it's not so easy....
 */
#define VESA_NO_BLANKING        0
#define VESA_VSYNC_SUSPEND      1
#define VESA_HSYNC_SUSPEND      2
#define VESA_POWERDOWN          3

static struct {
	int		fd;
	int		has_console;

	int		is_open;
	video_desc_t	vmode;			/* if open */

	short		*color_table;
	struct fb_cmap	cmap;

	double		gamma[3];

	int		use_cache;
	int		blank;			/* display power state */

	int		startup_mode_ok;
	struct fb_var_screeninfo startup_var;	
	struct fb_fix_screeninfo startup_fix;

	int		initialized;
} cv[1];


static void
setup_gamma( void )
{
	double val2, val=1.0;
	char *buf;
	int i;
	
	for( i=0; i<3; i++ ) {
		cv->gamma[i] = val;
		if( !(buf=get_str_res_ind("gamma",0,i)) )
			continue;
		val2 = atof(buf);
		if( val2 < 0.1 || val2 > 10 ) {
			printm("Bad gamma value (should be one or three"
			       "values between 0.1 and 10.0\n");
			continue;
		}
		cv->gamma[i] = val = 1/val2;
	}
}

/* format is [R,G,B] x 256 */
static void 
setcmap( char *pal )
{
	int i;

	if( !cv->is_open ) {
		printm("setcmap: not open!\n");
		return;
	}
	for( i=0; i<256; i++ ) {
		cv->cmap.red[i] = (uint)(65535.0 * pow( (double)(*pal++)/255.0, cv->gamma[0] ));
		cv->cmap.green[i] = (uint)(65535.0 * pow( (double)(*pal++)/255.0, cv->gamma[1] ));
		cv->cmap.blue[i] = (uint)(65535.0 * pow( (double)(*pal++)/255.0, cv->gamma[2] ));
	}
	if( cv->has_console )
		ioctl(cv->fd, FBIOPUTCMAP, &cv->cmap );
}

static void
vpowermode( int mode ) 
{
	if( !cv->is_open || !cv->has_console )
		return;

	switch( mode ) {
	case kAVPowerOff:
		cv->blank = VESA_POWERDOWN + 1;
		break;
	case kAVPowerStandby:
		cv->blank = VESA_HSYNC_SUSPEND + 1;
		break;
	case kAVPowerSuspend:
		cv->blank = VESA_VSYNC_SUSPEND + 1;
		break;
	case kAVPowerOn:
	default:
		cv->blank = 0;
		break;
	}
	ioctl( cv->fd, FBIOBLANK, (unsigned long)cv->blank );
}


static int
vopen( video_desc_t *vm )
{
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;

	if( cv->is_open || !cv->has_console )
		return 1;

	/* Put console in graphics mode and open display */
	console_set_gfx_mode(1);
	var = *(struct fb_var_screeninfo*)vm->module_data;
	var.activate = FB_ACTIVATE_NOW;
	if( ioctl(cv->fd, FBIOPUT_VSCREENINFO, &var) < 0 ) {
		perrorm("Could not set VSCREENINFO\n");
		goto bail;
	}

	/* is this panning necessary? */
        var.xoffset = 0;
        var.yoffset = 0;
        ioctl(cv->fd, FBIOPAN_DISPLAY, &var);

	if( ioctl(cv->fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		perrorm("Could not get FSCREENINFO!\n");
		goto bail;
	}

	if( vm->offs != ((ulong)fix.smem_start & 0xfff) ) {
		printm("Framebuffer offset has changed!\n");
		goto bail;
	}
	if( vm->rowbytes != fix.line_length ) {
		printm("Rowbytes has changed (was %d, now %d)!\n", vm->rowbytes, fix.line_length);
		goto bail;
	}

	/* runtime fields */
	vm->map_base = (ulong)fix.smem_start & ~0xfff;
        vm->lvbase = map_phys_mem( NULL, vm->map_base, FBBUF_SIZE(vm), PROT_READ | PROT_WRITE );
	vm->mmu_flags = MAPPING_PHYSICAL;

	if( is_newworld_boot() || is_oldworld_boot() )
		vm->mmu_flags |= MAPPING_DBAT;

	/* Use cache with write through bit set */
	vm->mmu_flags |= cv->use_cache? MAPPING_FORCE_CACHE : MAPPING_MACOS_CONTROLS_CACHE;
	use_hw_cursor(0);	/* no hw-cursor for now */

	cv->is_open = 1;
	cv->vmode = *vm;

	return 0;

bail:
	console_set_gfx_mode(0);
	return 1;
}

/* This function is sometimes called (indirectly) from the console thread */
static void
vclose( void )
{
	if( !cv->is_open )
		return;

	/* make sure the display power is on when we leave */
	if( cv->blank )
		vpowermode( kAVPowerOn );

	cv->is_open = 0;

	if( cv->vmode.lvbase ) {
		/* XXX: memset on the framebuffer causes the process to die ?!? */
		size_t size = (cv->vmode.h * cv->vmode.rowbytes)/sizeof(long);
		long *p = (long*)(cv->vmode.lvbase + cv->vmode.offs);
		while( size-- )
			*p++ = 0;
	}
	unmap_mem( cv->vmode.lvbase, FBBUF_SIZE( &cv->vmode ) );
	console_set_gfx_mode(0);
}

/************************************************************************/
/*	video mode setup						*/
/************************************************************************/

static void
add_mode( struct fb_var_screeninfo *var, int depth, int rowbytes, int offs, ulong refresh )
{
	video_desc_t *vm;

	vm = calloc( 1, sizeof(*vm)+sizeof(struct fb_var_screeninfo) );
	vm->w = var->xres;
	vm->h = var->yres;
	vm->rowbytes = rowbytes;
	vm->offs = offs;
	vm->depth = std_depth( depth );
	var->bits_per_pixel = vm->depth;
	vm->refresh = refresh;
	vm->module_data = (void*)&vm[1];
	*(struct fb_var_screeninfo*)vm->module_data = *var;

	/* add mode to linked list */
	vm->next = fbdev_module.modes;			
	fbdev_module.modes = vm;
}

static int
setup_video_modes( void )
{
	char *vmodes_file = get_filename_res("fbdev_prefs");
	struct fb_var_screeninfo v;
	int modes_added = 0;
	char buf[160];
	FILE *f=NULL;

	if( !vmodes_file )
		printm("---> molrc resource 'video_modes' is missing!\n");
	if( vmodes_file && (f=fopen( vmodes_file, "r")) ){
		int line, done=0, err, i;		
		for( line=0; !done; ){
			while( !(done=!fgets( buf, sizeof(buf), f )) ) {
				line++;
				if( buf[0] != '#' )
					break;
			}
			if( done )
				break;
			memset( &v, 0, sizeof(v) );
			err = sscanf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", 
				     &v.xres, &v.yres, &v.xres_virtual, &v.yres_virtual, &v.xoffset, &v.yoffset,
				     &v.pixclock, &v.left_margin, &v.right_margin, &v.upper_margin, &v.lower_margin,
 				     &v.hsync_len, &v.vsync_len, &v.sync, &v.vmode ) != 15;
			v.xres_virtual = v.xres;
			v.yres_virtual = v.yres;
			v.xoffset = v.yoffset = 0;
			if( !err ) {
				int ndepths, depth, rowbytes, page_offs, refresh;
				line++;
				err = fscanf(f, "%d %d", &ndepths, &refresh ) != 2;
				for( i=0; i<3; i++ ) {
					if( !err )
						err = fscanf( f, "%d %d %d", &depth, &rowbytes, &page_offs ) != 3;
					if( !err && i < ndepths ) {
						modes_added++;
						add_mode( &v, depth, rowbytes, page_offs, refresh );
					}
				}
				if(fgets( buf, sizeof(buf),  f) == NULL) {
					err = 1;
					break;
				}
			}
			if( err )
				printm("Line %d in '%s' is malformed.\n", line, vmodes_file );
		}
		fclose( f );
	}
	if( vmodes_file && !f ) {
		printm("---> Could not open the video modes configuration file (%s)\n", vmodes_file );
	}
	
	if( !modes_added ) {
		if( cv->startup_mode_ok ){
			struct fb_fix_screeninfo *fix = &cv->startup_fix;
			v = cv->startup_var;
			add_mode( &v, v.bits_per_pixel, fix->line_length, (int)fix->smem_start & 0xfff, (75<<16) );
		}
	}
	return !fbdev_module.modes;
}

static void
free_vmodes( void )
{
	video_desc_t *vm;

	while( (vm=fbdev_module.modes) ) {
		fbdev_module.modes = vm->next;
		free( vm );
	}
}

/************************************************************************/
/*	console switching						*/
/************************************************************************/

void
fbdev_vt_switch( int activate )
{
	if( !cv->initialized )
		return;
	cv->has_console = activate;
	
	if( activate ) {
		if( cv->is_open )
			return;
		video_module_become( &fbdev_module );
	} else {
		if( !cv->is_open )
			return;
		video_module_yield( &fbdev_module, 0 /* may not fail */ );
		if( cv->is_open )
			printm("Internal error: console video is still open!\n");
	}
}

int
switch_to_fbdev_video( void ) 
{
	if( !cv->initialized )
		return -1;
	if( cv->is_open )
		return 0;

	if( !supports_vmode( &fbdev_module, NULL ) )
		return 1;
	console_to_front();
	return 0;
}

static int
go_console_key_action( int key, int val )
{
	switch_to_fbdev_video();
	return 1;
}

static key_action_t console_keys[] = {
	{ KEY_COMMAND, KEY_CTRL, KEY_ENTER,	go_console_key_action, 0 },
	{ KEY_COMMAND, KEY_CTRL, KEY_RETURN,	go_console_key_action, 1 },
	{ KEY_COMMAND, KEY_CTRL, KEY_SPACE,	go_console_key_action, 2 },
};

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static int
fbdev_init( video_module_t *m )
{
	char *name;

	memset( cv, 0, sizeof(cv[0]) );

	if( !get_bool_res("enable_console_video") )
		return 1;

	if( !(name=get_str_res("fbdev")) )
		name = "/dev/fb0";
	if( (cv->fd=open(name, O_RDWR)) < 0 ) {
		printm("Error opening frame buffer device '%s'\n",name );
		return 1;
	}
	fcntl( cv->fd, F_SETFD, FD_CLOEXEC );

	cv->startup_mode_ok = !ioctl( cv->fd, FBIOGET_VSCREENINFO, &cv->startup_var )
		&& !ioctl( cv->fd, FBIOGET_FSCREENINFO, &cv->startup_fix );
	if( !cv->startup_mode_ok )
		printm("Warning: Startup video mode query failed\n");

	if( !console_init() ) {
		close( cv->fd );
		return 1;
	}
	if( setup_video_modes() ) {
		printm("No usable console video modes were found\n");
		close( cv->fd );
		return 1;
	}
	
	cv->cmap.start = 0;
	cv->cmap.len = 256;
	cv->color_table = (short*)malloc( cv->cmap.len * sizeof(short) * 3 );
	cv->cmap.red = (unsigned short *) cv->color_table;
	cv->cmap.green = (unsigned short *) (cv->color_table + 256);
	cv->cmap.blue = (unsigned short *) (cv->color_table + 512);
	cv->cmap.transp = NULL;

	setup_gamma();

	cv->blank = 0;	/* VESA mode; assume power is initially on */

	if( (cv->use_cache = (get_bool_res("use_fb_cache") != 0)) )
		printm("Cache enabled for console-video\n");

	cv->initialized = 1;
	add_key_actions( console_keys, sizeof(console_keys) );
	return 0;
}

static void
fbdev_cleanup( video_module_t *m ) 
{
	if( cv->is_open )
		vclose();
	if( cv->fd != -1 )
		close( cv->fd );
	if( cv->color_table )
		free( cv->color_table );
	free_vmodes();

	console_cleanup();
}

/* Video module interface */
video_module_t fbdev_module  = {
	.name		= "console_video",
	.vinit		= fbdev_init,
	.vcleanup	= fbdev_cleanup,
	.vopen		= vopen,
	.vclose		= vclose,
	.setcmap	= setcmap,
	.vpowermode	= vpowermode,
};
