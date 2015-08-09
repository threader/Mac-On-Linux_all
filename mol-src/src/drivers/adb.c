/* 
 *   Creation Date: <2003/01/03 03:08:42 samuel>
 *   Time-stamp: <2004/02/07 18:15:27 samuel>
 *   
 *	<adb.c>
 *	
 *	Apple Desktop Bus emulation
 *   
 *   Copyright (C) 1998-2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include "adb.h"
#include "via-cuda.h"
#include "debugger.h"
#include "driver_mgr.h"
#include "timer.h"
#include "hacks.h"
#include "res_manager.h"
#include "session.h"
#include "booter.h"
#include "input.h"

/* #define ADB_VERBOSE */

/* from <linux,asm/adb.h> */
#define ADB_READREG(id, reg)    (0xc + (reg) + ((id) << 4))
#define ADB_PACKET		0
#define ADB_KEYBOARD		2
#define	ADB_MOUSE		3

/* XXX should a device be polled by CUDA if service_req_enable is FALSE? 
 *
 * XXX: What should the second byte of the CUDA packet contain?
 * If the packet is autopolled from CUDA then it should be 0x40
 * (eg. 00 40 3c 68 44). The question is, if the message is a response
 * of a request, should it still be 40?
 */

typedef struct adb_device adb_device_t;

typedef int	(*adb_read_reg_fp)( adb_device_t *ad, int reg, unsigned char *retbuf );
typedef void	(*adb_write_reg_fp)( adb_device_t *ad, int reg, char *data, int len );
typedef void	(*adb_cleanup_fp)( adb_device_t *ad );

static struct adb_device {
	int			id;
	int			orig_id;
	int			service_req_enable;
	int			service_req;

	struct adb_device	*next;

	adb_read_reg_fp		read_reg_fp;
	adb_write_reg_fp	write_reg_fp;
	adb_cleanup_fp		cleanup_fp;

	void			*usr;
	int			handler_id;
} *root, *kbd, *mouse;

static int			stop_input;

static adb_device_t	*register_adb_device( int id, adb_read_reg_fp, adb_write_reg_fp, adb_cleanup_fp );


/************************************************************************/
/*	ADB mouse							*/
/************************************************************************/

/*
 * (Handler 3)
 *             BITS    COMMENTS
 *  data[0] = dddd 1100 ADB command: Talk, register 0, for device dddd.
 *  data[1] = bxxx xxxx First button and x-axis motion.
 *  data[2] = byyy yyyy Second button and y-axis motion.
 *
 *  For Apple's 3-button mouse protocol the data array will contain the
 *  following values:
 *
 * (Handler 4)
 *              BITS    COMMENTS
 *  data[0] = dddd 1100 ADB command: Talk, register 0, for device dddd.
 *  data[1] = bxxx xxxx Left button and x-axis motion.
 *  data[2] = byyy yyyy Second button and y-axis motion.
 *  data[3] = byyy bxxx Third button and fourth button.  Y is additional
 *            high bits of y-axis motion.  XY is additional
 *            high bits of x-axis motion.
 */

struct mouse_status {
	int	buttons;
	int	dx, dy;
	int	dirty;
};

typedef struct {
	char  		button:1;
	signed char	delta:7;
} mouse_byte_t;


static int
mouse_read_reg( adb_device_t *ad, int reg, unsigned char *retbuf )
{
	struct mouse_status *ms = (struct mouse_status*)ad->usr;
	int len=0, dx, dy;
	mouse_byte_t *mb;

	retbuf[0] = ADB_PACKET;
	retbuf[1] = 0x40;	/* experimentally */
	retbuf[2] = ADB_READREG( ad->id, reg );

	/* printm("mouse_read_reg\n"); */
	switch( reg ){
	case 0: /* mouse data */
		if( !ms->dirty ) {
			ad->service_req = 0;
			/* No package is returned if no mouse event is queued */
			break;
		}
		
		dx = ms->dx;
		dy = ms->dy;
		if( dx > 63 )
			dx = 63;
		else if (dx < -64 ) 
			dx = -64;
		if( dy > 63 ) 
			dy = 63;
		else if (dy < -64 ) 
			dy = -64;
		mb = (mouse_byte_t*)&retbuf[3];
		mb->delta = dy;
		mb->button = (ms->buttons & kButton1)? 1 : 0;
		(++mb)->delta = dx;
		mb->button = (ms->buttons & kButton2)? 1 : 0;
		ms->dx -= dx;
		ms->dy -= dy;

		len = 5;
		if( !ms->dx && !ms->dy ) {
			ms->dirty = ad->service_req = 0;
		}
		break;
	default:
		printm("mouse_read_reg, unexpected regster %d\n", reg );
		retbuf[3] = 0;
		len = 4;
		break;
	}
	return len;
}

static void 
mouse_write_reg( adb_device_t *ad, int reg, char *data, int len )
{
	/* struct mouse_status *ms = (struct mouse_status*)ad->usr; */

	switch( reg ){
	case 0: /* key data (?) */
		printm("Mouse write reg 0\n");
		break;
	case 3: /* control_register (two bytes data guaranteed)  */
		printm("Write of mouse control register (%02X %02X)\n",
		       data[0], data[1]);
		break;
	default:
		printm("mouse_write_reg, unexpected register %d\n", reg );
		break;
	}
}

/* the event is always two byte long */

static void 
adb_mouse_event( int dx, int dy, int buts )
{
	struct mouse_status *ms;

	/* Currently unused. */
	printm("adb_mouse_event: FIXME - set mousemoved stamp!\n");

	/* printm("Mouse event: dx = %d, dy = %d, buts = %X\n",dx, dy, buts ); */
	
	if( !mouse )
		return;
	ms = (struct mouse_status*) mouse->usr;

	ms->buttons = ~buts & kButtonMask;
	ms->dx += dx;
	ms->dy += dy;

	/* printm("delta: %d %d\n", ms->dx, ms->dy ); */

	ms->dirty = mouse->service_req = 1;

	/* notify CUDA there is a package ready */
	if( cuda_autopolls() && !stop_input /* && mouse->service_req_enable */  ) 
		adb_service_request();
}

static void
mouse_cleanup( adb_device_t *ad )
{
	if( ad->usr )
		free( ad->usr );
}

static void
mouse_init( void )
{
	mouse = register_adb_device( ADB_MOUSE, mouse_read_reg, mouse_write_reg, 
				     mouse_cleanup );
	if( mouse ) {
		/* Apple standard 1-button mouse uses handler 3 */
		mouse->handler_id = 0x3;
		mouse->usr = (void*) calloc( sizeof(struct mouse_status),1 );
	}
}

/************************************************************************/
/*	ADB keyboard							*/
/************************************************************************/

/*
 * REG 0:
 * data[0] = dddd 1100 ADB command: Talk, register 0, for device dddd.
 * data[1] = uxxx xxxx	key upp and scancode
 * data[2] = uxxx xxxx 	key upp and scancode (0x7f == nothing?)
 *
 * REG 2: 
 * data[0] = dddd 1110 ADB command: Talk, register 2
 * data[1] = xxxx xxxx  pressed keys (0=pressed),
 *			CMD 		0x1
 *			LALT/RALT	0x2
 *			SHIFT		0x4
 *			CTRL		0x8
 *			CAPSL		0x44
 *			TAB		0x4	(?)
 * data[2] = xxxx xxxx leds (0=lighted)
 */

#define KEY_BUF_SIZE	16
#define KEY_BUF_MASK	(KEY_BUF_SIZE-1)

typedef struct kbd_status {
	unsigned char	buf[KEY_BUF_SIZE];
	int		buf_bot, buf_top;
} kbd_status_t;

static int
kbd_read_reg( adb_device_t *ad, int reg, unsigned char *retbuf )
{
	struct kbd_status *ks = (struct kbd_status*)ad->usr;
	int i, len;

	len = 0;
	retbuf[0] = ADB_PACKET;
	retbuf[1] = 0x40;	/* experimentally */
	retbuf[2] = ADB_READREG( ad->id, reg );


	/* printm("kbd_read_reg\n"); */

	switch( reg ){
	case 0: /* key data */
		retbuf[3] = retbuf[4] = 0xff;
		for(i=3; i<=4; i++ )
			if( ks->buf_bot != ks->buf_top ) {
				retbuf[i] = ks->buf[ ks->buf_bot++ ];
				ks->buf_bot &= KEY_BUF_MASK;
			}
		if( ks->buf_bot == ks->buf_top )
			ad->service_req = 0;
		if( retbuf[3] != 0xff )
			len = 5;
		/* No package should be returned if no key event is queued */
		break;
	case 2: /* XXX: Here we should reply led + modifier status */
		retbuf[3] = 0xff;
		retbuf[4] = 0xff;
		len = 5;
		break;		
	default:
		printm("KBD: kbd_read_reg, unexpected regster %d\n", reg );
		len = 3;
		break;
	}
	return len;
}

static void 
kbd_write_reg( adb_device_t *ad, int reg, char *data, int len )
{
	switch( reg ){
	case 0: /* key data (?) */
		printm("Keyboard write reg 0\n");
		break;
	case 2: /* led register */
		printm("Keyboard write LED register\n");
		break;
	case 3: /* control_register (two bytes data guaranteed)  */
		if( len != 2 )
			return;
		switch( data[1] ){
#if 0
		case 2: /* handler 2, left/right keys same keycodes */ 
		case 3: /* handler 3, left/right leys different keycodes */
			ad->handler_id = data[1];
			break;
#endif
		default:
			break;
		}
		break;
	}
}

static void
kbd_cleanup( adb_device_t *ad )
{
	if( ad->usr )
		free( ad->usr );
}

static void 
kbd_init( void ) 
{
	struct kbd_status *ks;
	kbd = register_adb_device( ADB_KEYBOARD, kbd_read_reg, kbd_write_reg, 
				   kbd_cleanup);
	if( kbd ) {
		if ((kbd->handler_id = get_numeric_res("keyboard_id")) == -1)
			kbd->handler_id = 2; /* Apple Extended Keyboard (US) */
		ks = (kbd_status_t*) calloc( sizeof(kbd_status_t),1 );
		kbd->usr = (void*)ks;
	}
}


static int
adb_key( unsigned char key, int raw )
{
	struct kbd_status *ks;
	int ret = 0;
	
	/* until video is up running, disregard key presses 
	 * (early events, sometimes confuse the CUDA)
	 */
	if( !kbd || !mac_video_initialized() )
		return -1;
	
	ks = (struct kbd_status*) kbd->usr;

	/* Never let 0xff through (0xff has a special meaning) */
	if( key == 0xff )
		return -1;

	/* need to handle 0x7f (start button) specially */
	if( (key & 0x7f) == 0x7f ) {
		/* A double 0x7f seems to be the start button */
		if( raw != 0 )
			adb_key( key, 0 );
	}

	/* queue key */
	if( ((ks->buf_top - ks->buf_bot ) & KEY_BUF_MASK) + 1 < KEY_BUF_SIZE ){
		ks->buf[ks->buf_top++] = key;
		ks->buf_top &= KEY_BUF_MASK;		

		kbd->service_req = 1;
	} else {
		/* printm("adb_key: buffer overflow\n"); */
		ret = -1;
	}

	/* notify CUDA there is a package ready */
	if( cuda_autopolls() && !stop_input /* && kbd->service_req_enable */  )
		adb_service_request();

	return ret;
}


/************************************************************************/
/*	Generic ADB implementation					*/
/************************************************************************/

static adb_device_t * 
register_adb_device( int id, adb_read_reg_fp rfp, adb_write_reg_fp wfp, adb_cleanup_fp cfp ) 
{
	adb_device_t *ad;
	
	ad = calloc( sizeof(adb_device_t),1 );
	ad->id = ad->orig_id = id;
	ad->read_reg_fp = rfp;
	ad->write_reg_fp = wfp;
	ad->cleanup_fp = cfp;
	ad->next = root;
	root = ad;

	return ad;
}

static struct adb_device *
find_adb_device( int id )
{
	adb_device_t *ad;

	for(ad=root; ad; ad=ad->next ){
		if( ad->id == id )
			return ad;
	}
	return NULL;
}

void
adb_poll( void )
{
	unsigned char buf[32];
	adb_device_t *ad;
	int len;

	/* printm("adb_poll\n"); */

	for( ad=root; ad; ad=ad->next ) {
		if( /* ad->service_req_enable && */ ad->service_req ){
			if( !ad->read_reg_fp )
				continue;
			if( (len = ad->read_reg_fp( ad, 0, buf )) != 0 ) {
#if 0
				{
					int i;
					printm("ADB event packet: ");
					for(i=0; i< len; i++)
						printm("%02X",buf[i] );
					printm("\n");
				}
#endif
				via_cuda_set_reply_buffer( (char *) buf, len );
				break;
			} else {
				/* this is a programming error in adb device driver */
				printm("ADB service request -- BUT NO DATA!\n");
			}
		}
	}
}

int 
adb_mes_avail( void )
{
	adb_device_t *ad;

	for( ad=root; ad; ad=ad->next )
		if( /* ad->service_req_enable && */ ad->service_req && ad->read_reg_fp )
			return 1;
	return 0;
}


void 
adb_packet( int cmd, unsigned char *data, int len )
{
	int id = (cmd >> 4) & 0xf;
	int reg = (cmd & 0x3);
	adb_device_t *ad;
	unsigned char buf[32];

	/* ADB_BUSRESET */
	if( !(cmd & 0xf) ) {
		/* printm("ADB_BUSRESET\n"); */
		goto out;
	}

	if( !(ad = find_adb_device( id )) )
		goto out;

	/* ADB_FLUSH */
	if( (cmd & 0xf) == 1 ){
		/* printm("ADB_FLUSH, id %d\n", id );*/
		goto out;
	}

	/* ADB_READREG */
	if( (cmd & 0xc) == 0xc ){
		if( len ) {
			printm("ADB: unexpected len of read request\n");
			goto out;
		}
		/* we perform some register 3 services here for simplicity */
		if( reg == 3 ){
			unsigned char b1;
			b1 = ad->service_req_enable ? 0x60 : 0x40;
			b1 |= random() % 0xf;
			if (ad->orig_id != ADB_KEYBOARD)
				via_cuda_reply( 5, ADB_PACKET, 0x40, cmd, b1, ad->handler_id );
			else
				via_cuda_reply( 5, ADB_PACKET, 0, cmd, b1, ad->handler_id );
			return;
		}
		if( ad->read_reg_fp )
			len = ad->read_reg_fp( ad, reg, buf );
		via_cuda_set_reply_buffer( (char *) buf, len );
		return;
	}
	/* ADB_WRITEREG */
	if( cmd & 0x8 ){
		/* we perform some register 3 services here for simplicity */
		if( reg == 3 ) {
			if( len!=2 ) {
				printm("ADB: Unexpected len of write request\n");
				goto out;
			}
#ifdef ADB_VERBOSE
			printm("write adb reg 3: %02X %02X\n", data[0], data[1] );
#endif
			ad->service_req_enable = (data[0] & 0x20) ? 1:0;

			switch( data[1] ){
			case 0xff: /* self test */
				printm("ADB: Performing selftest.........DONE\n");
				goto out;
			case 0xfe: /* change ADB id, if no collision was detected */
				/* ...first device in linked list never
				 * detects collisions, other always do...
				 */
				ad->id = data[0] & 0xf;
				goto out;
			case 0xfd: /* change ADB id, "if the activator is pressed" */
				printm("ADB: Change adb ID, if activator is pressed???\n");
				goto out;
			case 0: /* change id (all devices) */
				if( ad->id != (data[0] & 0xf) ) {
					adb_device_t *p = ad;
					while(p){
						p->id = data[0] & 0xf;
						p = find_adb_device( id );
					}
				}
				ad->id = data[0] & 0xf;
				goto out;
			}
		}
		if( ad->write_reg_fp )
			ad->write_reg_fp( ad, reg, (char *) data, len );
		goto out;
	}
	printm("Unknown ADB command %02X\n", cmd );

out:
	/* default to reply command byte */
	via_cuda_reply( 3, ADB_PACKET, 0x40, cmd );
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static int
prepare_save( void )
{
	/* make sure no new mouse and keypresses are sent to the CUDA */
	stop_input = 1;
	return 0;
}

void
adb_init( void )
{
	kbd_init();
	mouse_init();

	/* Export mouse/keyboard interface */
	if( !gPE.adb_key_event )
		gPE.adb_key_event = adb_key;
	if( !gPE.mouse_event )
		gPE.mouse_event = adb_mouse_event;

	session_save_proc( NULL, prepare_save, kDynamicChunk );
}

void
adb_cleanup( void )
{
	adb_device_t *ad, *next;

	for( ad=root; ad; ad=next ){
		if( ad->cleanup_fp )
			ad->cleanup_fp( ad );
		next = ad->next;
		free( ad );
	}
}
