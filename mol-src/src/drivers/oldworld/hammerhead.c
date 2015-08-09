/* 
 *   Creation Date: <1999/03/05 13:58:16 samuel>
 *   Time-stamp: <2003/06/02 18:26:03 samuel>
 *   
 *	<hammerhead.c>
 *	
 *	Memory controller (7500,8500,9500,...)
 *   
 *   Copyright (C) 1999, 2000, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "verbose.h"
#include "promif.h"
#include "ioports.h"
#include "debugger.h"
#include "mac_registers.h"
#include "memory.h"
#include "driver_mgr.h"
#include "timer.h"
#include "molcpu.h"

/* 
 * NOTE:
 *   It appears we don't have to mess with the hammerhead, except 
 *   making sure that the 0xf8000000 register holds the correct 
 *   identification byte. 
 *
 *   Correction: If we add a control device to the PCI bus, then
 *   we must take a closer look at the hammerhead.
 */


#define HAMMERHEAD_SIZE		0x800
#define G3_CLOCK_FIX_HACK

static struct {
	ulong		mregs;		/* register base (0xF8000000) */
} hs;


#ifdef G3_CLOCK_FIX_HACK
/* 
 * Hack of the 8500/8200/7200 ROM to fix the G3 calibration
 * of the DEC/TB.
 *
 *  0x1104 = processor frequency (we fix this too altough it is not important)
 *  0x1108 = timebase-frequency   (DEC)
 *  0x110c = timebase-frequency/4 (TB)
 */
static inline void 
g3_clock_sync_hack( void )
{
	ulong *lvptr = (ulong*)&ram.lvbase[0x1100];
	ulong tb_freq = get_timebase_frequency();

	lvptr[1] = get_cpu_frequency();
	lvptr[2] = tb_freq;
	lvptr[3] = tb_freq/4;
}
#endif


/************************************************************************/
/*	I / O								*/
/************************************************************************/

static ulong 
reg_io_read( ulong addr, int len, void *usr )
{
	int reg = (addr - hs.mregs)>>4;
	ulong buf[2], ret;

	if( addr & 0xf )
		printm("hammerhead: Unsupported access!\n");

	buf[0] = 0;
	switch( reg ){
	case 0: 
		/* Identification register. Only the MSB seems to be
		 * used. The value is from a 8500/150.
		 */
		buf[0] = 0x39000000;
		break;
	case 2:
		/* This register is necessary if we register the control
		 * device on the chaos pci bus. Conversely, if we define
		 * this register, we must add a control entry...
		 */
		/* buf[0] = 0x90000020; */
		break;
	case 0xe:
#ifdef G3_CLOCK_FIX_HACK
		/* This hack fixes the calibration of the DEC/TB
		 * (mostly needed on the G3, but useful for other machines too).
		 * This applies to the 8200/8500/7200 ROM.
		 */
		if( mregs->nip == 0xfff03bd4 ) {
			printm("[CLOCK_FIX_HACK]\n");
			g3_clock_sync_hack();
		}
#endif
		break;

	case 0x1c:	/* control/status */
	case 0x4d:	/* base  */
		/* ... */
	case 0x4e:	/* control/status */
	case 0x4f:	/* base */
		break;
	}
	ret = read_mem( (char*)buf+(addr&3), len );

	return ret;
}

static void 
reg_io_write( ulong addr, ulong data, int len, void *usr )
{
	if( addr & 0xf )
		printm("hammerhead: Unsupported access!\n");
}

static void 
reg_io_print( int isread, ulong addr, ulong data, int len, void *usr )
{	
	printm("hammerhead: %08lx (len=%d) %s %08lX\n", addr, len,
	       isread ? "read: " : "write:", data );
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static io_ops_t ops = {
	.read		= reg_io_read,
	.write		= reg_io_write,
	.print		= reg_io_print
};

static int
hammerhead_init( void )
{
	mol_device_node_t *dn;
	int *ip, rl;

	if( !(dn=prom_find_devices("hammerhead")) )
		return 0;
	if( !(ip=(int*)prom_get_property(dn, "reg", &rl)) || rl != sizeof(int[2]) || ip[1] != HAMMERHEAD_SIZE ) {
		printm("hammerhead: Bad reg property\n");
		return 0;
	}
	hs.mregs = ip[0];

	add_io_range( hs.mregs, HAMMERHEAD_SIZE, "hammerhead", 0, &ops, NULL );
	return 1;
}

static void 
hammerhead_cleanup( void )
{
}


driver_interface_t hammerhead_driver = {
    "hammerhead", hammerhead_init, hammerhead_cleanup
};
