/* 
 *   Creation Date: <1999/06/14 22:51:59 samuel>
 *   Time-stamp: <2004/02/07 18:13:40 samuel>
 *   
 *	<hacks.c>
 *	
 *	Various hacks
 *   
 *   Copyright (C) 1999, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "hacks.h"
#include "debugger.h"
#include "promif.h"
#include "driver_mgr.h"

static int hacks = 0;

int 
get_hacks( void )
{
	return hacks;
}

static int
hacks_init( void )
{
	hacks = 0;
	if( prom_find_devices( "starmax_hack" ) ){
		printm("Starmax hack activated\n");
		hacks |= hack_starmax;
	}
	return 1;
}

driver_interface_t hacks_driver = {
    "hacks", hacks_init, NULL
};
