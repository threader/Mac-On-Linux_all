/* 
 *   Creation Date: <1999/12/22 22:47:48 samuel>
 *   Time-stamp: <2003/04/29 23:14:12 samuel>
 *   
 *	<mouse.c>
 *	
 *	Mouse interface
 *   
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "os_interface.h"
#include "driver_mgr.h"
#include "mac_registers.h"
#include "mouse_sh.h"
#include "debugger.h"
#include "timer.h"
#include "res_manager.h"
#include "booter.h"
#include "timer.h"
#include "input.h"
#include "promif.h"
#include "pic.h"
#include "osi_driver.h"
#include "pci.h"
#include "async.h"
#include "poll_compat.h"

static struct {
	int	irq;
	int	dpi;

	int	dx, dy;
	int	absx, absy, absw, absh;
	int	cur_buts;

	int	hwsw_switch;			/* hardware/software cursor switch */

	/* hardware driver */
	int	fd;
	int	handler_id;
	void	(*driver)( int fd, int events );
} m;


static void
event_occured( void )
{
	if( m.irq > 0 )
		irq_line_hi( m.irq );
}

void
mouse_move_to( int x, int y, int width, int height )
{
	m.absx = x;
	m.absy = y;
	m.absw = width;
	m.absh = height;

	event_occured();
}

void
mouse_move( int dx, int dy )
{
	m.dx += dx;
	m.dy += dy;

	event_occured();
}

void
mouse_but_event( int butevent )
{
	m.cur_buts |= (butevent & 7);
	m.cur_buts &= ~((butevent >> 3) & 7 );

	event_occured();
};

/* but bits: ..00000321. 1=pressed */ 
void
mouse_event( int dx, int dy, int buts )
{
	m.dx += dx;
	m.dy += dy;

	m.cur_buts = buts & 7;
	event_occured();
}

void
mouse_hwsw_curs_switch( void )
{
	m.hwsw_switch = 1;
	event_occured();
}


/************************************************************************/
/*	OSI Interface							*/
/************************************************************************/

static int
osip_ack_mouse_irq( int sel, int *params )
{
	return irq_line_low( m.irq );
}

static int
osip_get_mouse( int sel, int *params )
{
	osi_mouse_t *ret_pb = (void*)&mregs->gpr[4];	/* return in r4-r7 */
	int event;
	
	event = m.cur_buts;

	if( m.dx || m.dy ) {
		ret_pb->x = m.dx;
		ret_pb->y = m.dy;
		event |= kMouseEvent_MoveDelta;
	} else if( m.absx >= 0 ) {
		ret_pb->x = m.absx;
		ret_pb->y = m.absy;
		ret_pb->width = m.absw;
		ret_pb->height = m.absh;
		event |= kMouseEvent_MoveTo;
	}
	if( m.hwsw_switch ) {
		m.hwsw_switch = 0;
		event |= kMouseEvent_HwSwCursSwitch;
	}
	ret_pb->event = event;

	m.dx = m.dy = 0;
	m.absx = -1;

	return irq_line_low( m.irq );
}

static int
osip_mouse_cntrl( int sel, int *params )
{
	switch( params[0] ) {
	case kMouseGetDPI:
		return m.dpi;
	case kMouseRouteIRQ:
		oldworld_route_irq( params[1], &m.irq, "mouse" );
		break;
	default:
		return 1;
	}
	return 0;
}


/************************************************************************/
/*	hardware drivers						*/
/************************************************************************/

static void 
mdrvr_ps2( int fd, int dummy_events )	/* 0b00yx0321, dx, -dy, [dz] ; x/y = sign of dx/dy, 1=pressed */
{
	static signed char buf[3];
	static int partial_cnt;
	int n;

	n = 3 - partial_cnt;
	if( n && (n=read( fd, &buf[partial_cnt], n )) > 0 )
		partial_cnt += n;

	if( partial_cnt == 3 ) {
		int dx, dy;
		dx = !(buf[0] & 0x10) ? buf[1] : -(~buf[1]+1);
		dy = !(buf[0] & 0x20) ? buf[2] : -(~buf[2]+1);			
		PE_mouse_event( dx, -dy, buf[0] & kButtonMask );
		partial_cnt = 0;
	}
}

static void
mdrvr_adb( int fd, int dummy_events )	/* 0x10000123, dx, dy - 0=pressed */
{
	signed char buf[3];

	if( read( fd, buf, 3 ) != 3 )
		return;
	buf[0] = ((buf[0] & 1) << 2) | ((buf[0] & 4) >> 2) | (buf[0] & 2);
	PE_mouse_event( buf[1], -buf[2], ~buf[0] & kButtonMask );
}

static struct {
	char 		*name, *device;
	void 		(*drvr)( int fd, int events );
} mdrvr_table[] = { 
	{ "USB", 	"/dev/input/mice",	mdrvr_ps2 },
	{ "console", 	NULL,			NULL },
	{ "ps2", 	"/dev/input/mice",	mdrvr_ps2 },
	{ "adb", 	"/dev/adbmouse",	mdrvr_adb }
};

static void
mouse_driver_init( void ) 
{
	int n = sizeof(mdrvr_table)/sizeof(mdrvr_table[0]);
	char *s, *mdev;
	int ind, fd=-1;
	
	if( !(s=get_str_res("mouse_protocol")) ) {
		s = "usb";
	}
	for( ind=0; ind<n ; ind++ )
		if( !strcasecmp( s, mdrvr_table[ind].name ) )
			break;
	if( ind == n )
		ind = 0;
	m.driver = mdrvr_table[ind].drvr;
	mdev = mdrvr_table[ind].device;

	if( m.driver ) {
		if( (s=get_str_res("mouse_device")) ) {
			mdev = s;
			if( (fd=open( mdev, O_RDONLY )) < 0 )
				perrorm("---> Failed opening mouse device '%s'", s );
		}
		if( fd < 0 ) {
			if( (fd=open( mdev, O_RDONLY )) < 0 ) {
				perrorm("---> Failed opening '%s'", mdev );
				m.driver = NULL;
			}
		}
	}
	if( !m.driver )
		return;

	printm("Using %s mouse on %s\n", mdrvr_table[ind].name, mdev );
	m.fd = fd;
	m.handler_id = add_async_handler( fd, 0, m.driver, 0 );
}

int
mouse_activate( int activate )
{
	if( !m.driver )
		return 1;
	set_async_handler_events( m.handler_id, activate ? POLLIN : 0 );
	return 0;
}

/************************************************************************/
/*	Init / Cleanup							*/
/************************************************************************/

static int 
mouse_init( void )
{
	static pci_dev_info_t pci_config = { 0x1000, 0x0003, 0x02, 0x0, 0x0100 };
	memset( &m, 0, sizeof(m) );
	
	if( !register_osi_driver( "misc", "mol-mouse", is_classic_boot()? &pci_config : NULL, &m.irq ) )
		return 0;

	if( get_bool_res("use_adbmouse") != 1 )
		gPE.mouse_event = mouse_event;		
	
	if( (m.dpi=get_numeric_res("mouse_dpi")) <= 0 )
		m.dpi = 160;

	mouse_driver_init();
	
	os_interface_add_proc( OSI_GET_MOUSE, osip_get_mouse );
	os_interface_add_proc( OSI_ACK_MOUSE_IRQ, osip_ack_mouse_irq );
	os_interface_add_proc( OSI_MOUSE_CNTRL, osip_mouse_cntrl );
	return 1;
}

static void 
mouse_cleanup( void )
{
	if( m.driver ) {
		delete_async_handler( m.handler_id );
		close( m.fd );
	}
}

driver_interface_t osi_mouse_driver = {
	"osi_mouse", mouse_init, mouse_cleanup
};
