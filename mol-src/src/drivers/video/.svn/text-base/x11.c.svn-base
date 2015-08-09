/* 
 *   Creation Date: <2003/01/16 00:43:35 samuel>
 *   Time-stamp: <2004/01/11 21:32:34 samuel>
 *   
 *	<x11.c>
 *	
 *	Common X11 functions
 *   
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#ifdef HAVE_PNG
#include <png.h>
#include <setjmp.h>
#endif
#include "driver_mgr.h"
#include "res_manager.h"
#include "keycodes.h"
#include "timer.h"
#include "booter.h"
#include "video.h"
#include "x11.h"

/* globals */
x11_info_t		x11;

static struct {
	int		mapped;
	Window		win;
	GC		gc;

	int		bw_mode;			/* set in black-and-white mode */
	XImage		*im;
	int		butt_down;

	const char	*msg, *msg_persistent;
} w;

static const int 	kEventMask = KeyPressMask | KeyReleaseMask | StructureNotifyMask
					| PointerMotionMask | EnterWindowMask | ExposureMask
					| ButtonReleaseMask | ButtonPressMask;

static const int	but1_x=204, but1_y=8, but1_w=108, but1_h=22;
static const int	but2_x=204, but2_y=37, but2_w=108, but2_h=22;
static const int	text_y=110;

#define BW_WIN_WIDTH	320
#define BW_WIN_HEIGHT	160


/************************************************************************/
/*	create X-picture						*/
/************************************************************************/

#ifdef HAVE_PNG
#define PNG_BYTES_TO_CHECK 8

static int
check_if_png( char *fname, FILE **retf )
{
	char buf[ PNG_BYTES_TO_CHECK ];
	FILE *f;
	int ret=1;

	if( !((f=fopen(fname, "ro"))) )
		return 0;
	ret = fread(buf, 1, PNG_BYTES_TO_CHECK, f) == PNG_BYTES_TO_CHECK;
	ret = ret && !png_sig_cmp((png_bytep) buf, (png_size_t)0, PNG_BYTES_TO_CHECK);
	
	*retf = f;
	if( !ret )
		fclose(f);
	return ret;
}

static void
convert_from_24( char *s, char *d, int width, int depth, int byteorder )
{
	int i;
	if( x11.xdepth >= 24 ) {
		for( i=0; i<width; i++, s+=3, d+=4 )
			d[1] = s[0], d[2] = s[1], d[3] = s[2];
	} else if( x11.xdepth == 15 ) {
		ushort *dd = (ushort*)d;
		for( i=0; i<width; i++, s+=3 ) {
			*dd++ = (((s[0] << 7) & 0x7c00) |
				((s[1] << 2) & 0x3e0) |
				(s[2] >> 3));
		}
	} else if( x11.xdepth == 16 ) {
		ushort *dd = (ushort*)d;
		for( i=0; i<width; i++, s+=3 ) {
			*dd++ = (((s[0] << 8) & 0xf800) |
				((s[1] << 3) & 0x7e0) |
				(s[2] >> 3));
		}
	}
}

static int
create_x_picture( int *retw, int *reth ) 
{
	png_structp png;
	png_infop info = 0;
	png_uint_32 width, height;
	int transf = PNG_TRANSFORM_EXPAND | PNG_TRANSFORM_STRIP_ALPHA;
	int i, ret, rb;
	char *s, *buf=NULL;
	png_bytepp rows;
	FILE *f;

	if( !(s=get_filename_res("mdialog")) || !check_if_png(s, &f) )
		return 1;

	png = png_create_read_struct( PNG_LIBPNG_VER_STRING, NULL, NULL, NULL );
	if( png )
		info = png_create_info_struct( png );

	if( !info || setjmp(png_jmpbuf(png)) ) {
		ret = 1; goto bail2;
	}
	ret = 1;
	png_init_io( png, f );
	png_set_sig_bytes( png, PNG_BYTES_TO_CHECK );

	png_read_png( png, info, transf, NULL );
	rows = png_get_rows( png, info );

	width = png_get_image_width( png, info );
	height = png_get_image_height( png, info );

	rb = width * x11.bpp;
	if( rb & 3 )
		rb += 4 - (rb & 3);
	if( !(buf=malloc(height * rb)) )
		goto bail;
	
	w.im = XCreateImage( x11.disp, x11.vis, x11.xdepth, ZPixmap, 0,
			     (char*)buf, width, height, 32, rb );
	w.im->byte_order = MSBFirst;

	for( i=0; i<height ; i++ ) {
		convert_from_24( (char *) rows[i], buf + i*rb, width, x11.xdepth, x11.byte_order == MSBFirst );
	}
	ret = 0;
	*retw = width, *reth = height;
 bail:
	if( ret && buf )
		free( buf );
 bail2:
	png_destroy_read_struct( &png, info ? &info : NULL, NULL );
	fclose( f );
	return ret;
}
#else /* HAVE_PNG */
static int
create_x_picture( int *retw, int *reth ) 
{
	return 1;
}
#endif

/************************************************************************/
/*	useful routines							*/
/************************************************************************/

void
x11_set_win_title( Window w, char *title )
{
	XTextProperty tp;

	if( !XStringListToTextProperty( &title, 1, &tp ) )
		return;
	XSetWMName( x11.disp, w, &tp );
	XFree( tp.value );
}

static int 
init_window( int width, int height )
{
	char *window_name = "Mac-on-Linux";
	char *icon_name = "MOL";
	XSetWindowAttributes wattr;
	XTextProperty winName, iconName;
	XWMHints *wm_hints;
	XClassHint *class_hints;
	XSizeHints *size_hints;
	Window win;
	
	/* create window */
	memset( &wattr, 0, sizeof(wattr) );
	wattr.event_mask = kEventMask;
	/* wattr.background_pixel = x11.black_pixel; */
	wattr.background_pixmap = None;
	wattr.border_pixel = x11.black_pixel;
	wattr.backing_store = NotUseful;
	wattr.backing_planes = x11.xdepth;
	wattr.override_redirect = False;

	XSync( x11.disp, 0 );
	w.win = win = XCreateWindow( x11.disp, x11.rootwin, 0, 0, width, height, 0, x11.xdepth,
				     InputOutput, x11.vis, CWEventMask | CWBackPixmap | CWBorderPixel
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
	size_hints->flags = PPosition | PSize | PMinSize | PMaxSize;
	size_hints->max_width = size_hints->min_width = width;
	size_hints->max_height = size_hints->min_height = height;

	XSetWMProperties( x11.disp, win, &winName, &iconName, NULL, 0,
			  size_hints, wm_hints, class_hints );
	XFree( winName.value );
	XFree( iconName.value );
	XFree( wm_hints );
	XFree( class_hints );
	XFree( size_hints );

	/* XStoreName(x11.disp, win, "Mac-on-Linux" ); */

	/* Create GC */
	w.gc = XCreateGC( x11.disp, win, 0, 0 );
	XSetState( x11.disp, w.gc, x11.black_pixel, x11.white_pixel, GXcopy, AllPlanes );

	/* Window title */
	x11_set_win_title( win, "Mac-on-Linux" );
	XRaiseWindow( x11.disp, win );
	
	XSync( x11.disp, 0 );
	return 0;
}


/************************************************************************/
/*	event functions							*/
/************************************************************************/

static char failed_text[]="The current video mode/depth is unsupported\n"
			  "by the X11 video driver.\n";

static void
set_msg( const char *text, int is_tmp )
{
	int update = w.msg != text;
		
	w.msg = text;
	if( !is_tmp )
		w.msg_persistent = text;

	if( update && w.mapped ) {
		if( !w.bw_mode )
			XClearArea( x11.disp, w.win, 8, text_y,
				    w.im->width-8, w.im->height-text_y, 1 );
		else
			XClearArea( x11.disp, w.win, 0, 0, BW_WIN_WIDTH, BW_WIN_HEIGHT, 1 );
		XFlush( x11.disp );
	}
}

static void
draw_button( int n ) 
{
	int restore = n < 0;
	int dx,dy,x,y,h,ww,offs;
	XImage *im2;
	char *p;

	if( w.bw_mode )
		return;

	dx = but2_x, dy = but2_y, ww=but2_w, h=but2_h;
	if( n==1 || n==-1 )
		dx = but1_x, dy = but1_y, ww=but1_w, h=but1_h;

	if( restore ) {
		XPutImage( x11.disp, w.win, w.gc, w.im, dx, dy, dx, dy, ww, h );
		return;
	}

	im2 = XSubImage( w.im, dx, dy, ww, h );

	for( y=1; y<im2->height-1; y++ ) {
		x = offs = (y>2 && y<im2->height-2)? 1 : 2;
		p = im2->data + im2->bytes_per_line * y + x11.bpp * x;

		if( x11.xdepth >= 24 ) {
			for( ; x<im2->width-offs; x++ ) {
				int r = p[1] - 20, g = p[2] - 20;
				p[1] = (r < 0) ? 0 : r;
				p[2] = (g < 0) ? 0 : g;
				//p[3] = (b < 0) ? 0 : b;
				p+=4;
			}
		} else if( x11.xdepth == 15 ) {
			ushort *pp = (ushort*)p;
			for( ; x<im2->width-offs; x++ ) {
				int r = (*pp & 0x7c00) - (5<<10);
				int g = (*pp & 0x3e0) - (5<<5);
				int b = (*pp & 0x1f) - 5;
				*pp++ = ((r < 0) ? 0 : r) | ((g<0) ? 0 : g) | ((b<0)? 0: b);
			}
		} else if( x11.xdepth == 16 ) {
			ushort *pp = (ushort*)p;
			for( ; x<im2->width-offs; x++ ) {
				int r = (*pp & 0xf800) - (5<<11);
				int g = (*pp & 0x7e0) - (5<<6);
				int b = (*pp & 0x1f) - 5;
				*pp++ = ((r < 0) ? 0 : r) | ((g<0) ? 0 : g) | ((b<0)? 0: b);
			}
		}
	}
	XPutImage( x11.disp, w.win, w.gc, im2, 0, 0, dx, dy, ww, h );
	XDestroyImage( im2 );
}

static int
in_button( int x, int y )
{
	if( w.bw_mode )
		return 0;
	if( (uint)(x - but1_x) < but1_w && (uint)(y - but1_y) < but1_h )
		return 1;
	if( (uint)(x - but2_x) < but2_w && (uint)(y - but2_y) < but2_h )
		return 2;
	return 0;
}

static void
handle_button( int but )
{
	int ret;
	if( but == 1 ) {
		/* set_msg( "Switching to fullscreen mode", 1 ); */
		ret = switch_to_fullscreen();
		if( ret < 0 ) {
			set_msg("No fullscreen video modes are configured\n", 1);
		} else if( ret ) {
			set_msg("The current video mode is unsupported by\n"
				"the fullscreen video driver. Please change\n"
				"the depth and/or resolution in MacOS\n", 1);
		} else {
			set_msg("Fullscreen mode activated", 1);
		}
	} else {
		set_msg("This operation is not yet supported.\n", 1);
		/* change res */
	}
}

static void
handle_event( XEvent *event )
{
	KeySym k;
	const char *p;
	int y, but;
	
	switch( event->type ) {
	case KeyRelease:
		/* Do the switch on key release instead of key press. Keys tend to
		 * get stuck in the down position otherwise.
		 */
		k = XLookupKeysym( (XKeyEvent*)event, 0 );
		if( k == XK_space || k == XK_Tab || k == XK_Return )
			handle_button(1);
		break;

	case ButtonPress:
	case ButtonRelease:
		#define ev (event->xbutton)
		but = in_button( ev.x, ev.y );
		if( event->type == ButtonRelease ) {
			if( but && but == w.butt_down )
				handle_button( but );
			but = (w.butt_down < 0)? -w.butt_down : w.butt_down;
			draw_button( -but );
			w.butt_down = 0;

			if( w.bw_mode )
				handle_button( 1 );
		} else {
			w.butt_down = but;
			draw_button( but );
		}
		XFlush( x11.disp );
		#undef ev
		break;

	case MotionNotify:
		#define ev (event->xmotion)
		but = in_button( ev.x, ev.y );
		if( (w.butt_down>0 && w.butt_down != but) || (w.butt_down<0 && but == -w.butt_down) ) {
			w.butt_down = -w.butt_down;
			draw_button( w.butt_down );
		}
		#undef ev
		break;

	case EnterNotify:
		if( w.msg != w.msg_persistent )
			set_msg( w.msg_persistent, 0 );
		break;

	case Expose:
		if( !w.mapped )
			break;
		#define ev (event->xexpose)
		if( !w.bw_mode ) {
			XPutImage( x11.disp, w.win, w.gc, w.im, ev.x, ev.y,	
				   ev.x, ev.y, ev.width, ev.height );
			if( w.butt_down )
				draw_button( w.butt_down );
			y = 130;
		} else {
			XGCValues v;
			static const char *inst = "Press [Ret] to switch to full-screen";
			v.foreground = x11.black_pixel;
			XChangeGC( x11.disp, w.gc, GCForeground, &v );
			XFillRectangle( x11.disp, w.win, w.gc, ev.x, ev.y, ev.width, ev.height );
			v.foreground = x11.white_pixel;
			XChangeGC( x11.disp, w.gc, GCForeground, &v );
			XDrawString( x11.disp, w.win, w.gc, 14, 16, inst, strlen(inst) );
			y = 16+20;
		}
		for( p=w.msg ; *p ; y+=13 ) {
			char *p2 = strchr(p, '\n');
			int len = p2 ? (p2-p) : strlen(p);

			XDrawString( x11.disp, w.win, w.gc, 14, y, p, len );
			if( p2 )
				len++;
			p += len;
		}
		XFlush( x11.disp );
		#undef ev
		break;

	case MapNotify:
		w.mapped = 1;
		break;
	case UnmapNotify:
		w.mapped = 0;
		break;
	}
}

static void
event_loop( int id, void *dummy, int latency )
{
	XEvent event;

	while( XCheckWindowEvent( x11.disp, w.win, kEventMask, &event ) )
		handle_event(&event);

	usec_timer( w.mapped ? 1000000/10 : 1000000/4, event_loop, NULL );
}

void
display_x_dialog( void )
{
	if( !w.win || w.mapped )
		return;

	set_msg( failed_text, 0 );
	usec_timer( 1000000/15, event_loop, NULL );
	XRaiseWindow( x11.disp, w.win );	
	XMapWindow( x11.disp, w.win );
	XFlush( x11.disp );
}

void
remove_x_dialog( void )
{
	if( w.mapped ) {
		XUnmapWindow( x11.disp, w.win );
		XFlush( x11.disp );
	}
}


/************************************************************************/
/*	keycode stuff							*/
/************************************************************************/

void
x11_key_event( XKeyEvent *ev ) 
{
	key_event( kRawXKeytable, X_RAW_KEYCODE | ev->keycode, ev->type == KeyPress );
}

typedef struct {
	KeySym	ksym;
	int	adb_code;
} ksym_tran_t;

typedef struct {
	KeySym	ksym1, ksym2;
	int	adb_code;
} ksym_tran2_t;

static const ksym_tran_t ttab[] = {
	{ XK_Left,		0x3b	},
	{ XK_Right,		0x3c	},
	{ XK_Up,		0x3e	},
	{ XK_Down,		0x3d	},

	{ XK_Shift_L,		0x38	},
	{ XK_Shift_R,		0x38	},
	{ XK_Control_L,		0x36	},
	{ XK_Control_R,		0x36	},
	{ XK_Caps_Lock,		0x39	},
	{ XK_Mode_switch,	0x3a	},
	{ XK_space,		0x31	},
	{ XK_Return,		0x24	},
	{ XK_Tab,		0x30	},
	{ XK_Escape,		0x35	},

	{ XK_KP_1,		0x53	},
	{ XK_KP_2,		0x54	},
	{ XK_KP_3,		0x55	},
	{ XK_KP_4,		0x56	},
	{ XK_KP_5,		0x57	},
	{ XK_KP_6,		0x58	},
	{ XK_KP_7,		0x59	},
	{ XK_KP_8,		0x5b	},
	{ XK_KP_9,		0x5c	},
	{ XK_KP_0,		0x52	},
	{ XK_KP_Decimal,	0x41	},
	{ XK_KP_Separator,	0x41	},
	{ XK_KP_Enter,		0x4c	},	/* ibook enter = 0x34 */

	{ XK_KP_Up,		0x5b	},	/* num-lock mode translates */
	{ XK_KP_Down,		0x54	},	/* to keypad keys */
	{ XK_KP_Begin,		0x57	},
	{ XK_KP_Home,		0x59	},
	{ XK_KP_End,		0x53	},
	{ XK_KP_Prior,		0x5c	},
	{ XK_KP_Next,		0x55	},
	{ XK_KP_Left,		0x56	},
	{ XK_KP_Right,		0x58	},
	{ XK_KP_Insert,		0x52	},
	{ XK_KP_Delete,		0x41	},
	
	{ XK_Num_Lock,		0x47	},
	{ XK_KP_Equal,		0x51	},
	{ XK_KP_Divide,		0x4b	},
	{ XK_KP_Multiply,	0x43	},
	{ XK_KP_Add,		0x45	},
	{ XK_KP_Subtract,	0x4e	},

	{ XK_Help,		0x72	},
	{ XK_Insert,		0x72	},

	{ XK_Begin,		0x73	},
	{ XK_Home,		0x73	},
	{ XK_End,		0x77	},
	{ XK_Prior,		0x74	},
	{ XK_Page_Up,		0x74	},
	{ XK_Next,		0x79	},
	{ XK_Page_Down,		0x79	},

	{ XK_paragraph,		0x0a	},
	{ XK_1,			0x12    },
	{ XK_2,			0x13    },
	{ XK_3,			0x14    },
	{ XK_4,			0x15    },
	{ XK_5,			0x17    },
	{ XK_6,			0x16    },
	{ XK_7,			0x1a    },
	{ XK_8,			0x1c    },
	{ XK_9,			0x19    },
	{ XK_0,			0x1d    },
	{ XK_minus,		0x1b	},
	{ XK_equal,		0x18	},

	{ XK_q,			0x0c    },
	{ XK_w,			0x0d    },
	{ XK_e,			0x0e    },
	{ XK_r,			0x0f    },
	{ XK_t,			0x11    },
	{ XK_y,			0x10    },
	{ XK_u,			0x20    },
	{ XK_i,			0x22    },
	{ XK_o,			0x1f    },
	{ XK_p,			0x23    },
	{ XK_bracketleft,	0x21	},
	{ XK_bracketright,	0x1e	},

	{ XK_a,			0x00    },
	{ XK_s,			0x01    },
	{ XK_d,			0x02    },
	{ XK_f,			0x03    },
	{ XK_g,			0x05    },
	{ XK_h,			0x04    },
	{ XK_j,			0x26    },
	{ XK_k,			0x28    },
	{ XK_l,			0x25    },
	{ XK_semicolon,		0x29	},
	{ XK_apostrophe,	0x27	},
	{ XK_backslash,		0x2a	},
	
	{ XK_grave,		0x32	},
	{ XK_z,			0x06    },
	{ XK_x,			0x07    },
	{ XK_c,			0x08    },
	{ XK_v,			0x09    },
	{ XK_b,			0x0b    },
	{ XK_n,			0x2d    },
	{ XK_m,			0x2e    },
	{ XK_comma,		0x2b	},
	{ XK_period,		0x2f    },
	{ XK_slash,		0x2c	},

	{ XK_F1,		0x7a	},
	{ XK_F2,		0x78	},
	{ XK_F3,		0x63	},
	{ XK_F4,		0x76	},
	{ XK_F5,		0x60	},
	{ XK_F6,		0x61	},
	{ XK_F7,		0x62	},
	{ XK_F8,		0x64	},
	{ XK_F9,		0x65	},
	{ XK_F10,		0x6d	},
	{ XK_F11,		0x67	},
	{ XK_F12,		0x6f	},
	{ XK_F13,		0x69	},
	{ XK_F14,		0x6b	},
	{ XK_F15,		0x71	},

	/* International keyboards */
	{ XK_less,		0x32	},
#if 1
	/* These depends on the distribution... */
	{ XK_BackSpace,		0x33    },	/* Real backspace */
	{ XK_Delete,		0x75    },
	{ XK_Alt_L,		0x37	},	
	{ XK_Alt_R,		0x37	},	/* 0x3a = alt, 0x37 = command */
	{ XK_Meta_L,		0x37	},
	{ XK_Meta_R,		0x37	},
#endif

	/* Swedish / Finnish */
	{ XK_odiaeresis,	0x29	},
	{ XK_adiaeresis,	0x27	},
	{ XK_aring,		0x21	},
	{ XK_dead_diaeresis,	0x1e	},
	{ XK_dead_grave,	0x18	},

	/* Norwegian */
	{ XK_ae,		0x27	},
	{ XK_oslash,		0x29	}
};

/* These fixes are not perfect (but reduces the need for manual tuning) */
static const ksym_tran2_t ttab2[] = {
	/* Fixes for Swedish (and others) */
	{ XK_apostrophe,	XK_asterisk,	0x2a	},

	/* Norwegian */
	{ XK_apostrophe,	XK_paragraph,	0x0a	},

	/* German */
	{ XK_asciicircum,	XK_degree,	0x0a	},
	{ XK_plus,		XK_asterisk,	0x1e	}

};

static const ksym_tran2_t ttab_key[] = {
	/* Matching is performed against the first entry. If there is a 
	 * binding maches, all the following bindings are used too.
	 */

	/* German */
	{ XK_udiaeresis,	XK_Udiaeresis,	0x21	},	/* Signature */
	{ XK_z,			XK_Z,		0x10	},
	{ XK_y,			XK_Y,		0x06	},
 	{ XK_minus,		XK_underscore,	0x2c	},
	{ 0,0,0 },

	/* Scandinavian languages */
	{ XK_aring,		XK_Aring,	0x21	},	/* Signature */
 	{ XK_minus,		XK_underscore,	0x2c	},
	{ 0,0,0 },

	/* French */
	{ XK_ampersand,		XK_1, 		0x12	},	/* Signature */
	{ XK_at,		XK_numbersign,	0x0a	},
	{ XK_eacute,		XK_2, 		0x13	},
	{ XK_egrave,		XK_2, 		0x13	},
	{ XK_quotedbl,		XK_3, 		0x14	},
	{ XK_apostrophe,	XK_4, 		0x15	},
	{ XK_parenleft,		XK_5, 		0x17	},
	{ XK_minus,		XK_6,		0x16	},
	{ XK_paragraph,		XK_6, 		0x16	},
	{ XK_egrave,		XK_7, 		0x1a	},
	{ XK_eacute,		XK_7, 		0x1a	},
	{ XK_underscore,	XK_8, 		0x1c	},
	{ XK_exclam,		XK_8, 		0x1c	},
	{ XK_ccedilla,		XK_9, 		0x19	},
	{ XK_aacute,		XK_0, 		0x1d	},
	{ XK_agrave,		XK_0, 		0x1d	},
	{ XK_parenright,	XK_degree,	0x1b	},
 	/* { XK_minus,		XK_underscore,	0x51	},*/
	{ XK_dollar,		XK_asterisk,	0x1e	},
	{ XK_dollar,		XK_sterling,	0x1e	},
	{ XK_a,			XK_A,		0x0c    },
	{ XK_z,			XK_Z,		0x0d    },
	{ XK_e,			XK_E,		0x0e    },
	{ XK_q,			XK_Q,		0x00    },
	{ XK_m,			XK_M,		0x29	},
	{ XK_w,			XK_W,		0x06	},
	{ XK_ugrave,		XK_percent,	0x27	},
	{ XK_grave,		XK_sterling,	0x2a	},
	{ XK_comma,		XK_question,	0x2e	},
	{ XK_semicolon,		XK_period, 	0x2b	},
	{ XK_colon,		XK_slash,	0x2f	},
	{ XK_equal,		XK_plus,	0x18	},
	{ XK_exclam,		XK_paragraph, 	0x2c	},
	{ 0,0,0 }
};


static void
setup_keymapping( void )
{
	KeySym *list, *p;
	int i, j, min, max, ksyms_per_keycode, match;

	XDisplayKeycodes( x11.disp, &min, &max );
	register_key_table( kRawXKeytable, min, max );
	
	list = XGetKeyboardMapping( x11.disp, min, max-min+1, &ksyms_per_keycode );

	for( i=min,p=list; i<=max; i++, p+=ksyms_per_keycode ) {
		for( j=0; j<sizeof(ttab)/sizeof(ksym_tran_t); j++ )
		if( ttab[j].ksym == *p )
			set_keycode( kRawXKeytable, i, ttab[j].adb_code );
		if( ksyms_per_keycode < 2 )
			continue;
		for( j=0; j<sizeof(ttab2)/sizeof(ksym_tran2_t); j++ )
			if( ttab2[j].ksym1 == p[0] && ttab2[j].ksym2 == p[1] )
				set_keycode( kRawXKeytable, i, ttab2[j].adb_code );
	}

	match=0;
	for( j=0; j<sizeof(ttab_key)/sizeof(ksym_tran2_t); j++ ) {
		if( !ttab_key[j].ksym1 ) {
			match=0;
			continue;
		}
		for( i=min,p=list; match>=0 && i<=max; i++, p+=ksyms_per_keycode ) {
			if( ttab_key[j].ksym1 == p[0] && ttab_key[j].ksym2 == p[1] ) {
				set_keycode( kRawXKeytable, i, ttab_key[j].adb_code );
				match=1;
			}
		}
		if( !match )
			match--;
	}
	XFree(list);

	user_kbd_customize( kRawXKeytable );
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static int 
error_handler( Display *d, XErrorEvent *e )
{
	char buf[80];
	XGetErrorText( d, e->error_code, buf, sizeof(buf) );
	printm("---> X11 Error: %s\n", buf );

	/* simply disregard the error and hope for the best... */
	return 0;
}

static int
x11_init( void )
{
	int screen, color_class, width, height;
	XVisualInfo visual_info;
	Display *disp;
	char *str;
	
	if( !get_bool_res("enable_xvideo") && !get_bool_res("enable_xdga_video") )
		return 0;

	str = get_str_res("xdisplay");
	if( !(disp=XOpenDisplay(str)) ) {
		printm("Could not connect to X server %s\n", XDisplayName(str));
		return 0;
	}
	x11.disp = disp;
	XSetErrorHandler( error_handler );

	/* find screen and root window */
	x11.screen = screen = XDefaultScreen( disp );
	x11.rootwin = XRootWindow( disp, screen );

	/* get screen depth */
	x11.xdepth = DefaultDepth( disp, screen );

	/* find black and white colors */
	XParseColor( disp, DefaultColormap(disp, screen), "rgb:00/00/00", &x11.black );
	XAllocColor( disp, DefaultColormap(disp, screen), &x11.black);
	XParseColor( disp, DefaultColormap(disp, screen), "rgb:ff/ff/ff", &x11.white );
	XAllocColor( disp, DefaultColormap(disp, screen), &x11.white );
	x11.black_pixel = BlackPixel( disp, screen );
	x11.white_pixel = WhitePixel( disp, screen );

	/* get appropriate visual */
	color_class = (x11.xdepth == 1)? StaticGray : (x11.xdepth < 15)? PseudoColor : TrueColor;

	if( !XMatchVisualInfo(disp, screen, x11.xdepth, color_class, &visual_info) ) {
		printm("No XVisual Error\n");
		return 1;
	}
	x11.vis = visual_info.visual;
	x11.is_565 = (x11.xdepth == 16) && (visual_info.green_mask == 0x7e0);
	x11.bpp = (x11.xdepth >= 24)? 4 : (x11.xdepth >= 15)? 2 : 1;
	x11.byte_order = XImageByteOrder( disp );

	setup_keymapping();
	x11.connected = 1;
#if 1
	/* init MOL-not running window */
	width = BW_WIN_WIDTH, height = BW_WIN_HEIGHT;
	if( x11.xdepth < 15 || create_x_picture(&width, &height) )
		w.bw_mode = 1;
	init_window( width, height );
#endif
	return 1;
}


static void
x11_cleanup( void )
{
	if( w.im ) {
		char *b = w.im->data;
		w.im->data = NULL;
		XDestroyImage( w.im );
		free( b );
	}
	XSync( x11.disp, False );
	XCloseDisplay( x11.disp );

	x11.connected = 0;
}

driver_interface_t x11_common_driver = {
	"x11-video", x11_init, x11_cleanup
};
