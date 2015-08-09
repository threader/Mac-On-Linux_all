/*
  *  Copyright (C) 1999, 2000 AT&T Laboratories Cambridge.  All Rights Reserved.
  *
  *  This is free software; you can redistribute it and/or modify
  *  it under the terms of the GNU General Public License as published by
  *  the Free Software Foundation; either version 2 of the License, or
  *  (at your option) any later version.
  *
  *  This software is distributed in the hope that it will be useful,
  *  but WITHOUT ANY WARRANTY; without even the implied warranty of
  *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  *  GNU General Public License for more details.
  *
  *  You should have received a copy of the GNU General Public License
  *  along with this software; if not, write to the Free Software
  *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307,
  *  USA.
  */

/*
  * vncvideo.c - a driver for mol which exports the display via the VNC protocol
  *
  * This pretty much a hack. There are lots of things done very inelegantly
  * and sometimes just plain wrong. Do not be surprised if it doesn't work.
  *
  * To enable this driver put enable_vncvideo in your molrc.
  *
  * To specifiy which port to use to serve VNC add a line like this:
  *
  * vnc_port:  5900
  *
  * If you don't specifiy vnc_port you'll get a random one (probably)
  *
  * It serve unauthenticated VNC (i.e. anyone with access to your vnc port has ac
  * ess to MOL). There is no http server (yet) so java connections won't work.
  *
  * CREDITS:
  *    Christian Bauer - original Basillisk II X-driver
  *    Samuel Rydh - original xvideo driver and, of course, MOL.
  *    Tristan Richardson - VNC
  *    Nick Hollighurst - Linux hackery
  *
  * AUTHOR:
  *    Charlie McLachlan cim@uk.research.att.com
  *
  * FURTHER DETAILS:
  *    http://www.uk.research.att.com/vnc
  *    http://www.ibrium.se
  *
  * VERSION:
  *    0.0.0 - 8 May 2000
  *
  */

#include "mol_config.h"

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <X11/extensions/XShm.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <pthread.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <sys/socket.h>
#include "poll_compat.h"

#include "verbose.h"
#include "debugger.h"
#include "res_manager.h"
#include "memory.h"
#include "mouse_sh.h"
#include "wrapper.h"
#include "thread.h"
#include "video_module.h"
#include "booter.h"
#include "driver_mgr.h"
#include "keycodes.h"
#include "input.h"
#include "async.h"

// --- Basic VNC definitions -----

typedef unsigned char   CARD8;
typedef unsigned short  CARD16;
typedef unsigned int    CARD32;

#include "rfbproto.h"

// --- Mol definitions ----------

SET_VERBOSE_NAME("vncvideo");

typedef unsigned char   uint8;
typedef int             bool;
#define false   	0
#define true    	1

#define ADBKeyDown( ch ) PE_adb_key_event( (ch & 0x7f), VNC_RAW_KEYCODE | (ch & 0x7f) )
#define ADBKeyUp( ch ) PE_adb_key_event( (ch | 0x80), VNC_RAW_KEYCODE | (ch & 0x7f) )

// ---- vnc helper macros ----------

#define CARD32IntoCARD8Star(a, b)  	\
   *(b++) = ((a) & 0xff000000) >> 24;	\
   *(b++) = ((a) & 0xff0000) >> 16;	\
   *(b++) = ((a) & 0xff00) >> 8;	\
   *(b++) = ((a) & 0xff);

#define SwapCARD32IntoCARD8Star(a, b)  	\
   *(b++) = ((a) & 0xff);		\
   *(b++) = ((a) & 0xff00) >> 8;	\
   *(b++) = ((a) & 0xff0000) >> 16;	\
   *(b++) = ((a) & 0xff000000) >> 24;

#define CARD16IntoCARD8Star(a, b) 	\
   *(b++) = ((a) & 0xff00) >> 8; 	\
   *(b++) = ((a) & 0xff);

#define SwapCARD16IntoCARD8Star(a, b)	\
   *(b++) = ((a) & 0xff); 		\
   *(b++) = ((a) & 0xff00) >> 8;

#define IndexedToRemote(i)                                                \
               ((mac_pal[i*3] * remote_format.redMax + 127) / 255)        \
               << remote_format.redShift           |                      \
               ((mac_pal[i*3 + 1] * remote_format.greenMax + 127) / 255)  \
               << remote_format.greenShift           |                    \
               ((mac_pal[i*3 + 2] * remote_format.blueMax + 127) / 255)   \
               << remote_format.blueShift;
 
#define RGB555ToRemote(i) \
  (((((i >> 10) & 31) * remote_format.redMax + 15) / 31)               \
                                       << remote_format.redShift) |    \
  (((((i >> 5) & 31) * remote_format.greenMax + 15) / 31)              \
                                       << remote_format.greenShift) |  \
  ((((i & 31) * remote_format.blueMax + 15) / 31)                      \
                                       << remote_format.blueShift);

#define RGB565ToRemote(i) \
  (((((i >> 11) & 31) * remote_format.redMax + 15) / 31)               \
                                       << remote_format.redShift) |    \
  (((((i >> 5) & 63) * remote_format.greenMax + 31) / 63)              \
                                       << remote_format.greenShift) |  \
  ((((i & 31) * remote_format.blueMax + 15) / 31)                      \
                                       << remote_format.blueShift);
 

/* #define RGB888ToRemote(i) \
   (((((i >> 16) & 0xff) * remote_format.redMax + 127) / 255)   \
             << remote_format.redShift) |			\
   (((((i >> 8) & 0xff) * remote_format.greenMax + 127) / 255)  \
             << remote_format.greenShift) |      		\
   ((((i & 0xff) * remote_format.blueMax + 127) / 255)    	\
             << remote_format.blueShift);    */

#define ARGB8888ToRemote(i) \
   (((((i >> 16) & 0xff) * remote_format.redMax + 127) / 255)   \
             << remote_format.redShift) |			\
   (((((i >> 8) & 0xff) * remote_format.greenMax + 127) / 255)  \
             << remote_format.greenShift) |			\
   ((((i & 0xff) * remote_format.blueMax + 127) / 255)		\
             << remote_format.blueShift);

static bool		sendHextiles8( int x, int y, int w, int h);
static bool		sendHextiles16( int x, int y, int w, int h);
static bool		sendHextiles32( int x, int y, int w, int h);

// cache the Mac's palette
static unsigned char	mac_pal[256 * 3];

// bpp = bytes per pixel
static int		offscreen_bpp = 2;
static int		output_bpp = 2;

// --- vnc functions -------------------------------

static int		ks_decode( CARD32 ks );

static bool 		request_received;
static volatile int	vnc_sock = -1;
static bool		vnc_sock_valid;

static rfbPixelFormat	remote_format;

// --- vnc thread / main communication

static bool		force_redraw;
static bool		is_open;

static unsigned char	*offscreen_buf;		// The Mac's Buffer
static unsigned char	*output_buf;
static long		output_buf_size;

static pthread_mutex_t  buffers_mutex = PTHREAD_MUTEX_INITIALIZER;

static video_desc_t	vmode;  		// format of the Mac's buffer
static bool		connection_running;	// is anyone connected?

static int		ublen;

#define TRANSTABLE_SIZE	(65536*4)
static volatile CARD8	*translationtable;

// --- listen / main communication
static int		vnc_listen_sock=-1;
static int		async_listener_id;


static bool		g_CanHextile;


/************************************************************************/
/*	VNC functions							*/
/************************************************************************/

static bool
write_exact( const unsigned char *buffer, int size )
{
	int p=0, i=0;

	if( !vnc_sock_valid )
		return false;
 
	while( p < size ) {
		i = write( vnc_sock, buffer + p, size - p );
		if( i < 0 ) {
			LOG("Error on socket write\n");
			perror("Write to socket");
			// assume the socket is broken
			vnc_sock_valid = false;
			return false;
		}
		p += i;
	}
	return true;
}

static void
translate( unsigned char *in, unsigned char *out, int pixels )
{
	int x;
	CARD32 ttwo, trans = 0;
	CARD32 *in32 = (CARD32 *)in;
	CARD16 *in16 = (CARD16 *)in;
	CARD8 *in8 = (CARD8 *)in;

	//LOG( "translate( %lx, %lx, %d ) ( %d -> %d )\n", (long) in, (long) out, pixels, vmode.depth, output_bpp );

	if( vmode.depth != 32 ) {
		for( x=0; x<pixels; x++ ) {
			switch( vmode.depth ) {
			case 8:
				trans = *(in8++);
				break;
			case 15:
			case 16:
				trans = *(in16++);
				//LOG("input pixel is %04x\n", trans);
				break;
			default:
				LOG("bad vmode.depth %d\n", vmode.depth);
				return;
			}

			switch( output_bpp ) {
			case 1:
				out[x] = translationtable[trans];
				break;

			case 2:
				*(out++) = translationtable[trans * 2];
				*(out++) = translationtable[trans * 2 + 1];
				break;

			case 4:
				*(out++) = translationtable[trans * 4];
				*(out++) = translationtable[trans * 4 + 1];
				*(out++) = translationtable[trans * 4 + 2];
				*(out++) = translationtable[trans * 4 + 3];
				break;
 
			default:
				LOG("bad output_bpp (1) %d\n", output_bpp);
				return;
			}
		}
	} else {
		for( x=0; x<pixels; x++ ) {
			trans = *(in32++);
			ttwo = ARGB8888ToRemote(trans);
			switch( output_bpp ) {
			case 1:
				out[x] = ttwo;
				break;

			case 2:
				if( remote_format.bigEndian ) {
					CARD16IntoCARD8Star(ttwo, out);
				} else {
					SwapCARD16IntoCARD8Star(ttwo, out);
				}
				break;
				
			case 4:
				if( remote_format.bigEndian ) {
					CARD32IntoCARD8Star(ttwo, out);
				} else {
					SwapCARD32IntoCARD8Star(ttwo, out);
				}
				break;

			default:
				LOG("bad output_bpp (2) %d\n", output_bpp);
				return;
			}
		}
	}
}

static bool
send_rect_hextile( int x, int y, int w, int h )
{
	rfbFramebufferUpdateRectHeader rect;
	rect.encoding = rfbEncodingHextile;

	//LOG( "send_rect_hextile( %d, %d, %d, %d )\n", x, y, w, h );

	rect.r.x = x;
	rect.r.y = y;
	rect.r.w = w;
	rect.r.h = h;

	if ( !write_exact( (unsigned char*) &rect, sizeof(rect) ) ) {
		LOG( "failed to write hextile header\n" );
		return false;
	}

	switch( vmode.depth ) {
		case 8:
			return sendHextiles8( x, y, w, h );
			break;

		case 15:
		case 16:
			return sendHextiles16( x, y, w, h );
			break;

		case 32:
			return sendHextiles32( x, y, w, h );
			break;

		default:
			LOG("bad vmode.depth %d\n", vmode.depth);
			return false;
	}
	return false;
}

static bool
send_rect( int x, int y, int w, int h )
{
	int hh;
	bool res;
	
	rfbFramebufferUpdateRectHeader rect;
	
	//LOG("Send %dx%dx%dx%d Dep=(%d -> %d) Bpp=(%d -> %d)\n",
	//		    x, y, w, h,
	//                 vmode.depth, remote_format.bitsPerPixel,
	//                  offscreen_bpp, output_bpp);
	
	if( g_CanHextile )
		return send_rect_hextile( x, y, w, h );

	//LOG( "sending rawdata\n" );

	rect.encoding = 0;
	rect.r.x = x;
	rect.r.y = y;
	rect.r.w = w;
	rect.r.h = h;

	//LOG("Send %d %d %d %d (%d -> %d) (%d -> %d)\n", x, y, w, h,
	//                  vmode.depth, remote_format.bitsPerPixel,
	//                  offscreen_bpp, output_bpp);
	
	if( !write_exact((unsigned char *)&rect, sizeof(rect)) )
		return false;

	for( hh=0; hh < h; hh++ ) {
		pthread_mutex_lock(&buffers_mutex);

		translate( offscreen_buf + vmode.offs + (x + (y + hh) * vmode.w)
			   * offscreen_bpp, output_buf, w );

		res = write_exact(output_buf, w * output_bpp);

		pthread_mutex_unlock(&buffers_mutex);

		if( !res )
			return false;
	}
	
	return true;
}

static void
send_update( void )
{
	//struct timespec close_req = {0, 1}; /* 60 Hz */
	rfbFramebufferUpdateMsg msg;
	short dirty_buffer[80];
	int dirty_size = 0;
	int i, y=-1, hight;

	// wait for sock and fb to become valid
	if( (vnc_sock_valid == false) || (vnc_sock == -1) ||
	    (is_open == false) || (offscreen_buf == NULL) )
	{
		//LOG( "sock/fb not ready, EXITING send_update\n" );
		return;
	}

	if( force_redraw ) {
		dirty_size = 1;

		pthread_mutex_lock(&buffers_mutex);
		_get_dirty_fb_lines(dirty_buffer, sizeof(dirty_buffer) );
		pthread_mutex_unlock(&buffers_mutex);
 
		dirty_buffer[0] = 0;
		dirty_buffer[1] = vmode.h - 1;

		force_redraw = false;
	}
 
	if( !dirty_size ) {
		pthread_mutex_lock(&buffers_mutex);
		dirty_size = _get_dirty_fb_lines(dirty_buffer, sizeof(dirty_buffer) );
		pthread_mutex_unlock(&buffers_mutex);
	}

	if( !dirty_size )
		return;

	//LOG( "now sending update, size=%d\n", dirty_size );

	msg.type = rfbFramebufferUpdate;
	msg.nRects = dirty_size;

	request_received = false;

	// send the update

	if( !write_exact((unsigned char *)&msg, sizeof(msg)) ) {
		//LOG( "failed to write update header\n" );
		return;
	}
 
	for( i=0; i<dirty_size; i++ ) {
		y = dirty_buffer[i*2];
		hight = dirty_buffer[i*2+1] - dirty_buffer[i*2] + 1;

		if( !send_rect(0, y, vmode.w, hight) ) {
			//LOG( "failed to send rect, leaving send_update\n" );
			return;
		}
	}
	
	//LOG( "leaving send_update\n" );
}

static bool
read_exact( unsigned char *buffer, int size )
{
	int p=0, i=0;
	
	if( !vnc_sock_valid )
		return false;
	
	while( p < size ) {
		struct pollfd polls;
		int retval = 1;

		if( request_received ) {
			polls.fd = vnc_sock;
			polls.events = POLLIN;
			
			retval = poll(&polls, 1, 1000 / 10);
		}
 
		if( retval > 0 ) {
			i = read( vnc_sock, buffer + p, size - p);
			if( i <= 0 ) {
				LOG("Error on socket read %d\n", i);
				perror("Read from socket");
				vnc_sock_valid = false;
				return false;
			}
			p += i;
		}

		if( request_received ) {
			send_update();
		}
	}
	return true;
}

static void
init_translationtable( void )
{
	CARD16 tone=0;
	CARD32 ttwo;
	CARD8 *pos;
	int i;
 
	if( !is_open || !connection_running )
		return;

	if( !translationtable ) {
		translationtable = malloc( TRANSTABLE_SIZE );
		if( !translationtable ) 
			fatal("vnc: out-of-mem\n");
	}
	
	LOG("init_translationtable %d %d\n",
	    vmode.depth, remote_format.bitsPerPixel);

	pos = (CARD8 *)translationtable;

	switch( vmode.depth ) {
	case 8:
		for( i=0; i < 256; i++ ) {
			switch( remote_format.bitsPerPixel ) {
			case 8:
				*(pos++) = IndexedToRemote(i);
				break;
			case 16:
				tone = IndexedToRemote(i);
				if( remote_format.bigEndian ) {
					CARD16IntoCARD8Star(tone, pos);
				} else {
					SwapCARD16IntoCARD8Star(tone, pos);
				}
				break;
			case 32:
				ttwo = IndexedToRemote(i);
				if( remote_format.bigEndian ) {
					CARD32IntoCARD8Star(tone, pos);
				} else {
					SwapCARD32IntoCARD8Star(tone, pos);
				}
				break;
			}
		}
		break;

	case 16:
	case 15:
		for( i=0; i < 65536; i++ ) {
			switch( remote_format.bitsPerPixel ) {
			case 8:
				translationtable[i] = RGB555ToRemote(i);
				break;
			case 16:
				tone = RGB555ToRemote(i);
				if( remote_format.bigEndian ) {
					CARD16IntoCARD8Star(tone, pos);
				} else {
					SwapCARD16IntoCARD8Star(tone, pos);
				}
				break;

			case 32:
				ttwo = RGB555ToRemote(i);
 
				if( remote_format.bigEndian) {
					CARD32IntoCARD8Star(ttwo, pos);
				} else {
					SwapCARD32IntoCARD8Star(ttwo, pos);
				}
				break;
			}
		}
		break;
	}
}

static void
vnc_thread( void *arg )
{
	unsigned char buffer[256];
	CARD32 encodingBuffer[20];
	CARD32 auth = rfbNoAuth;
	CARD8 lastbuttonmask = 0;

	const rfbPixelFormat eight = { 8, 8, 1, 1, 3, 7, 7, 0, 2, 5 };
	const rfbPixelFormat sixteen = { 16, 16, 1, 1, 31, 31, 31, 0, 5, 10 };
	const rfbPixelFormat thirtytwo = { 32, 32, 1, 1, 255, 255, 255, 0, 8, 16 };
	const char *display_name  = "MOL via VNC";
	int i;

	rfbSetPixelFormatMsg set_pixels;
	rfbSetEncodingsMsg set_encodings;
	rfbFramebufferUpdateRequestMsg request;
	rfbKeyEventMsg  key;
	rfbPointerEventMsg  pointer;
	rfbClientCutTextMsg cut;
	rfbServerInitMsg server_init;

	bool caps_on = false;        // Flag: Caps Lock on

	request_received = false;

	LOG("vnc_thread starts as pid %d\n", getpid());
 
	snprintf( (char *)buffer, 256, rfbProtocolVersionFormat, rfbProtocolMajorVersion,
		  rfbProtocolMinorVersion);

	if( !write_exact(buffer, 12) )
		goto endo;
	if( !read_exact(buffer, 12) )
		goto endo;
	if( !write_exact((unsigned char *)&auth, sizeof(auth)) )
		goto endo;
	if( !read_exact(buffer, sizeof(CARD8)) )
		goto endo;

	switch( vmode.depth ) {
	case 8:
		server_init.format = eight; break;
	case 15:
	case 16:
		server_init.format = sixteen; break;
	case 24:
	case 32:
		server_init.format = thirtytwo; break;
	}

	server_init.framebufferWidth = vmode.w;
	server_init.framebufferHeight = vmode.h;
	server_init.nameLength = strlen(display_name);
	
	if( !write_exact((unsigned char *)&server_init, sizeof(server_init)) )
		goto endo;
 
	if( !write_exact((unsigned char *)display_name, strlen(display_name)) )
		goto endo;

	force_redraw = true;

	while( vnc_sock_valid ) {
		if( !read_exact(buffer, sizeof(CARD8)) )
			goto endo;
 
		switch( buffer[0] ) {
		case rfbSetPixelFormat:
			//LOG( "rfbSetPIxelFormat\n" );

			if( !read_exact(((unsigned char *)&set_pixels) + 1,
					sz_rfbSetPixelFormatMsg - 1) < 0 )
				goto endo;

			connection_running = true;
			remote_format = set_pixels.format;
			output_bpp = remote_format.bitsPerPixel / 8;

			//LOG( "setting output_bpp to %d\n", output_bpp );

			init_translationtable();

			break;
		case rfbSetEncodings:
			if( !read_exact(((unsigned char *)&set_encodings) + 1,
					sz_rfbSetEncodingsMsg - 1) < 0 )
				goto endo;

			//LOG( "rfbSetEncoding - %d encodings\n", set_encodings.nEncodings );
 
			if( !read_exact((unsigned char*)&encodingBuffer,
					sizeof(CARD32) * set_encodings.nEncodings) < 0 )
				goto endo;

			g_CanHextile = false;
			for( i=0; i<set_encodings.nEncodings; ++i ) {
				//LOG( "rfbSetEncoding - likes %d\n", encodingBuffer[i] );

				if( encodingBuffer[i] == rfbEncodingHextile ) {
					g_CanHextile = true;
					break;
				}
			}
			break;
 
		case rfbFramebufferUpdateRequest:
			//LOG( "update request\n" );

			if( !read_exact(((unsigned char *)&request) + 1,
					sz_rfbFramebufferUpdateRequestMsg - 1) < 0 )
				goto endo;
			if (!request.incremental)
				force_redraw = true;
			request_received = true;
			//send_update();
			break;
 
		case rfbKeyEvent: {
			int code;
 
			//LOG( "rfbKeyEvent\n" );

			if(!read_exact(((unsigned char *)&key) + 1, sz_rfbKeyEventMsg - 1) < 0 )
				goto endo;

			code = ks_decode(key.key);

			if( code == 0x39 ) { // Caps Lock pressed
				if( (!caps_on) && (key.down) ) {
					ADBKeyDown(code);
					caps_on = true;
				} else if( (caps_on) && (!key.down) ) {
					ADBKeyUp(code);
					caps_on = false;
				}
				break;
			}

			if( key.down )
				ADBKeyDown(code);
			else
				ADBKeyUp(code);
			break;
		}
 
		case rfbPointerEvent:
			//LOG( "rfbPointerEvent\n" );

			if( !read_exact(((unsigned char *)&pointer) + 1,
					sz_rfbPointerEventMsg - 1)  < 0 )
				goto endo;

			if( pointer.buttonMask != lastbuttonmask )
				mouse_but_event( pointer.buttonMask ? 
						 kMouseEvent_Down1 : kMouseEvent_Up1 );

			lastbuttonmask = pointer.buttonMask;

			mouse_move_to( pointer.x, pointer.y, vmode.w, vmode.h );
			break;
 
		case rfbClientCutText:
			if( !read_exact(((unsigned char *)&cut) + 1,
					sz_rfbClientCutTextMsg - 1) < 0 )
				goto endo;

			while( cut.length ) {
				int i = cut.length;
				if( i>255 )
					i = 255;
				if( !read_exact(buffer, i) < 0 )
					goto endo;
				cut.length -= i;
			}
 			break;

		default:
			LOG("Bad message %x\n", buffer[0]);
			goto endo;
			break;
		}
	}

endo:
	connection_running = false;
	LOG("Closing vnc_sock\n");
	LOG("Result %d \n", close(vnc_sock));
	vnc_sock_valid = false;
	vnc_sock = -1;
	LOG("------- vnc_thread ends\n");
	return;
}


#if 0
static void
copypixels( unsigned char *in, unsigned char*out, int ps, int rw, int w, int h )
{
	int rowBytes = w * ps;
	int realRowBytes = rw * ps;
	int y;

	/*LOG( "in=%lx out=%lx ps=%d rw=%d w=%d h=%d 
	  realRowBytes=%d rowBytes=%d\n", (long) in, (long) out, 
	  ps, rw, w, h, realRowBytes, rowBytes ); */

	for( y=0; y < h; y++ ) {
		//LOG( "y=%d in=%lx out=%lx\n", y, (long) in, (long) out );

		memcpy( out, in, rowBytes );
		in += realRowBytes;
		out += rowBytes;
	}
}
#endif


#define PUT_PIXEL8(pix) (updateBuf[ublen++] = (pix))

#define PUT_PIXEL16(pix) (updateBuf[ublen++] = ((char*)&(pix))[0], \
			  updateBuf[ublen++] = ((char*)&(pix))[1])

#define PUT_PIXEL32(pix) (updateBuf[ublen++] = ((char*)&(pix))[0], \
			  updateBuf[ublen++] = ((char*)&(pix))[1], \
			  updateBuf[ublen++] = ((char*)&(pix))[2], \
			  updateBuf[ublen++] = ((char*)&(pix))[3])



#define DEFINE_SEND_HEXTILES(bpp)					      \
									      \
									      \
static bool subrectEncode##bpp(CARD##bpp *data, int w, int h, CARD##bpp bg,   \
			       CARD##bpp fg, bool mono);		      \
static void testColours##bpp(CARD##bpp *data, int size, bool *mono,	      \
			     bool *solid, CARD##bpp *bg, CARD##bpp *fg);      \
									      \
									      \
/*									      \
  * rfbSendHextiles							      \
  */									      \
									      \
static bool								      \
sendHextiles##bpp( int rx, int ry, int rw, int rh )			      \
{									      \
     int x, y, w, h;							      \
     int startUblen;							      \
     CARD##bpp bg, fg, newBg, newFg;					      \
     bool mono, solid;							      \
     bool validBg = false;						      \
     bool validFg = false;						      \
     CARD##bpp clientPixelData[16*16*(bpp/8)];				      \
     int hh;								      \
     unsigned char* updateBuf;						      \
     unsigned char* fbptr;						      \
     unsigned char* clptr;						      \
     ublen = 0;								      \
     bg = 0;								      \
     fg = 0;								      \
     newBg = 0;								      \
     newFg = 0;								      \
     updateBuf = output_buf;						      \
     									      \
     /* LOG( "sendHextiles%dbpp( %d, %d, %d, %d )\n", bpp, rx, ry, rw, rh ); */ \
     /* LOG( "offscreen_bpp=%d\n", offscreen_bpp ); */			      \
     									      \
     for (y = ry; y < ry+rh; y += 16) {					      \
	for (x = rx; x < rx+rw; x += 16) {				      \
     									      \
	    w = h = 16;							      \
	    if (rx+rw - x < 16)						      \
		w = rx+rw - x;						      \
	    if (ry+rh - y < 16)						      \
		h = ry+rh - y;						      \
									      \
	    if ( (ublen + 1 + 16*16*(bpp/8)) > output_buf_size) {	      \
     	        pthread_mutex_lock(&buffers_mutex);			      \
		/* LOG( "Writing buffer %lx of length %d\n", (long) output_buf, ublen ); */ \
	    	if ( !write_exact(output_buf, ublen) )			      \
		{							      \
     	            pthread_mutex_unlock(&buffers_mutex);		      \
		    return false;					      \
		}							      \
     	        pthread_mutex_unlock(&buffers_mutex);			      \
									      \
		ublen = 0;						      \
	    }								      \
									      \
									      \
	    /* LOG( "sending hextile(%dx%dx%dx%d) ofsb=%lx\n", x, y, w, h, (long) offscreen_buf ); */ \
     	    pthread_mutex_lock(&buffers_mutex);				      \
	    for( hh = 0; hh < h; ++hh )					      \
	    {								      \
		/* LOG( "offscreen_buf=%lx hh=%d\n", (long) offscreen_buf, hh ); */ \
		fbptr = offscreen_buf + (x * offscreen_bpp) + ( (y+hh) * rw) * offscreen_bpp; \
		clptr = ((unsigned char*) clientPixelData) + (hh*16) * (bpp/8); \
									      \
		/* copypixels( fbptr, (unsigned char*) clientPixelData, bpp/8, rw, w, h );  */ \
	    	translate( fbptr, clptr, w );				      \
	    }								      \
     	    pthread_mutex_unlock(&buffers_mutex);			      \
									      \
	    startUblen = ublen;						      \
	    updateBuf[startUblen] = 0;					      \
	    ublen++;							      \
									      \
	    testColours##bpp(clientPixelData, w * h,			      \
			     &mono, &solid, &newBg, &newFg);		      \
									      \
	    if (!validBg || (newBg != bg)) {				      \
		validBg = true;						      \
		bg = newBg;						      \
		updateBuf[startUblen] |= rfbHextileBackgroundSpecified;	      \
		PUT_PIXEL##bpp(bg);					      \
	    }								      \
									      \
	    if (solid) {						      \
		continue;						      \
	    }								      \
									      \
	    updateBuf[startUblen] |= rfbHextileAnySubrects;		      \
									      \
	    if (mono) {							      \
		if (!validFg || (newFg != fg)) {			      \
		    validFg = true;					      \
		    fg = newFg;						      \
		    updateBuf[startUblen] |= rfbHextileForegroundSpecified;   \
		    PUT_PIXEL##bpp(fg);					      \
		}							      \
	    } else {							      \
		validFg = false;					      \
		updateBuf[startUblen] |= rfbHextileSubrectsColoured;	      \
	    }								      \
									      \
	    if (!subrectEncode##bpp(clientPixelData, w, h, bg, fg, mono)) {   \
		/* encoding was too large, use raw */			      \
		validBg = false;					      \
		validFg = false;					      \
		ublen = startUblen;					      \
		updateBuf[ublen++] = rfbHextileRaw;			      \
									      \
	    	/* LOG( "sending raw(%dx%dx%dx%d)\n", x, y, w, h ); */	      \
     	    	pthread_mutex_lock(&buffers_mutex);			      \
	    	for( hh = 0; hh < h; ++hh )				      \
	    	{							      \
			fbptr = offscreen_buf + (x * offscreen_bpp) + ( (y+hh) * rw) * offscreen_bpp; \
			clptr = ((unsigned char*) clientPixelData) + (hh*16) * (bpp/8); \
									      \
			/* copypixels( fbptr, (unsigned char*) clientPixelData, bpp/8, rw, w, h ); */ \
	    		translate( fbptr, clptr, w );			      \
	    	}							      \
     	    	pthread_mutex_unlock(&buffers_mutex);			      \
	    	memcpy( &updateBuf[ublen], (char*) clientPixelData, w * h * (bpp/8) ); \
	    	ublen += w * h * (bpp/8);				      \
	    }								      \
	}								      \
     }									      \
									      \
    /* LOG( "Writing remaining buffer %lx of length %d\n", (long) output_buf, ublen ); */ \
    if ( ublen > 0 && !write_exact(output_buf, ublen) )			      \
    	return false;							      \
    ublen = 0;								      \
									      \
     return true;							      \
}									      \
									      \
									      \
static bool								      \
subrectEncode##bpp(CARD##bpp *data, int w, int h, CARD##bpp bg,		      \
		   CARD##bpp fg, bool mono)				      \
{									      \
     CARD##bpp cl;							      \
     int x,y;								      \
     int i,j;								      \
     int hx=0,hy,vx=0,vy;						      \
     int hyflag;							      \
     CARD##bpp *seg;							      \
     CARD##bpp *line;							      \
     int hw,hh,vw,vh;							      \
     int thex,they,thew,theh;						      \
     int numsubs = 0;							      \
     int newLen;							      \
     int nSubrectsUblen;						      \
     unsigned char* updateBuf;						      \
     updateBuf = output_buf; 						      \
									      \
     nSubrectsUblen = ublen;						      \
     ublen++;								      \
									      \
     for (y=0; y<h; y++) {						      \
	line = data+(y*w);						      \
	for (x=0; x<w; x++) {						      \
	    if (line[x] != bg) {					      \
		cl = line[x];						      \
		hy = y-1;						      \
		hyflag = 1;						      \
		for (j=y; j<h; j++) {					      \
		    seg = data+(j*w);					      \
		    if (seg[x] != cl) {break;}				      \
		    i = x;						      \
		    while ((seg[i] == cl) && (i < w)) i += 1;		      \
		    i -= 1;						      \
		    if (j == y) vx = hx = i;				      \
		    if (i < vx) vx = i;					      \
		    if ((hyflag > 0) && (i >= hx)) {			      \
			hy += 1;					      \
		    } else {						      \
			hyflag = 0;					      \
		    }							      \
		}							      \
		vy = j-1;						      \
									      \
		/* We now have two possible subrects: (x,y,hx,hy) and	      \
		 * (x,y,vx,vy).  We'll choose the bigger of the two.	      \
		 */							      \
		hw = hx-x+1;						      \
		hh = hy-y+1;						      \
		vw = vx-x+1;						      \
		vh = vy-y+1;						      \
									      \
		thex = x;						      \
		they = y;						      \
									      \
		if ((hw*hh) > (vw*vh)) {				      \
		    thew = hw;						      \
		    theh = hh;						      \
		} else {						      \
		    thew = vw;						      \
		    theh = vh;						      \
		}							      \
									      \
		if (mono) {						      \
		    newLen = ublen - nSubrectsUblen + 2;		      \
		} else {						      \
		    newLen = ublen - nSubrectsUblen + bpp/8 + 2;	      \
		}							      \
									      \
		if (newLen > (w * h * (bpp/8)))				      \
		    return false;					      \
									      \
		numsubs += 1;						      \
									      \
		if (!mono) PUT_PIXEL##bpp(cl);				      \
									      \
		updateBuf[ublen++] = rfbHextilePackXY(thex,they);	      \
		updateBuf[ublen++] = rfbHextilePackWH(thew,theh);	      \
									      \
		/*							      \
		 * Now mark the subrect as done.			      \
		 */							      \
		for (j=they; j < (they+theh); j++) {			      \
		    for (i=thex; i < (thex+thew); i++) {		      \
			data[j*w+i] = bg;				      \
		    }							      \
		}							      \
	    }								      \
	}								      \
     }									      \
									      \
     updateBuf[nSubrectsUblen] = numsubs;				      \
									      \
     return true;							      \
}									      \
									      \
									      \
/*									      \
  * testColours() tests if there are one (solid), two (mono) or more	      \
  * colours in a tile and gets a reasonable guess at the best background      \
  * pixel, and the foreground pixel for mono.				      \
  */									      \
									      \
static void								      \
testColours##bpp(data,size,mono,solid,bg,fg)				      \
     CARD##bpp *data;							      \
     int size;								      \
     bool *mono;							      \
     bool *solid;							      \
     CARD##bpp *bg;							      \
     CARD##bpp *fg;							      \
{									      \
     CARD##bpp colour1, colour2;					      \
     int n1 = 0, n2 = 0;						      \
     *mono = true;							      \
     *solid = true;							      \
     colour1 = 0;							      \
     colour2 = 0;							      \
									      \
     for (; size > 0; size--, data++) {					      \
									      \
	if (n1 == 0)							      \
	    colour1 = *data;						      \
									      \
	if (*data == colour1) {						      \
	    n1++;							      \
	    continue;							      \
	}								      \
									      \
	if (n2 == 0) {							      \
	    *solid = false;						      \
	    colour2 = *data;						      \
	}								      \
									      \
	if (*data == colour2) {						      \
	    n2++;							      \
	    continue;							      \
	}								      \
									      \
	*mono = false;							      \
	break;								      \
     }									      \
									      \
     if (n1 > n2) {							      \
	*bg = colour1;							      \
	*fg = colour2;							      \
     } else {								      \
	*bg = colour2;							      \
	*fg = colour1;							      \
     }									      \
}

DEFINE_SEND_HEXTILES(8)
DEFINE_SEND_HEXTILES(16)
DEFINE_SEND_HEXTILES(32)

// functions called from fb_watch_thread and vnc_thread

static int
ks_decode( CARD32 ks )
{
	/* keysym to mac keycode translation */
	switch (ks) {
	case XK_A: case XK_a: return 0x00;
	case XK_B: case XK_b: return 0x0b;
	case XK_C: case XK_c: return 0x08;
	case XK_D: case XK_d: return 0x02;
	case XK_E: case XK_e: return 0x0e;
	case XK_F: case XK_f: return 0x03;
	case XK_G: case XK_g: return 0x05;
	case XK_H: case XK_h: return 0x04;
	case XK_I: case XK_i: return 0x22;
	case XK_J: case XK_j: return 0x26;
	case XK_K: case XK_k: return 0x28;
	case XK_L: case XK_l: return 0x25;
	case XK_M: case XK_m: return 0x2e;
	case XK_N: case XK_n: return 0x2d;
	case XK_O: case XK_o: return 0x1f;
	case XK_P: case XK_p: return 0x23;
	case XK_Q: case XK_q: return 0x0c;
	case XK_R: case XK_r: return 0x0f;
	case XK_S: case XK_s: return 0x01;
	case XK_T: case XK_t: return 0x11;
	case XK_U: case XK_u: return 0x20;
	case XK_V: case XK_v: return 0x09;
	case XK_W: case XK_w: return 0x0d;
	case XK_X: case XK_x: return 0x07;
	case XK_Y: case XK_y: return 0x10;
	case XK_Z: case XK_z: return 0x06;
 
	case XK_1: case XK_exclam: return 0x12;
	case XK_2: case XK_at: return 0x13;
	case XK_3: case XK_numbersign: return 0x14;
	case XK_4: case XK_dollar: return 0x15;
	case XK_5: case XK_percent: return 0x17;
	case XK_6: return 0x16;
	case XK_7: case XK_ampersand: return 0x1a;
	case XK_8: case XK_asterisk: return 0x1c;
	case XK_9: case XK_parenleft: return 0x19;
	case XK_0: case XK_parenright: return 0x1d;
 
	case XK_grave: case XK_asciitilde: return 0x32;
	case XK_minus: case XK_underscore: return 0x1b;
	case XK_equal: case XK_plus: return 0x18;
	case XK_bracketleft:
	case XK_braceleft: return 0x21;
	case XK_bracketright:
	case XK_braceright: return 0x1e;
	case XK_backslash: case XK_bar: return 0x2a;
	case XK_semicolon: case XK_colon: return 0x29;
	case XK_apostrophe: case XK_quotedbl: return 0x27;
	case XK_comma: case XK_less: return 0x2b;
	case XK_period: case XK_greater: return 0x2f;
	case XK_slash: case XK_question: return 0x2c;
	case XK_Tab: return 0x30;
	case XK_Return: return 0x24;
	case XK_space: return 0x31;
	case XK_BackSpace: return 0x33;
 
	case XK_Delete: return 0x75;
	case XK_Insert: return 0x72;
	case XK_Home: case XK_Help: return 0x73;
	case XK_End: return 0x77;
#ifdef __hpux
	case XK_Prior: return 0x74;
	case XK_Next: return 0x79;
#else
	case XK_Page_Up: return 0x74;
	case XK_Page_Down: return 0x79;
#endif
 
	case XK_Control_L: return 0x36;
	case XK_Control_R: return 0x36;
	case XK_Shift_L: return 0x38;
	case XK_Shift_R: return 0x38;
	case XK_Mode_switch: return 0x37;       /* Mac Command */
	case XK_Meta_L: return 0x37;
	case XK_Meta_R: return 0x37;
	case XK_Alt_L: return 0x3a;	    /* Mac Alt */
	case XK_Alt_R: return 0x3a;
	case XK_Menu: return 0x32;
	case XK_Caps_Lock: return 0x39;
	case XK_Num_Lock: return 0x47;
 
	case XK_Up: return 0x3e;
	case XK_Down: return 0x3d;
	case XK_Left: return 0x3b;
	case XK_Right: return 0x3c;
 
	case XK_Escape: return 0x35;
 
	case XK_F1: return 0x7a;
	case XK_F2: return 0x78;
	case XK_F3: return 0x63;
	case XK_F4: return 0x76;
	case XK_F5: return 0x60;
	case XK_F6: return 0x61;
	case XK_F7: return 0x62;
	case XK_F8: return 0x64;
	case XK_F9: return 0x65;
	case XK_F10: return 0x6d;
	case XK_F11: return 0x67;
	case XK_F12: return 0x6f;
 
	case XK_Print: return 0x69;
	case XK_Scroll_Lock: return 0x6b;
	case XK_Pause: return 0x71;
 
#if defined(XK_KP_Prior) && defined(XK_KP_Left) && defined(XK_KP_Insert) && defined (XK_KP_End)
	case XK_KP_0: case XK_KP_Insert: return 0x52;
	case XK_KP_1: case XK_KP_End: return 0x53;
	case XK_KP_2: case XK_KP_Down: return 0x54;
	case XK_KP_3: case XK_KP_Next: return 0x55;
	case XK_KP_4: case XK_KP_Left: return 0x56;
	case XK_KP_5: case XK_KP_Begin: return 0x57;
	case XK_KP_6: case XK_KP_Right: return 0x58;
	case XK_KP_7: case XK_KP_Home: return 0x59;
	case XK_KP_8: case XK_KP_Up: return 0x5b;
	case XK_KP_9: case XK_KP_Prior: return 0x5c;
	case XK_KP_Decimal: case XK_KP_Delete: return 0x41;
#else
	case XK_KP_0: return 0x52;
	case XK_KP_1: return 0x53;
	case XK_KP_2: return 0x54;
	case XK_KP_3: return 0x55;
	case XK_KP_4: return 0x56;
	case XK_KP_5: return 0x57;
	case XK_KP_6: return 0x58;
	case XK_KP_7: return 0x59;
	case XK_KP_8: return 0x5b;
	case XK_KP_9: return 0x5c;
	case XK_KP_Decimal: return 0x41;
#endif
	case XK_KP_Add: return 0x45;
	case XK_KP_Subtract: return 0x4e;
	case XK_KP_Multiply: return 0x43;
	case XK_KP_Divide: return 0x4b;
	case XK_KP_Enter: return 0x4c;
	case XK_KP_Equal: return 0x51;
	}
	return ~0;
}


/************************************************************************/
/*	The listener							*/
/************************************************************************/

static void
vnc_rcv_connection( int fd, int event )
{
	struct sockaddr_in addr;
	int new_socket;
	int len = sizeof(addr);

	if( !(event & POLLIN) ) {
		printm("vnc_rcv: Event %x\n", event );
		return;
	}

	if( (new_socket=accept(fd, (struct sockaddr*)&addr, (unsigned int *) &len)) < 0 ) {
		perrorm("vnc_accept");
		return;
	}
	if( vnc_sock != -1 ) {
		// there is already a vnc thread running
		LOG("Vnc connection rejected. Connection is already in progress (%d)\n", vnc_sock );
		close( new_socket );
	} else {
		vnc_sock = new_socket;
		vnc_sock_valid = true;
 
		create_thread( vnc_thread, NULL, "VNC-thread");
	}
}

static int
setup_listener( void )
{
	struct sockaddr_in addr;
	int port, sock, one=1;

	if( (sock=socket( AF_INET, SOCK_STREAM, 0 )) < 0 )
		goto bail;

	if( setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char*)&one, sizeof(one)) < 0 )
		goto bail;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	port = get_numeric_res("vnc_port");
	addr.sin_port = (port == -1)? 0 : port;
 
	if( bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0 )
		goto bail;

	if( listen(sock, 5) < 0 )
		goto bail;

	printm("Serving VNC on port %d\n", addr.sin_port);
	vnc_listen_sock = sock;
	async_listener_id = add_async_handler( sock, POLLIN, vnc_rcv_connection, 1 /* SIGIO */ );	

	return 0;
 bail:
	perrorm("VNC socket");
	if( sock >= 0 )
		close( sock );
	return 1;
}


/************************************************************************/
/*	Main thread (module entry points)				*/
/************************************************************************/

static int
vnc_init( video_module_t *m )
{
	const int depths[] = { 8, 16, 24, 32 };
	video_desc_t *vm;
	int i;
 
	if( get_bool_res("enable_vncvideo") != 1 )
		return 1;

	if( setup_listener() )
		return 1;

	vnc_module.modes = vm = calloc( 4, sizeof( video_desc_t ));
	for( i=0; i<4; i++ ) {
		vm[i].offs = -1;
		vm[i].rowbytes = -1;
		vm[i].h = -1;
		vm[i].w = -1;
		vm[i].depth = depths[i];
		vm[i].next = (i+1<4) ? &vm[i+1] : NULL;
	}
	is_open = false;

	return 0;
}

static void
vnc_cleanup( video_module_t *m )
{
	int i;
 
	LOG("vnc_cleanup called\n");

	// shutdown listener
	if( vnc_listen_sock >= 0 ) {
		delete_async_handler( async_listener_id );
		close( vnc_listen_sock );
		vnc_listen_sock = -1;
	}

	if( vnc_sock != -1 ) {
		LOG("Shutting down vnc sock\n");
 
		// the next poll or read will fail and exit the vnc_thread

		shutdown( vnc_sock, 2 );
		vnc_sock_valid = false;
		
		for( i=0 ; i<60 && vnc_sock != -1 ; i++ ) {
			LOG("Waiting for vnc thread\n");
			sleep(1);
		}
	}
	if( translationtable ) {
		free( (void*)translationtable );
		translationtable = NULL;
	}
	
	free( vnc_module.modes );
}

static int
vopen( video_desc_t *org_vm )
{
	LOG( "vopen called\n" );

	vmode = *org_vm;

	if( is_open ) {
		LOG("Open called twice!\n");
		return 1;
	}

	pthread_mutex_lock(&buffers_mutex);
 
	offscreen_buf = (unsigned char *) map_zero( NULL, FBBUF_SIZE(org_vm));

	//output_buf_size = org_vm->w * 4;
	output_buf_size = 10000;
	output_buf = malloc( output_buf_size );
	//LOG( "output_buf = %lx  size = %ld\n", (long) output_buf, output_buf_size );
 
	// Fill in the fields video.c expects
	org_vm->mmu_flags = MAPPING_FB_ACCEL | MAPPING_FORCE_CACHE;
	org_vm->map_base = 0;
	org_vm->lvbase = (char *)offscreen_buf;

	vmode = *org_vm;

	offscreen_bpp = (vmode.depth + 7) / 8;
 
	if( offscreen_bpp == 3 )
		offscreen_bpp = 4;

	_setup_fb_accel( vmode.lvbase+vmode.offs, vmode.rowbytes, vmode.h );

	is_open = true;

	pthread_mutex_unlock(&buffers_mutex);
	return 0;
}

static void
vclose( void )
{
	//LOG( "vclose called\n" );

	if( !is_open )
		return;
 
	is_open = false;

	pthread_mutex_lock(&buffers_mutex);
 
	_setup_fb_accel( NULL, 0, 0 );

	munmap( offscreen_buf, FBBUF_SIZE(&vmode) );
	offscreen_buf = NULL;

	free( output_buf );
	output_buf = NULL;
	output_buf_size = 0;

	pthread_mutex_unlock(&buffers_mutex);
}

static void
vrefresh( void )
{
	force_redraw = true;
}

static void
setcmap( char *pal )
{
	if( vmode.depth != 8 )
		return;
 
	memcpy( mac_pal, pal, 256 * 3 );

	init_translationtable();
}

video_module_t vnc_module  = {
	.name 		= "vnc",
	.vinit		= vnc_init,
	.vcleanup	= vnc_cleanup,
	.vopen		= vopen,
	.vclose		= vclose,
	.vrefresh	= vrefresh,
	.setcmap	= setcmap,
	.modes		= NULL
};
