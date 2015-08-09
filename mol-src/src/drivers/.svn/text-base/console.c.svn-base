/* 
 *   Creation Date: <2002/09/29 03:35:05 samuel>
 *   Time-stamp: <2004/02/07 18:44:33 samuel>
 *   
 *	<console.c>
 *	
 *	Initialization of screen and keyboard
 *   
 *   Copyright (C) 1997-2004 Samuel Rydh <samuel@ibrium.se>
 *   
 *   Inspired from the ppc-code in XPmac, with the following 
 *   copyrights:
 *
 *     Copyright 1988-1993 by Apple Computer, Inc., Cupertino, CA
 *     Copyright (c) 1987, 1999 by the Regents of the University of California
 *     Copyright 1987 by Sun Microsystems, Inc. Mountain View, CA.
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include <signal.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <termios.h>
#include <linux/vt.h>          
#include <linux/kd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <stdarg.h>

#include "verbose.h"
#include "console.h"
#include "debugger.h"
#include "timer.h"
#include "driver_mgr.h"
#include "res_manager.h"
#include "mouse_sh.h"
#include "session.h"
#include "async.h"
#include "keycodes.h"
#include "booter.h"
#include "molcpu.h"
#include "input.h"
#include "drivers.h"

SET_VERBOSE_NAME("console");

#define 	VT_RELEASE_SIGNAL	40
#define 	VT_ACQUIRE_SIGNAL	41

static struct {
	int	activate_pending;

	int	acquire_token;
	int	release_token;
	int	has_console;

	int	vt_no;
	int	vt_orig;

	int	fd;
	int	initialized;
} vt;


/************************************************************************/
/*	keyboard							*/
/************************************************************************/

static struct {
	struct termios save_tio;

	int	handler_id;
	int	console_mouse;

	int	initialized;
} kbd;

static int
fkey_action( int dummy_key, int fkey )
{
	if( !fkey )
		fkey = vt.vt_orig;
	if( fkey != vt.vt_no && !vt.activate_pending ) {
		ioctl( vt.fd, VT_ACTIVATE, fkey );
		vt.activate_pending = 1;
	}
	return 1;
}

#define MODS	KEY_SET2( KEY_COMMAND, KEY_ALT )

static key_action_t console_keys[] = {
	{ MODS, KEY_CTRL, KEY_F1,		fkey_action, 1 },
	{ MODS, KEY_CTRL, KEY_F2,		fkey_action, 2 },
	{ MODS, KEY_CTRL, KEY_F3,		fkey_action, 3 },
	{ MODS, KEY_CTRL, KEY_F4,		fkey_action, 4 },
	{ MODS, KEY_CTRL, KEY_F5,		fkey_action, 5 },
	{ MODS, KEY_CTRL, KEY_F6,		fkey_action, 6 },
	{ MODS, KEY_CTRL, KEY_F7,		fkey_action, 7 },
	{ MODS, KEY_CTRL, KEY_F8,		fkey_action, 8 },
	{ MODS, KEY_CTRL, KEY_F9,		fkey_action, 9 },
	{ MODS, KEY_CTRL, KEY_F10,		fkey_action, 10 },
	{ MODS, KEY_CTRL, KEY_F11,		fkey_action, 11 },
	{ MODS, KEY_CTRL, KEY_F12,		fkey_action, 12 },

	{ KEY_COMMAND, KEY_CTRL, KEY_ENTER,	fkey_action, 0 },
	{ KEY_COMMAND, KEY_CTRL, KEY_RETURN,	fkey_action, 0 },
	{ KEY_COMMAND, KEY_CTRL, KEY_SPACE,	fkey_action, 0 },
};

static void 
console_key_event( int fd, int dummy_events )
{
	unsigned char events[64];
	int i, n, nn;

	n = read( fd, events, sizeof(events)-4 );

	for( i=0; i<n; i++ ) {
		if( uses_linux_keycodes() && (events[i] & 0x7f) == 0x00 /* extended */ ) {
			while( i+3 > n ) {
				if( (nn=read(fd, events+n, i+3-n )) <= 0 )
					break;
				n += nn;
			}
			key_event( kConsoleKeytable, CONSOLE_RAW_KEYCODE | ((events[i+1]&0x7f)<<7) | (events[i+2]&0x7f), !(events[i] & 0x80) );
			i+=2;
			continue;
		}
		if( !uses_linux_keycodes() && (events[i] & 0x7f) == 0x7e /* mouse */ ) {
			while( i+3 > n ) {
				if( (nn=read(fd, events+n, i+3-n )) <= 0 )
					break;
				n += nn;
			}
			/* queue mouse event */
			if( kbd.console_mouse ) {
				signed char dx = events[i+2] << 1;
				signed char dy = events[i+1] << 1;
				PE_mouse_event( dx/2, dy/2, ((events[i+1]&0x80)? 0:1) );
			}
			i+=2;
			continue;
		}
		key_event( kConsoleKeytable, CONSOLE_RAW_KEYCODE | (events[i]&0x7f), !(events[i] & 0x80) );
	}
}

static int
set_raw_kbd( void )
{
	struct termios tio;

	if( !kbd.initialized )
		return 1;

	ioctl( vt.fd, KDSETMODE, KD_GRAPHICS );
	if( uses_linux_keycodes() )
		ioctl( vt.fd, KDSKBMODE, K_MEDIUMRAW );
	else
		ioctl( vt.fd, KDSKBMODE, K_RAW );

	tio = kbd.save_tio;
	tio.c_iflag = (IGNPAR | IGNBRK) & (~PARMRK) & (~ISTRIP);
	tio.c_oflag = tio.c_lflag = 0;
	tio.c_cflag = CREAD | CS8 | B9600;
	tio.c_cc[VTIME]=0;
	tio.c_cc[VMIN]=1;

	if( tcsetattr(vt.fd, TCSANOW, &tio ) < 0 ) {
		perrorm("tcsetattr");
		return 1;
	}
	return 0;
}

static int
set_text_kbd( void )
{
	if( !kbd.initialized )
		return 1;

	ioctl( vt.fd, KDSETMODE, KD_TEXT );
	ioctl( vt.fd, KDSKBMODE, K_XLATE );
	/* reset palette */
	/* write( vt.fd, "\033]R", 3); */
	tcsetattr( vt.fd, TCSANOW, &kbd.save_tio );

	return 0;
}

void
console_make_safe( void )
{
	set_text_kbd();
}

void
console_set_gfx_mode( int flag )
{
	if( flag )
		set_raw_kbd();
	else
		set_text_kbd();
}

static void
keyboard_init( void )
{
	memset( &kbd, 0, sizeof(kbd) );

	if( tcgetattr( vt.fd, &kbd.save_tio ) < 0 )
		fatal("tcgetattr");

	kbd.handler_id = add_async_handler( vt.fd, POLLIN, console_key_event, 1 /* use SIGIO */ );
	kbd.initialized = 1;

	register_key_table( kConsoleKeytable, 0, MOL_KEY_MAX );
	user_kbd_customize( kConsoleKeytable );

	set_text_kbd();
}

static void
keyboard_cleanup( void ) 
{
	if( !kbd.initialized )
		return;
	
	delete_async_handler( kbd.handler_id );
	set_text_kbd();
	kbd.initialized = 0;
}

/************************************************************************/
/*	VT switching							*/
/************************************************************************/

int
console_to_front( void )
{
	VPRINT("Console_to_front\n");
	
	if( !vt.initialized )
		return 1;

	if( !vt.has_console && !vt.activate_pending ) {
		VPRINT("Sending VT_ACTIVATE\n");
		ioctl( vt.fd, VT_ACTIVATE, vt.vt_no );
	}
	return 0;
}

static void
vt_switch_aevent( int token )
{
	if( token == vt.acquire_token ) {
		if( vt.has_console ) {
			printm("vt_acquire called, but has_console is true!\n");
			return;
		}
		VPRINT("Grabbing console\n");

		vt.has_console = 1;
		ioctl( vt.fd, VT_RELDISP, 2 );

		fbdev_vt_switch(1);
		kbd.console_mouse = mouse_activate(1);
		
		add_key_actions( console_keys, sizeof(console_keys) );
		VPRINT("We are now the owner of the console\n");
	} else {
		if( !vt.has_console ) {
			printm("vt_release called with !has_console\n");
			return;
		}
		VPRINT("Giving up console\n");

		mouse_activate(0);
		fbdev_vt_switch(0);

		ioctl( vt.fd, VT_RELDISP, 1 );
		vt.has_console = 0;

		remove_key_actions( console_keys, sizeof(console_keys) );
		VPRINT("We no longer own the console...\n");
	}
	zap_keyboard();
	vt.activate_pending = 0;
}

/* signal handler for virtual terminal switching */
static void 
vt_switch_signal( int sig )
{
	int token = (sig == VT_ACQUIRE_SIGNAL)? vt.acquire_token : vt.release_token;
	send_aevent( token );
}

static void
setup_vt_switching( void )
{
	struct vt_mode vm;
	sigset_t set;
	int err;

	vt.has_console = 0;
	vt.release_token = add_aevent_handler( vt_switch_aevent );
	vt.acquire_token = add_aevent_handler( vt_switch_aevent );

	signal( VT_RELEASE_SIGNAL, vt_switch_signal );
	signal( VT_ACQUIRE_SIGNAL, vt_switch_signal );
	sigemptyset( &set );
	sigaddset( &set, VT_RELEASE_SIGNAL );
	sigaddset( &set, VT_ACQUIRE_SIGNAL );
	pthread_sigmask( SIG_UNBLOCK, &set, NULL );

	if( (err=ioctl( vt.fd, VT_GETMODE, &vm )) >=0 ){
		vm.mode = VT_PROCESS;
		vm.relsig = VT_RELEASE_SIGNAL;
		vm.acqsig = VT_ACQUIRE_SIGNAL;
		
		err = ioctl( vt.fd, VT_SETMODE, &vm );
	}
	if( err < 0 ) {
		printm("VT_GETSETMODE");
		exit(1);
	}
}


/************************************************************************/
/*	VT allocation							*/
/************************************************************************/

static int 
vt_open( void )
{
	struct vt_stat vts;
	char vtname[32], vtname2[32];
	int vt_no = get_numeric_res( "vt" ); 	/* -1 if missing */
	int fd;
	
	if( (fd=open("/dev/tty0", O_WRONLY)) < 0 && 
            (fd=open("/dev/vc/0", O_WRONLY)) < 0 ) {
		printm("failed to open /dev/tty0 or /dev/vc/0: %s\n",
		       strerror(errno));
		return 1;
	}

	/* find original vt */
	if( ioctl(fd, VT_GETSTATE, &vts) < 0 )
		return 1;
	vt.vt_orig = vts.v_active;

	/* make sure the user specified a free vt */
	if( (vt_no > 0 && (vts.v_state & (1 << vt_no ))) || vt_no > 31 ) {
		LOG("vt%d is already in use\n", vt_no );
		vt_no = -1;
	}

	/* find free console? */
	if( vt_no <= 0 )
		if( (ioctl(fd, VT_OPENQRY, &vt_no) < 0) || (vt_no == -1) ) {
			LOG("Cannot find a free VT\n");
			return 1;
		}
	close(fd);

	/* detach old tty */
	setpgrp();
       	if( (fd=open("/dev/tty",O_RDWR)) >= 0 ) {
		ioctl( fd, TIOCNOTTY, 0 );
		close(fd);
	}

	/* open our vt */
	sprintf( vtname, "/dev/tty%d", vt_no );
	if( (fd=open(vtname, O_RDWR | O_NONBLOCK)) < 0 ) {
		sprintf( vtname2, "/dev/vc/%d", vt_no );
		if( (fd=open(vtname2, O_RDWR | O_NONBLOCK)) < 0 ) {
			perrorm("Cannot open %s or %s", vtname, vtname2 );
			return 1;
		}
	}
	
	/* set controlling tty */
	if( ioctl(fd, TIOCSCTTY, 1) < 0 )
		perrorm("Error setting controlling tty");

	vt.fd = fd;
	vt.vt_no = vt_no;
	return 0;
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

int
console_init( void )
{
	if( vt_open() )
		fatal("Failed opening a VT\n");

	printm("Fullscreen video on VT %d.\n", vt.vt_no);

	keyboard_init();	
	setup_vt_switching();
	vt.initialized = 1;
	return 1;
}

void
console_cleanup( void )
{
	struct vt_mode vm;
	int err;
	
	keyboard_cleanup();

	if( !vt.initialized )
		return;

	/* Switch back to the original console. First, we make sure
	 * the console has been put in "AUTO" mode so no confirmation
	 * is expected.
	 */
	if( (err=ioctl( vt.fd, VT_GETMODE, &vm )) >=0 ){
		vm.mode = VT_AUTO;
		vm.relsig = 0;
		vm.acqsig = 0;
		err = ioctl( vt.fd, VT_SETMODE, &vm );
	}
	if( err )
		LOG_ERR("VT_GETSETMODE");

	ioctl( vt.fd, VT_ACTIVATE, vt.vt_orig );
	ioctl( vt.fd, VT_WAITACTIVE, vt.vt_orig );

	/* OK... the console should now be usable again */
	close( vt.fd );

	vt.initialized = 0;
}
