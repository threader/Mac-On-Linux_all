/* 
 *   Creation Date: <2003/01/16 00:16:53 samuel>
 *   Time-stamp: <2003/08/09 23:35:29 samuel>
 *   
 *	<xdga.c>
 *	
 *	X11 DGA 2.0 support
 *   
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/xf86dga.h>
#include "verbose.h"
#include "debugger.h"
#include "memory.h"
#include "mouse_sh.h"
#include "timer.h"
#include "res_manager.h"
#include "keycodes.h"
#include "input.h"
#include "video_module.h"
#include "video.h"
#include "x11.h"

static struct {
	int 		is_open;
	int		ev_base;
	int		err_base;

	int		initialized;
} dga;

/* DGA version requirement */
#define MIN_MAJOR	2
#define MIN_MINOR	0


/************************************************************************/
/*	enter / leave DGA						*/
/************************************************************************/

int
switch_to_xdga_video( void )
{
	if( dga.is_open )
		return 0;
	if( !dga.initialized )
		return -1;
	if( !supports_vmode( &xdga_module, NULL ) )
		return 1;
	video_module_become( &xdga_module );
	return 0;
}

static int
leave_xdga_key_action( int key, int num )
{
	if( dga.is_open )
		video_module_yield( &xdga_module, 0 /* may not fail */);
	else
		video_module_become( &xdga_module );
	return 1;
}

static key_action_t xdga_keys[] = {
	{ KEY_COMMAND, KEY_CTRL, KEY_ENTER,	leave_xdga_key_action, 1 },
	{ KEY_COMMAND, KEY_CTRL, KEY_RETURN,	leave_xdga_key_action, 2 },
	{ KEY_COMMAND, KEY_CTRL, KEY_SPACE,	leave_xdga_key_action, 3 },
};


/************************************************************************/
/*	MOL interface							*/
/************************************************************************/

static void
handle_event( XEvent *event )
{
	uint button;
	XKeyEvent kev;
	
	switch( event->type - dga.ev_base ) {
	case ButtonPress:
		button = ((XDGAButtonEvent*)event)->button;
		if( button < 4)	/* 1..3 */
			mouse_but_event( kMouseEvent_Down1 << (button-1) );
		break;

	case ButtonRelease:
		button = ((XDGAButtonEvent*)event)->button;
		if( button < 4)
			mouse_but_event( kMouseEvent_Up1 << (button-1) );
		break;

	case MotionNotify:
		mouse_move( ((XDGAMotionEvent*)event)->dx, ((XDGAMotionEvent*)event)->dy );
		break;

	case KeyPress:
	case KeyRelease:
		XDGAKeyEventToXKeyEvent( (XDGAKeyEvent*)event, &kev );
		kev.type = event->type - dga.ev_base;
		x11_key_event( &kev );
		break;
	}
}


static Bool
event_predicate( Display *disp, XEvent *ev, XPointer dummyarg )
{
	uint n = ev->type - dga.ev_base;
	return (n < XF86DGANumberEvents);
}

static void 
dga_vbl( void )
{
	XEvent event;

	while( XCheckIfEvent( x11.disp, &event, event_predicate, NULL ) )
		handle_event( &event );
}

static int
vopen( video_desc_t *vm )
{
	XDGADevice *dev;
	int mask;
	
	if( dga.is_open )
		return 1;

	if( !XDGAOpenFramebuffer( x11.disp, x11.screen ) ) {
		printm("XDGAOpenFramebuffer failed\n");
		return 1;
	}
	if( !(dev=XDGASetMode( x11.disp, x11.screen, (int)vm->module_data )) ) {
		printf("XDGASetMode failure\n");
		XDGACloseFramebuffer( x11.disp, x11.screen );
		return 1;
	}

	printm("%d x %d: %d (%08x/%x)\n", vm->w, vm->h, vm->depth, (int)dev->data, vm->rowbytes );
	vm->lvbase = (char *)dev->data;
	vm->mmu_flags = get_bool_res("use_fb_cache")? MAPPING_FORCE_CACHE : MAPPING_MACOS_CONTROLS_CACHE;
	vm->map_base = 0;
	XFree( dev );

	dga.is_open = 1;
	use_hw_cursor(0);

	mask = KeyPressMask | KeyReleaseMask;
	if( mouse_activate(1) )
		mask |= ButtonPressMask | ButtonReleaseMask | PointerMotionMask;

	XDGASelectInput( x11.disp, x11.screen, mask );
	return 0;
}

static void
vclose( void )
{
	if( !dga.is_open )
		return;

	dga.is_open = 0;

	mouse_activate(0);
	XDGASetMode( x11.disp, x11.screen, 0 );
	XDGACloseFramebuffer( x11.disp, x11.screen );
}

/* format is [R,G,B] * 256 */
static void 
setcmap( char *pal )
{
}

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static int
dga_error_handler( Display *disp, XErrorEvent *ev )
{
	/* we don't want to display the X11 error... */
	return 0;
}

static int
vinit( video_module_t *module )
{
	int n, i, major, minor, mcount, nodga;
	int (*old_ehandler)(Display*, XErrorEvent*);
	Display *dis = x11.disp;
	XDGAMode *mm;
	video_desc_t *m;
	
	memset( &dga, 0, sizeof(dga) );
	if( !x11.connected || get_bool_res("enable_xdga_video")!=1 )
		return 1;

	nodga = !XDGAQueryExtension(dis, &dga.ev_base, &dga.err_base);
	old_ehandler = XSetErrorHandler( dga_error_handler );
	nodga = nodga || !XDGAQueryVersion(dis, &major, &minor);
	XSetErrorHandler( old_ehandler );

	if( nodga ) {
		printm("X11 DGA is not available\n");
		return 1;
	}
	if( major < MIN_MAJOR || (major == MIN_MAJOR && minor < MIN_MINOR) ) {
		printm("Xserver is running DGA %d.%d (%d.%d is required)\n",
		       major, minor, MIN_MAJOR, MIN_MINOR );
		return 1;
	}
	printm("Running X11 DGA %d.%d\n\n", major, minor );

	if( !(mm=XDGAQueryModes(dis, x11.screen, &mcount)) ) {
		printm("XDGAQueryModes failed\n");
		return 1;
	}

	m = calloc( mcount, sizeof(video_desc_t) );
	for( n=0, i=0; i<mcount; i++ ) {
		int j, skip = 0;
		char *s;

		for( j=0; (s=get_str_res_ind("xdga_modes", 0, j)); j++ ) {
			skip = 1;
			if( i == strtol(s,NULL,0) )
				break;
		}
		if( mm[i].visualClass != DirectColor /*&& mm[i].visualClass != PseudoColor*/ )
			skip = 1;
		if( mm[i].bitsPerPixel == 24 || mm[i].depth == 16 || mm[i].depth != x11.xdepth )
			skip = 1;
		if( s )
			skip = 0;

		printm("  %c Mode %d: \"%s\": %dx%d %2d (%dx%d) %.2f Hz %04x %d %d\n",
			skip ? ' ' : '*', mm[i].num, mm[i].name,
			mm[i].imageWidth, mm[i].imageHeight,
			mm[i].depth, mm[i].pixmapWidth, mm[i].pixmapHeight,
			mm[i].verticalRefresh,
			/*mm[i].viewportWidth, mm[i].viewportHeight, */
			mm[i].flags, mm[i].bytesPerScanline, mm[i].visualClass
		);

		if( skip )
			continue;
		
		/* add mode */
		m[n].offs = 0;	/* fixme! */
		m[n].rowbytes = mm[i].bytesPerScanline;
		m[n].w = mm[i].viewportWidth;
		m[n].h = mm[i].viewportHeight;
		m[n].depth = mm[i].depth;
		m[n].refresh = (mm[i].verticalRefresh * 65536) + 0.5;
               m[n].module_data = (void*)mm[i].num;
		m[n].next = NULL;
		if( n>0 )
			m[n-1].next = &m[n];
		n++;
	}
	printm("\n");
	xdga_module.modes = &m[0];
	XFree( mm );

	add_key_actions( xdga_keys, sizeof(xdga_keys) );
	dga.initialized = 1;
	return 0;
}

static void
vcleanup( video_module_t *m )
{
	if( dga.is_open )
		vclose();

	/* is this safe? */
	if( xdga_module.modes ) {
		free( xdga_module.modes );
		xdga_module.modes = NULL;
	}
	dga.initialized = 0;
}

/* Video module interface */
video_module_t xdga_module  = {
	.name		= "X11-dga",
	.vinit		= vinit,
	.vcleanup	= vcleanup,
	.vopen		= vopen,
	.vclose		= vclose,
	.setcmap	= setcmap,

	.vbl		= dga_vbl
};
