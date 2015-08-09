/* 
 *   Creation Date: <2003/12/17 19:10:38 samuel>
 *   Time-stamp: <2004/03/27 13:41:28 samuel>
 *   
 *	<kbd.c>
 *	
 *	OSI keyboard interface
 *   
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#include "mol_config.h"
#include "os_interface.h"
#include "keycodes.h"
#include "kbd_sh.h"
#include "promif.h"
#include "pic.h"
#include "booter.h"
#include "mac_registers.h"
#include "driver_mgr.h"

#define KEY_BUF_SIZE	16
#define KEY_BUF_MASK	(KEY_BUF_SIZE - 1)

static struct {
	unsigned char	keybuf[ KEY_BUF_SIZE ];
	unsigned int	rawbuf[ KEY_BUF_SIZE ];
	int		bot, top;
	int		irq;

	int	(*save_receiver)( unsigned char key, int raw );
} ki;

static int
post_osi_key_event( unsigned char key, int raw_keycode )
{
	/* raw_keycode = driver_keycode_id | driver_keycode */

	if( ((ki.top + 1) & KEY_BUF_MASK) == ki.bot )
		return -1;

	ki.keybuf[ki.top] = key;
	ki.rawbuf[ki.top] = raw_keycode;
	ki.top = (ki.top + 1) & KEY_BUF_MASK;

	if( ki.irq )
		irq_line_hi( ki.irq );
	return 0;
}

/* returns raw keycode in r4 (raw_keycode = driver_id | keycode) */
static int
osip_get_adb_key( int sel, int *params )
{
	int ret=-1;

	if( ki.top != ki.bot ) {
		ret = ki.keybuf[ki.bot];
		mregs->gpr[4] = ki.rawbuf[ki.bot];
		ki.bot = (ki.bot + 1) & KEY_BUF_MASK;
	}
	if( ki.irq )
		irq_line_low( ki.irq );
	return ret;
}

static int
osip_kbd_cntrl( int sel, int *params )
{
	int cmd = params[0];

	zap_keyboard();

	switch( cmd ) {
	case kKbdCntrlActivate:
		ki.save_receiver = gPE.adb_key_event;
		gPE.adb_key_event = post_osi_key_event;
		break;
	case kKbdCntrlSuspend:
		if( ki.save_receiver )
			gPE.adb_key_event = ki.save_receiver;
		break;
	}
	return 0;
}

static const osi_iface_t osi_iface[] = {
	{ OSI_GET_ADB_KEY,	osip_get_adb_key	},
	{ OSI_KBD_CNTRL,	osip_kbd_cntrl		},
};


static int
osi_kbd_init( void )
{
	mol_device_node_t *dn;
	irq_info_t irqinfo;
	
	register_osi_iface( osi_iface, sizeof(osi_iface) );

	if( (dn=prom_find_devices("mol-keyboard")) ) {
		if( prom_irq_lookup(dn, &irqinfo) == 1 )
			ki.irq = irqinfo.irq[0];
		gPE.adb_key_event = post_osi_key_event;
	}
	return 1;
}

static void
osi_kbd_cleanup( void )
{
}

driver_interface_t osi_kbd_driver = {
    "osi_kbd", osi_kbd_init, osi_kbd_cleanup
};
