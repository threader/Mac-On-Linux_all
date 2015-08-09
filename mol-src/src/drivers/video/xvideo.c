/* 
 *   Creation Date: <1999/12/21 20:40:23 samuel>
 *   Time-stamp: <2004/06/12 18:27:23 samuel>
 *   
 *	<xvideo.c>
 *	
 *	X-windows video driver
 *	
 *	Derived from the Basillisk II X-driver, 
 *	(C) 1997-1999 Christian Bauer
 *   
 *   Adapted for MOL by Samuel Rydh, 1999, 2002, 2003 (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <X11/Xatom.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>
#include "driver_mgr.h"

#include "verbose.h"
#include "debugger.h"
#include "res_manager.h"
#include "memory.h"
#include "mouse_sh.h"
#include "wrapper.h"
#include "thread.h"
#include "video_module.h"
#include "keycodes.h"
#include "booter.h"
#include "video.h"
#include "molcpu.h"
#include "checksum.h"
#include "input.h"
#include "timer.h"
#include "byteorder.h"
#include "x11.h"

/* #define USE_ROOT_WINDOW */
/* #define CALC_FRAMERATE */

/* Little endian X-server seems to be able to handle big endian 24 */ 
#define CLIENT_LE24_SWAP

typedef unsigned char	uint8;
typedef int		bool;

#define false		False
#define true		True

static struct {
	/* cursor data */
#if 0
	XImage 		*cursor_image, *cursor_mask_image;
	Pixmap 		cursor_map, cursor_mask_map;
	Cursor 		mac_cursor;
	GC 		cursor_gc, cursor_mask_gc;
	uint8 		the_cursor[64];			/* Cursor image data */
#endif
	Cursor		no_x_cursor;
	Pixmap		no_cursor_bitmap;
	int		use_x_cursor;

	/* palette */
	Colormap	cmap;				/* Colormaps for 8-bit mode */
	XColor		palette[256];			/* Color palette for 8-bit mode */
	int		palette_changed;		/* Flag: Palette changed */

	/* variables for window mode */
	Window 		win;
	GC 		the_gc;
	int		is_mapped;
	XImage 		*img;
	XShmSegmentInfo	shminfo;

	/* framebuffer info */
	int		is_open;
	uint8		*the_buffer;			/* Mac frame buffer */
	int		have_shm;			/* Flag: SHM extensions available */
	int		force_redraw;

	/* redraw Checksum */
	video_cksum_t	*checksum;

	/* redraw timer */
	ulong 		vbl_period;

	/* depth conversion */
	int		depth_emulation;		/* in depth emulation mode */
	char		*offscreen_buf;
	ulong		*depth_blit_palette;		/* 256 byte table (8->24/32 pixel value conversion) */

	/* hooks for depth and endian conversions */
	void		(*blit_hook_pre)( int x, int y, int w, int h );
	void		(*blit_hook_post)( int x, int y, int w, int h );
} vs;

static video_desc_t	vmode;				/* virtual vmode (might have a different depth) */
static video_desc_t	mac_vmode;			/* video mode the mac expects */

static const int 	kEventMask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask 
					| PointerMotionMask | EnterWindowMask | ExposureMask
					| StructureNotifyMask;

static unsigned char	no_cursor_data[] = { 0, 0, 0, 0, 0, 0, 0, 0 };


/************************************************************************/
/*	support for various video mode conversions			*/
/************************************************************************/

#ifndef CLIENT_LE24_SWAP
static void
endian_conv_32( int x, int y, int w, int h ) 
{
	char *pp, *start = vmode.lvbase + vmode.offs + vmode.rowbytes * y + x*4;
	int i, ww = (w & ~3);

	pp = start;
	for( ; h-- > 0 ; pp += vmode.rowbytes ) {
		ulong *p = (ulong*)pp;
		if( w & 1 ) {
			p[0] = ld_le32( p );
			p++;
		}
		if( w & 2 ) {
			p[0] = ld_le32( p );
			p[1] = ld_le32( p+1 );
			p+=2;
		}
		for( i=0; i<ww; i+=4, p+=4 ) {
			p[0] = ld_le32( p );
			p[1] = ld_le32( p+1 );
			p[2] = ld_le32( p+2 );
			p[3] = ld_le32( p+3 );
		}
	}
}
#endif

static void
endian_conv_15( int x, int y, int w, int h ) 
{
	char *pp, *start = vmode.lvbase + vmode.offs + vmode.rowbytes * y + x*2;
	int i, ww = (w & ~3);

	pp = start;
	for( ; h-- > 0 ; pp += vmode.rowbytes ) {
		ushort *p = (ushort*)pp;
		if( w & 1 ) {
			p[0] = ld_le16( p );
			p++;
		}
		if( w & 2 ) {
			p[0] = ld_le16( p );
			p[1] = ld_le16( p+1 );
			p+=2;
		}
		for( i=0; i<ww; i+=4, p+=4 ) {
			p[0] = ld_le16( p );
			p[1] = ld_le16( p+1 );
			p[2] = ld_le16( p+2 );
			p[3] = ld_le16( p+3 );
		}
	}
}

#ifdef __ppc__
static inline void
conv_15_to_le16( u16 *p, u32 x )
{
	asm ("rlwimi %0,%0,1,16,25" : "+r" (x) : );
	asm ("rlwinm %0,%1,0,27,25" : "=r" (x) : "r" (x) );
	asm ("sthbrx %1,0,%2" : "=m" (*p) : "r" (x), "r" (p) );
}

static inline u32
conv_le16_to_15( u16 *p )
{
	u32 r;
	asm ("lhbrx %0,0,%1" : "=r" (r) : "r" (p), "m" (*p) );
	asm ("rlwimi %0,%0,31,17,26" : "+r" (r) : );
	asm ("rlwinm %0,%1,0,17,31" : "=r" (r) : "r" (r) );
	return r;
}
#else
/* fixme */
static inline void conv_15_to_le16( u16 *p, u32 x ) { *p = x; }
static inline u32 conv_le16_to_15( u16 *p ) { return *p; }
#endif

static void
endian_conv_16( int x, int y, int w, int h ) 
{
	char *pp, *start = vmode.lvbase + vmode.offs + vmode.rowbytes * y + x*2;
	int i, ww = (w & ~3);

	pp = start;
	for( ; h-- > 0 ; pp += vmode.rowbytes ) {
		ushort *p = (ushort*)pp;
		if( w & 1 ) {
			conv_15_to_le16( p, p[0] );
			p++;
		}
		if( w & 2 ) {
			conv_15_to_le16( p, p[0] );
			conv_15_to_le16( p+1, p[1] );
			p+=2;
		}
		for( i=0; i<ww; i+=4, p+=4 ) {
			conv_15_to_le16( p, p[0] );
			conv_15_to_le16( p+1, p[1] );
			conv_15_to_le16( p+2, p[2] );
			conv_15_to_le16( p+3, p[3] );
		}
	}
}

static void
endian_conv_16b( int x, int y, int w, int h ) 
{
	char *pp, *start = vmode.lvbase + vmode.offs + vmode.rowbytes * y + x*2;
	int i, ww = (w & ~3);

	pp = start;
	for( ; h-- > 0 ; pp += vmode.rowbytes ) {
		u16 *p = (u16*)pp;
		if( w & 1 ) {
			p[0] = conv_le16_to_15( p );
			p++;
		}
		if( w & 2 ) {
			p[0] = conv_le16_to_15( p );
			p[1] = conv_le16_to_15( p+1 );
			p+=2;
		}
		for( i=0; i<ww; i+=4, p+=4 ) {
			p[0] = conv_le16_to_15( p );
			p[1] = conv_le16_to_15( p+1 );
			p[2] = conv_le16_to_15( p+2 );
			p[3] = conv_le16_to_15( p+3 );
		}
	}
}

#ifdef __ppc__
static inline ulong
conv_15_to_16( ulong x )
{
	asm ("rlwimi %0,%0,1,16,25" : "+r" (x) : );
	asm ("rlwinm %0,%1,0,27,25" : "=r" (x) : "r" (x) );
	return x;
}

static inline ulong
conv_16_to_15( ulong x )
{
	asm ("rlwimi %0,%0,31,17,26" : "+r" (x) : );
	asm ("rlwinm %0,%1,0,17,31" : "=r" (x) : "r" (x) );
	return x;
}
#else
/* fixme */
static inline u32 conv_15_to_16( u32 x ) { return x; }
static inline u32 conv_16_to_15( u32 x ) { return x; }
#endif

static void
convert_555_to_565( int x, int y, int w, int h ) 
{
	char *pp, *start = vmode.lvbase + vmode.offs + vmode.rowbytes * y + x*2;
	int i, ww = (w & ~3);

	pp = start;
	for( ; h-- > 0 ; pp += vmode.rowbytes ) {
		ushort *p = (ushort*)pp;
		if( w & 1 ) {
			p[0] = conv_15_to_16( p[0] );
			p++;
		}
		if( w & 2 ) {
			p[0] = conv_15_to_16( p[0] );
			p[1] = conv_15_to_16( p[1] );
			p+=2;
		}
		for( i=0; i<ww; i+=4, p+=4 ) {
			p[0] = conv_15_to_16( p[0] );
			p[1] = conv_15_to_16( p[1] );
			p[2] = conv_15_to_16( p[2] );
			p[3] = conv_15_to_16( p[3] );
		}
	}
}

static void
convert_565_to_555( int x, int y, int w, int h ) 
{
	char *pp, *start = vmode.lvbase + vmode.offs + vmode.rowbytes * y + x*2;
	int i, ww = (w & ~3);

	pp = start;
	for( ; h-- > 0 ; pp += vmode.rowbytes ) {
		ushort *p = (ushort*)pp;
		if( w & 1 ) {
			p[0] = conv_16_to_15( p[0] );
			p++;
		}
		if( w & 2 ) {
			p[0] = conv_16_to_15( p[0] );
			p[1] = conv_16_to_15( p[1] );
			p+=2;
		}
		for( i=0; i<ww; i+=4, p+=4 ) {
			p[0] = conv_16_to_15( p[0] );
			p[1] = conv_16_to_15( p[1] );
			p[2] = conv_16_to_15( p[2] );
			p[3] = conv_16_to_15( p[3] );
		}
	}
}

static void
depth_blit_8_to_32( int x, int y, int w, int h )
{
	char *pp, *start = vmode.lvbase + vmode.offs + vmode.rowbytes * y + x*4;
	char *src = mac_vmode.lvbase + mac_vmode.offs + mac_vmode.rowbytes *y + x;
	ulong *pal = vs.depth_blit_palette;
	int i, ww = (w & ~3);

	pp = start;
	for( ; h-- > 0 ; pp += vmode.rowbytes, src += mac_vmode.rowbytes ) {
		ulong *p = (ulong*)pp;
		unsigned char *s = (unsigned char*)src;
		if( w & 1 ) {
			p[0] = pal[s[0]];
			p++; s++;
		}
		if( w & 2 ) {
			p[0] = pal[s[0]];
			p[1] = pal[s[1]];
			p+=2;
			s+=2;
		}
		for( i=0; i<ww; i+=4, p+=4, s+=4 ) {
			p[0] = pal[s[0]];
			p[1] = pal[s[1]];
			p[2] = pal[s[2]];
			p[3] = pal[s[3]];
		}
	}
}

static void
depth_blit_8_to_16( int x, int y, int w, int h )
{
	char *pp, *start = vmode.lvbase + vmode.offs + vmode.rowbytes * y + x*2;
	char *src = mac_vmode.lvbase + mac_vmode.offs + mac_vmode.rowbytes *y + x;
	ulong *pal = vs.depth_blit_palette;
	int i, ww = (w & ~3);

	pp = start;
	for( ; h-- > 0 ; pp += vmode.rowbytes, src += mac_vmode.rowbytes ) {
		ushort *p = (ushort*)pp;
		unsigned char *s = (unsigned char*)src;
		if( w & 1 ) {
			p[0] = pal[s[0]];
			p++; s++;
		}
		if( w & 2 ) {
			p[0] = pal[s[0]];
			p[1] = pal[s[1]];
			p+=2;
			s+=2;
		}
		for( i=0; i<ww; i+=4, p+=4, s+=4 ) {
			p[0] = pal[s[0]];
			p[1] = pal[s[1]];
			p[2] = pal[s[2]];
			p[3] = pal[s[3]];
		}
	}
}


/************************************************************************/
/*	misc								*/
/************************************************************************/

static void
vrefresh( void )
{
	if( !vs.is_open )
		return;

	vs.force_redraw = 1;
	/* XClearArea( x11.disp, vs.win, 0,0,0,0, True ); */
}


static void 
set_depth_blit_pentry( ulong *dest, char *p )
{
	/* p points to three chars (R,G,B) */

	if( std_depth(x11.xdepth) == std_depth(32) ) {
		*dest = (x11.byte_order == MSBFirst) ?
			((ulong)*p<<16) | ((ulong)*(p+1)<<8) | *(p+2) :
			((ulong)*(p+2)<<24) | ((ulong)*(p+1)<<16) | (*p<<8);
	} else {
		if( !x11.is_565 ) {
			*dest = (((ulong)*p<<(10-3)) & 0x7c00)
				| (((ulong)*(p+1)<<(5-3)) & 0x3e0)
				| (((ulong)*(p+2)>>3) & 0x1f);
		} else {
			/* 565 pixels */
			*dest = (((ulong)*p<<(11-3)) & 0xf800)
				| (((ulong)*(p+1)<<(5-2)) & 0x7e0)
				| (((ulong)*(p+2)>>3) & 0x1f);
		}
		if( x11.byte_order == LSBFirst)
			*dest = ((*dest & 0xff) << 8) | ((*dest & 0xff00) >> 8);
	}
}

static void 
setcmap( char *pal )
{
	int i;

	if( !vs.is_open )
		return;

	/* convert colors to XColor array */
	for( i=0; i<256; i++ ) {
		vs.palette[i].pixel = i;
		vs.palette[i].red = pal[i*3] * 0x0101;
		vs.palette[i].green = pal[i*3+1] * 0x0101;
		vs.palette[i].blue = pal[i*3+2] * 0x0101;
		vs.palette[i].flags = DoRed | DoGreen | DoBlue;
	}

	/* setup depth emulation tables */
	if( vs.depth_blit_palette ) {
		for( i=0; i<256; i++ )
			set_depth_blit_pentry( &vs.depth_blit_palette[i], &pal[i*3] );
		vs.force_redraw = 1;
	}
	vs.palette_changed = 1;
}


/************************************************************************/
/*	framerate calculation						*/
/************************************************************************/

#ifdef CALC_FRAMERATE
#define DBG_NEW_FRAME		calc_framerate();
#define DBG_FRAME_ACTIVE	dbg_fcount++

static int dbg_fcount=0;

static void
calc_framerate( void )
{
	struct timeval tv;
	static struct timeval old_tv;
	static int count=0;
	
	gettimeofday( &tv, NULL );
	count++;
	if( old_tv.tv_sec != tv.tv_sec ) {
		printm("Framerate: %d/%d\n", count, dbg_fcount );
		count=0;
		dbg_fcount=0;
		old_tv = tv;
	}
}
#else
#define DBG_NEW_FRAME		do {} while(0)
#define DBG_FRAME_ACTIVE	do {} while(0)
#endif /* CALC_FRAMERATE */


/************************************************************************/
/*	refresh and event handling (VBL)				*/
/************************************************************************/

static void
blit_rect( int x, int y, int w, int h )
{
	if( vs.blit_hook_pre )
		vs.blit_hook_pre( x, y, w, h );

	if( vs.have_shm ) 
		XShmPutImage( x11.disp, vs.win, vs.the_gc, vs.img, x, y, x, y,
			      w, h, 0/*send_event*/ );
	else
		XPutImage( x11.disp, vs.win, vs.the_gc, vs.img, x, y, x, y, w, h );

	XSync( x11.disp, 0);
	if( vs.blit_hook_post )
		vs.blit_hook_post( x, y, w, h );
}

static void
update_display( void )
{
	short buf[80];
	int i, n, flush=0;
#if 0
	static int num_events=0;
	XEvent ev;
	if( num_events ) {
		if( !XCheckTypedEvent( x11.disp, XShmGetEventBase(x11.disp) + ShmCompletion, &ev ) ) {
			/* printm("Frame skipped\n"); */
			return;
		}
		num_events--;
	}
#endif
	n = _get_dirty_fb_lines( buf, sizeof(buf) );

	if( vs.force_redraw ) {
		n=0;
		vs.force_redraw = 0;

		if( vs.checksum )
			vcksum_calc( vs.checksum, 0, vmode.h );

		blit_rect( 0,0, vmode.w, vmode.h );
		flush=1;
	}

	for( i=0; i<n; i++ ) {
		int y, h;

		y = buf[i*2];
		h = buf[i*2+1] - buf[i*2] +1;

		if( vs.checksum ) {
			vcksum_calc( vs.checksum, y, h );
			vcksum_redraw( vs.checksum, y, h, &blit_rect );
			/* vcksum_redraw( vs.checksum, y, h, &depth_blit_rec ); */
		} else {
			blit_rect( 0, y, vmode.w, h );
			flush=1;
		}
	}

	if( flush ) {
		XFlush( x11.disp );
		sched_yield();
		DBG_FRAME_ACTIVE;
	}
#if 0
	if( n ) {
		num_events++;
	}
#endif
}

static inline int
switch_mouse_buts( int but )
{
	/* interchange button 2 and button 3 */
	return (but == 1)? 0 : (but^1)-1;
}

static void
handle_events( XEvent *event )
{
	uint but;
	
	switch( event->type ) {
	case ButtonPress:
		if( (but=switch_mouse_buts(((XButtonEvent*)event)->button)) <= 2 )
			mouse_but_event( kMouseEvent_Down1 << but );
		break;

	case ButtonRelease:
		if( (but=switch_mouse_buts(((XButtonEvent*)event)->button)) <= 2 );
			mouse_but_event( kMouseEvent_Up1 << but );
		break;

	case EnterNotify:
	case MotionNotify:
		mouse_move_to( ((XMotionEvent *)event)->x, ((XMotionEvent *)event)->y,
			       mac_vmode.w, mac_vmode.h );
		break;

	case KeyPress:
	case KeyRelease:
		x11_key_event( (XKeyEvent*)event );
		break;

	case Expose:
		#define ev ((XExposeEvent*)event)
		if( ev->window != vs.win )
			break;
		if( ev->x < 0 || ev->y < 0 || ev->x + ev->width > vmode.w || ev->y + ev->height > vmode.h ) {
			printm("Bad Expose event, %d %d %d %d\n", ev->x, ev->y, ev->width, ev->height );
			ev->x = ev->y = 0;
			ev->width = vmode.w;
			ev->height = vmode.h;
		}
		blit_rect( ev->x, ev->y, ev->width, ev->height );
		break;
		#undef ev

	case MapNotify:
		vs.is_mapped = 1;
		break;

	case UnmapNotify:
		vs.is_mapped = 0;
		break;
	}
}

static void
xvideo_vbl( void ) 
{
	XEvent event;

	if( !vs.is_open )
		return;
	
	DBG_NEW_FRAME;

	if( vs.palette_changed ) {
		vs.palette_changed = 0;
		if( x11.xdepth == 8 )
			XStoreColors( x11.disp, vs.cmap, vs.palette, 256 );
	}
	
	update_display();

	while( XCheckWindowEvent( x11.disp, vs.win, kEventMask, &event) )
		handle_events( &event );
}

/************************************************************************/
/*	vopen / vclose							*/
/************************************************************************/

/* Trap SHM errors */
static bool	shm_error;
static int	(*old_error_handler)(Display *, XErrorEvent *);

static int 
shm_error_handler( Display *d, XErrorEvent *e )
{
	if( e->error_code == BadAccess ) {
		shm_error = true;
		return 0;
	}
	return old_error_handler(d, e);
}

/* this is called to set video mode and prepare buffers. Supposedly,
 * any offset or row_bytes value should work
 */
static int
vopen( video_desc_t *org_vm )
{
	Display *disp = x11.disp;
	XSizeHints *hints;

	if( vs.is_open ) {
		printm("vopen called twice!\n");
		return 1;
	}

	XSync( disp, false );
	vmode = *org_vm;

	vs.depth_emulation = 0;
	vs.blit_hook_pre = NULL;
	vs.blit_hook_post = NULL;

	if( std_depth(x11.xdepth) != std_depth(vmode.depth) ) {
		/* Use a X depth emulation mode... */
		if( (std_depth(x11.xdepth) != std_depth(15) && std_depth(x11.xdepth) != std_depth(32)) 
		    || (vmode.depth != 8))
		{
			printm("Bad vmode depth %d (x11.xdepth = %d)!\n", vmode.depth, x11.xdepth );
			return 1;
		}

		if( std_depth(x11.xdepth) == std_depth(32) )
			vs.blit_hook_pre = depth_blit_8_to_32;
		else
			vs.blit_hook_pre = depth_blit_8_to_16;
		
		vmode.rowbytes = vmode.w;
		switch( x11.xdepth ){
		case 1:
			vmode.rowbytes /= 8; 
			break;
		case 15: 
		case 16:
			vmode.rowbytes *= 2;
			break;
		case 24:
		case 32:
			vmode.rowbytes *= 4; 
			break;
		}
		vmode.depth = x11.xdepth;
		vmode.offs = 0;
		vs.depth_emulation = 1;
		if( vs.offscreen_buf || vs.depth_blit_palette )
			printm("Internal error in xvideo.c (unreleased resource)\n");

		vs.offscreen_buf = map_zero( NULL, FBBUF_SIZE(org_vm));
		vs.depth_blit_palette = calloc( 256, sizeof(ulong) );

	} else if( x11.byte_order != MSBFirst ) {
		/* endian conversion? */
		switch( vmode.depth ) {
		case 15:
		case 16:
			if( x11.is_565 ) {
				vs.blit_hook_pre = endian_conv_16;
				vs.blit_hook_post = endian_conv_16b;
			} else {
				vs.blit_hook_pre = vs.blit_hook_post = endian_conv_15;
			}
			break;
#ifndef CLIENT_LE24_SWAP
		case 24: 
		case 32:
			vs.blit_hook_pre = vs.blit_hook_post = endian_conv_32;
			break;
#endif
		}
	} else if( x11.is_565 ) {
		/* depth conversion 555 -> 565 */
		vs.blit_hook_pre = convert_555_to_565;
		vs.blit_hook_post = convert_565_to_555;
	}

	/* Try to create and attach SHM image */
	vs.have_shm = 0;
	if( vmode.depth != 1 && XShmQueryExtension(disp) ){

		/* Manipulating the bytes_per_line field in the XImage structure has no effect. Thus
		 * we need to set the width such that the correct row_bytes value is obtained.
		 */
		int fwidth = vmode.rowbytes;
		switch( x11.xdepth ){
		case 1: 		
			fwidth *= 8; break;
		case 15: 
		case 16: 	
			fwidth /= 2; break;
		case 24: 
		case 32:
			fwidth /= 4; break;
		}
		/* create SHM image */
		vs.img = XShmCreateImage( disp, x11.vis, x11.xdepth, (x11.xdepth == 1)? XYBitmap : ZPixmap,
					  0, &vs.shminfo, fwidth, vmode.h );

		if( vs.img->bytes_per_line != vmode.rowbytes ) {
			printm("row_bytes mismatch, %d != %d.\n", vmode.rowbytes, vs.img->bytes_per_line );
			XDestroyImage( vs.img );
		} else {
			vs.shminfo.shmid = shmget( IPC_PRIVATE, FBBUF_SIZE(&vmode), IPC_CREAT | 0777 );
			vs.the_buffer = (uint8*)shmat( vs.shminfo.shmid, 0, 0 );
			vs.shminfo.shmaddr = (char*)vs.the_buffer;
			vs.img->data = (char*)vs.the_buffer + vmode.offs;
			vs.shminfo.readOnly = True /*False*/;

			/* Try to attach SHM image, catching errors */
			shm_error = false;
			old_error_handler = XSetErrorHandler( shm_error_handler );
			XShmAttach( disp, &vs.shminfo );
			XSync( disp, false );
			XSetErrorHandler( old_error_handler );

			if( shm_error ) {
				shmdt( vs.shminfo.shmaddr );
				vs.img->data = NULL;
				XDestroyImage( vs.img );
				vs.shminfo.shmid = -1;
			} else {
				vs.have_shm = 1;
				shmctl( vs.shminfo.shmid, IPC_RMID, 0 );
			}
		}
	}

	/* create normal X image if SHM doesn't work */
	if( !vs.have_shm ) {
		vs.the_buffer = (uint8 *)map_zero( NULL, FBBUF_SIZE(&vmode) );
		vs.img = XCreateImage(disp, x11.vis, x11.xdepth, x11.xdepth == 1 ? XYBitmap : ZPixmap, 
				      0, (char *)vs.the_buffer + vmode.offs, vmode.w, vmode.h, 
				      32, vmode.rowbytes );
#ifdef CLIENT_LE24_SWAP
		if( vmode.depth == 24 || vmode.depth == 32 )
			vs.img->byte_order = MSBFirst;
#endif
	}

	/* 1-Bit mode is big-endian */
	if( x11.xdepth == 1 ) {
		vs.img->byte_order = MSBFirst;
		vs.img->bitmap_bit_order = MSBFirst;
	}

	/* make window unresizable */
	if( (hints = XAllocSizeHints()) ) {
		hints->min_width = 100;
		hints->max_width = vmode.w;
		hints->min_height = 100;
		hints->max_height = vmode.h;
		hints->flags = PMinSize | PMaxSize;
		XSetWMNormalHints(disp, vs.win, hints);
		XFree((char *)hints);
	}
	XResizeWindow( disp, vs.win, vmode.w, vmode.h );

	/* hide Mac-cursor */
	use_hw_cursor( vs.use_x_cursor );

	/* fill in the fields video.c expects */
	org_vm->mmu_flags = MAPPING_FB_ACCEL | MAPPING_FORCE_CACHE;
	org_vm->map_base = 0;
	if( !vs.depth_emulation ) {
		vmode.lvbase = org_vm->lvbase = (char *)vs.the_buffer;
	} else {
		vmode.lvbase = (char *)vs.the_buffer;
		org_vm->lvbase = vs.offscreen_buf;
	}
	mac_vmode = *org_vm;

	/* set MMU acceleration parameters */
	_setup_fb_accel( mac_vmode.lvbase+mac_vmode.offs, mac_vmode.rowbytes, mac_vmode.h );

	/* setup checksum */
	if( get_bool_res("use_xchecksum") != 0 )
		vs.checksum = alloc_vcksum( &mac_vmode );
	else {
		vs.checksum = NULL;
		printm("xvideo checksum disabled\n");
	}

	/* refresh and sync */
	vs.force_redraw = 1;

#ifdef USE_ROOT_WINDOW
	XLowerWindow( disp, vs.win );
#endif
	XMapWindow( disp, vs.win );

	XSync( disp, false );
	vs.is_open = 1;
	return 0;
}

static void
vclose( void )
{
	if( !vs.is_open )
		return;

	vs.is_open = 0;

	XUnmapWindow( x11.disp, vs.win );
	vs.is_mapped = 0;

	XSync( x11.disp, false );

	_setup_fb_accel( NULL, 0, 0 );

	if( vs.have_shm ) {
		/* is this the correct sequence? */
		XShmDetach( x11.disp, &vs.shminfo );
		vs.img->data = NULL;
		XDestroyImage( vs.img );
		shmdt( vs.shminfo.shmaddr );
	} else {
		munmap( vs.the_buffer, FBBUF_SIZE(&vmode) );
		vs.img->data = NULL;
		XDestroyImage( vs.img );
	}

	/* free depth blitting tables */
	if( vs.offscreen_buf ) {
		munmap( vs.offscreen_buf, FBBUF_SIZE(&mac_vmode) );
		vs.offscreen_buf = NULL;
	}
	if( vs.depth_blit_palette ) {
		free( vs.depth_blit_palette );
		vs.depth_blit_palette = NULL;
	}

	/* and force a screen refresh */
	XClearArea( x11.disp, vs.win, 0,0,0,0, True );
	XSync( x11.disp, false );
	
	if( vs.checksum )
		free_vcksum( vs.checksum );
}

/************************************************************************/
/*	FIX THE CURSOR CODE!						*/
/************************************************************************/

#if 0
static void
cursor_setter( void )
{
	/* Has the Mac started? (cursor data is not valid otherwise) */
	if( /*HasMacStarted()*/ 1) {
		/* set new cursor image if it was changed */
		if( memcmp(the_cursor, EA_TO_LVPTR(0x844), 64) ) {
			memcpy( the_cursor, EA_TO_LVPTR(0x844), 64 );
			memcpy( cursor_image->data, the_cursor, 32 );
			memcpy( cursor_mask_image->data, the_cursor+32, 32 );
			XFreeCursor( x11.disp, mac_cursor);
			XPutImage( x11.disp, cursor_map, cursor_gc, cursor_image, 0, 0, 0, 0, 16, 16);
			XPutImage( x11.disp, cursor_mask_map, cursor_mask_gc, cursor_mask_image, 0, 0, 0, 0, 16, 16);
			mac_cursor = XCreatePixmapCursor( x11.disp, cursor_map, cursor_mask_map, &x11.black, &x11.white, 
							*EA_TO_LVPTR(0x885), *EA_TO_LVPTR(0x887) );
			XDefineCursor(x11.disp, vs.win, mac_cursor);
		}
	}
}
#endif

/************************************************************************/
/*	enter/leave full screen mode					*/
/************************************************************************/

static void
netwm_fullscreen(int enable) {
	XEvent e;
	Display *display = x11.disp;
	Window  window = vs.win;
	int operation = enable ? 1 : 0;
	Atom atom_fullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN",  False);
	Atom atom_state      = XInternAtom(display, "_NET_WM_STATE",  False);

	memset(&e,0,sizeof(e));
	e.xclient.type = ClientMessage;
	e.xclient.message_type = atom_state;
	e.xclient.display = display;
	e.xclient.window = window;
	e.xclient.format = 32;
	e.xclient.data.l[0] = operation;
	e.xclient.data.l[1] = atom_fullscreen;

	XSendEvent(display, DefaultRootWindow(display), False,
		SubstructureRedirectMask, &e);
}


static int
toggle_xv_fullscreen_key_action( int key, int num )
{
	x11.full_screen = !x11.full_screen;

	printf("XV: Toggling full screen to: %s\n", x11.full_screen ? "on" : "off");

	if (x11.full_screen) {
		netwm_fullscreen(1);
		XGrabKeyboard(x11.disp, vs.win, True, GrabModeAsync, GrabModeAsync, CurrentTime);
	} else {
		netwm_fullscreen(0);
		XUngrabKeyboard(x11.disp, CurrentTime);
	}
	XSync( x11.disp, false );

	return 1;
}


static key_action_t xv_keys[] = {
	{ KEY_COMMAND, KEY_CTRL, KEY_ENTER,	toggle_xv_fullscreen_key_action, 1 },
	{ KEY_COMMAND, KEY_CTRL, KEY_RETURN,	toggle_xv_fullscreen_key_action, 2 },
	{ KEY_COMMAND, KEY_CTRL, KEY_SPACE,	toggle_xv_fullscreen_key_action, 3 },
};


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static bool 
init_window( int width, int height )
{
	char *window_name = "Mac-on-Linux";
	char *icon_name = "MOL";
	XSetWindowAttributes wattr;
	XTextProperty winName, iconName;
	XWMHints *wm_hints;
	XClassHint *class_hints;
	XSizeHints *size_hints;

	/* create window */
	memset( &wattr, 0, sizeof(wattr) );
	wattr.event_mask = kEventMask;
	wattr.background_pixel = x11.white_pixel;
	wattr.border_pixel = x11.black_pixel;
	wattr.backing_store = NotUseful;
	if( get_bool_res("use_backing_store")==1 ) {
		printm("Using backing store for X-window\n");
		wattr.backing_store = WhenMapped; 	/* better than Always, I think */
	}
	wattr.backing_planes = x11.xdepth;
#ifdef USE_ROOT_WINDOW
	wattr.override_redirect = True;
#else
	wattr.override_redirect = False;
#endif
	XSync( x11.disp, false );
	vs.win = XCreateWindow( x11.disp, x11.rootwin, 0, 0, width, height, 0, x11.xdepth, InputOutput, x11.vis,
				CWEventMask | CWBackPixel | CWBorderPixel | CWOverrideRedirect 
				| CWBackingStore | CWBackingPlanes, &wattr );

	/* setup properties */
	wm_hints = XAllocWMHints();
	class_hints = XAllocClassHint();
	size_hints = XAllocSizeHints();
	XStringListToTextProperty( &window_name, 1, &winName );
	XStringListToTextProperty( &icon_name, 1, &iconName );
	wm_hints->initial_state = NormalState;
	wm_hints->input = True;
	/* wm_hints->icon_pixmap = mol_icon; */
	wm_hints->flags = StateHint | InputHint /* | IconPixmapHint */;
	class_hints->res_name = "mol";
	class_hints->res_class = "Mol";
	size_hints->flags = PPosition | PSize | PMinSize;
	size_hints->min_width = 100;
	size_hints->min_height = 100;
	XSetWMProperties( x11.disp, vs.win, &winName, &iconName, NULL, 0,
			  size_hints, wm_hints, class_hints );
	XFree( winName.value );
	XFree( iconName.value );
	XFree( wm_hints );
	XFree( class_hints );
	XFree( size_hints );

	/* XStoreName(x11.disp, vs.win, "Mac-on-Linux" ); */

	/* set colormap */
	if( x11.xdepth == 8 ) {
		XSetWindowColormap( x11.disp, vs.win, vs.cmap );
		XSetWMColormapWindows( x11.disp, vs.win, &vs.win, 1 );
	}

	/* create GC */
	vs.the_gc = XCreateGC( x11.disp, vs.win, 0, 0 );
	XSetState( x11.disp, vs.the_gc, x11.black_pixel, x11.white_pixel, GXcopy, AllPlanes );

#if 0
	/* create cursor */
	cursor_image = XCreateImage( x11.disp, x11.vis, 1, XYPixmap, 0,
				     (char *)the_cursor, 16, 16, 16, 2 );
	cursor_image->byte_order = MSBFirst;
	cursor_image->bitmap_bit_order = MSBFirst;
	cursor_mask_image = XCreateImage( x11.disp, x11.vis, 1, XYPixmap, 0,
					  (char *)the_cursor+32, 16, 16, 16, 2 );
	cursor_mask_image->byte_order = MSBFirst;
	cursor_mask_image->bitmap_bit_order = MSBFirst;
	cursor_map = XCreatePixmap( x11.disp, vs.win, 16, 16, 1);
	cursor_mask_map = XCreatePixmap( x11.disp, vs.win, 16, 16, 1);
	cursor_gc = XCreateGC( x11.disp, cursor_map, 0, 0);
	cursor_mask_gc = XCreateGC( x11.disp, cursor_mask_map, 0, 0);
	mac_cursor = XCreatePixmapCursor( x11.disp, cursor_map, cursor_mask_map,	
					  &x11.black, &x11.white, 0, 0);
#endif

	if( !vs.use_x_cursor ) {
		vs.no_cursor_bitmap = XCreateBitmapFromData( x11.disp, vs.win, (char *)no_cursor_data, 8,8);
		if( vs.no_cursor_bitmap != None ){
			vs.no_x_cursor = XCreatePixmapCursor( x11.disp, vs.no_cursor_bitmap, 
							      vs.no_cursor_bitmap, &x11.black, &x11.black, 0, 0);
			XDefineCursor( x11.disp, vs.win, vs.no_x_cursor);
			XFreePixmap( x11.disp, vs.no_cursor_bitmap );
		}
	}

	/* window title */
	x11_set_win_title( vs.win, "Mac-on-Linux" );
	XRaiseWindow( x11.disp, vs.win );

	add_key_actions( xv_keys, sizeof(xv_keys) );
	
	XSync( x11.disp, false );
	return true;
}

static int 
xvideo_init( video_module_t *m )
{
	int width=640, height=480;
	video_desc_t *vm;
	int n, i, hz;

	if( !get_bool_res("enable_xvideo") || !x11.connected )
		return 1;

	memset( &vs, 0, sizeof(vs) );

	vs.use_x_cursor = get_bool_res("use_x_cursor")? 1: 0;

	/* create color maps for 8 bit mode */
	if( x11.xdepth == 8 ) {
		vs.cmap = XCreateColormap( x11.disp, x11.rootwin, x11.vis, AllocAll );
		XInstallColormap( x11.disp, vs.cmap );
	}

	/* initialize according to display type */
	if( !init_window(width, height) )
		return 1;

	/* Start periodic redraws */
	if( (hz=get_numeric_res("x_vbl_rate")) < 0 )
		hz = 85;
	else
		printm("X refresh rate: %d Hz\n", hz );

	vs.vbl_period = 1000000UL/hz;

	/* fill in supported video modes (depth 8 is emulated) */
	n = (x11.xdepth > 8)? 2 : 1;
	vm = calloc( n, sizeof(video_desc_t) );
	xvideo_module.modes = vm;

	for( i=0; i<n; i++ ) {
		vm[i].offs = -1;
		vm[i].rowbytes = -1;
		vm[i].h = -1;
		vm[i].w = -1;
		vm[i].depth = (!i) ? x11.xdepth : 8;
		vm[i].next = (i+1<n)? &vm[i+1] : NULL;
		vm[i].refresh = hz;
	}
	return 0;
}

static void 
xvideo_cleanup( video_module_t *m )
{
	/* Close window and server connection */
	XSync( x11.disp, false );

	if( vs.is_open )
		vclose();

	XFlush( x11.disp );
	XSync( x11.disp, false );
	if( x11.xdepth == 8 )
		XFreeColormap( x11.disp, vs.cmap );

	/* XXX: close server conection here! */
	free( xvideo_module.modes );
}

video_module_t xvideo_module  = {
	.name		= "xvideo",
	.vinit 		= xvideo_init,
	.vcleanup 	= xvideo_cleanup,
	.vopen		= vopen,
	.vclose		= vclose,
	.vrefresh	= vrefresh,
	.setcmap	= setcmap,
	
	.vbl		= xvideo_vbl,
};
