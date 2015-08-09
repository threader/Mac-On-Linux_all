/* 
 *   Creation Date: <1999/03/18 03:28:10 samuel>
 *   Time-stamp: <2004/01/05 17:49:47 samuel>
 *   
 *	<os_interface.c>
 *	
 *	Interface to the OS beeing emulated (calls are made through 
 *	the 'sc' instruction).
 *   
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <time.h>
#include "mol_assert.h"
#include "mac_registers.h"
#include "molcpu.h"
#include "os_interface.h"
#include "debugger.h"
#include "memory.h"
#include "rvec.h"
#include "timer.h"
#include "res_manager.h"

#define DEBUG

static osi_proc		osi_selectors[NUM_OSI_SELECTORS];


/* 
 * This function is called whenever the a 'sc' instruction is issued
 * and r3 & r4 contains the magic constants. The routine selector
 * is passed in r5, parameters in r6..r31. The return value goes
 * in r3 (r4..r11 can also be used to return data).
 *
 * Pointers passed *MUST* by MAC-PHYSICAL.
 */

/************************************************************************/
/*	OSI Interface							*/
/************************************************************************/

static int
osip_bad_vector( int sel, int *params )
{
	printm("Uninitialized osip vector %d\n", sel);
	return 0;
}

static int
osip_call_available( int sel, int *params )
{
	sel = params[0];
	if( (uint)sel >= NUM_OSI_SELECTORS )
		return 0;
	if( osi_selectors[sel] == osip_bad_vector )
		return 0;
	return 1;
}

static int
osip_debugger( int sel, int *params )
{
	FREEZE_TIMERS;
	printm("*** osi user break [%d] ***\n", params[0] );
	RETUNE_TIMERS;
	
	stop_emulation();
	return 0;
}

static int
osip_log_putc( int sel, int *params )
{
	FREEZE_TIMERS;
	printm("%c", params[0] );
	RETUNE_TIMERS;
	return 0;
}

static int
osip_get_date( int sel, int *params )
{
	time_t gmt = time( NULL );
	int adj = 0;
	
	/* if something goes wrong, let the user adjust the time by hand */
	if( get_str_res("adjust_time") )
		adj = get_numeric_res("adjust_time") * 3600;

	if( sel == OSI_GET_LOCALTIME ) {
		struct tm *ltime = localtime( &gmt );
#ifndef __linux__
		struct tm *gtime = gmtime( &gmt );
		/* XXX: Bug: GMT-12 is treated as GMT+12 */
		return gmt + adj + 0x7C25B080 - ((ltime->tm_hour - gtime->tm_hour + 11 + 24)%24-11) * 3600;
#else
		/* 1904 -> 1970 conversion with time zone adjustment */
		return gmt + adj + 0x7C25B080 - timezone + (ltime->tm_isdst ? 3600 : 0);                
#endif
	}
	return gmt;
}

static int
osip_exit( int sel, int *params )
{
	quit_emulation();
	return 0;
}

static int
osip_usleep( int sel, int *params )
{
	struct timespec tv;
	tv.tv_sec = 0;
	tv.tv_nsec = params[0] * 1000;
	nanosleep(&tv, NULL);
	return 0;
}

static int 
rvec_osi_syscall( int dummy_rvec )
{
	int sel = mregs->gpr[5];
#ifdef DEBUG
	mregs->dbg_last_osi = sel;
#endif
	if( (uint)sel >= NUM_OSI_SELECTORS ) {
		printm("Bogus selector %d in os_interface_call!\n", sel);
		return 0;
	}
	shield_fpu( mregs );
	mregs->gpr[3] = osi_selectors[sel]( sel, (int*)&mregs->gpr[6] );
	return 0;
}


/************************************************************************/
/*	World Interface							*/
/************************************************************************/

int
os_interface_add_proc( int sel, osi_proc proc )
{
	assert( sel >= 0 && sel < NUM_OSI_SELECTORS );
	assert( !osi_selectors[sel] || osi_selectors[sel] == osip_bad_vector );

	osi_selectors[sel] = proc;
	return 0;
}

void
os_interface_remove_proc( int sel )
{
	assert( sel >= 0 && sel < NUM_OSI_SELECTORS );

	if( osi_selectors[sel] == osip_bad_vector )
		printm("osi_remove_proc: Bad selector (%d)\n", sel);
	osi_selectors[sel] = osip_bad_vector;
}

int
register_osi_iface( const osi_iface_t *iface, int size ) 
{
	int ret, i, n=size/sizeof(osi_iface_t);

	for( ret=0, i=0; i<n; i++ )
		ret |= os_interface_add_proc( iface[i].selector, iface[i].proc );
	return ret;
}

void
unregister_osi_iface( const osi_iface_t *iface, int size ) 
{
	int i, n=size/sizeof(osi_iface_t);

	for( i=0; i<n; i++ )
		os_interface_remove_proc( iface[i].selector );
}


/************************************************************************/
/*	Init / Cleanup							*/
/************************************************************************/

static const osi_iface_t osi_iface[] = {
	{ OSI_CALL_AVAILABLE,	osip_call_available	},
	{ OSI_DEBUGGER,		osip_debugger		},
	{ OSI_LOG_PUTC,		osip_log_putc		},
	{ OSI_GET_LOCALTIME,	osip_get_date		},
	{ OSI_GET_GMT_TIME,	osip_get_date		},
	{ OSI_EXIT,		osip_exit		},
	{ OSI_USLEEP,		osip_usleep		},
};

void
os_interface_init( void )
{
	int i;
	for( i=0; i<NUM_OSI_SELECTORS; i++ )
		os_interface_add_proc( i, osip_bad_vector );

	register_osi_iface( osi_iface, sizeof(osi_iface) );
	set_rvector( RVEC_OSI_SYSCALL, rvec_osi_syscall, "OSI SysCall" );
}

void
os_interface_cleanup( void )
{
}
