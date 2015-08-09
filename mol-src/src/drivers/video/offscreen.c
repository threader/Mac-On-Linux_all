/* 
 *   Creation Date: <2000-01-01 04:32:32 samuel>
 *   Time-stamp: <2003/02/19 23:01:14 samuel>
 *   
 *	<offscreen.c>
 *	
 *	Simple offscreen video driver (fallback)
 *   
 *   Copyright (C) 2000, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "verbose.h"
#include "debugger.h"
#include "memory.h"
#include "mouse_sh.h"
#include "video_module.h"
#include "x11.h"

static struct {
	int 		is_open;
	video_desc_t 	vmode;
} ov;


static int
vopen( video_desc_t *vm )
{
	if( ov.is_open )
		return 1;
	
	vm->lvbase = map_zero( NULL, FBBUF_SIZE(vm) );

	vm->mmu_flags = MAPPING_FORCE_CACHE;
	vm->map_base = 0;

	if( !vm->lvbase )
		vm->mmu_flags |= MAPPING_SCRATCH;

	ov.vmode = *vm;
	ov.is_open = 1;

	display_x_dialog();
	return 0;
}

static void
vclose( void )
{
	if( !ov.is_open )
		return;
	ov.is_open = 0;

	if( ov.vmode.lvbase )
		munmap( ov.vmode.lvbase, FBBUF_SIZE( &ov.vmode ));

	remove_x_dialog();
}

static void 
setcmap( char *pal )
{
}

static int
init( video_module_t *m )
{
	memset( &ov, 0, sizeof(ov) );
	return 0;
}

static void
cleanup( video_module_t *m ) 
{
	if( ov.is_open )
		vclose();
}

video_module_t offscreen_module  = {
	.name		= "offscreen_video",
	.vinit		= init,
	.vcleanup	= cleanup,
	.vopen		= vopen,
	.vclose		= vclose,
	.setcmap	= setcmap,
};
