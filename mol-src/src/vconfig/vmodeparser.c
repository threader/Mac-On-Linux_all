/* 
 *   Creation Date: <2000/01/09 21:27:02 samuel>
 *   Time-stamp: <2003/07/03 17:13:38 samuel>
 *   
 *	<mode.c>
 *	
 *	Mode parser interface
 *   
 *	Based on fbset, (C) 1995-1998 by Geert Uytterhoeven	
 *
 *   Copyright (C) 2000, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include <stdarg.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/ioctl.h>

#include <linux/fb.h>
#include "res_manager.h"
#include "debugger.h"
#include "fbset.h"
#include "vmodeparser.h"

const char *vmode_db_name = NULL;
static struct VideoMode *VideoModes = NULL;

static void ConvertFromVideoMode(const struct VideoMode *vmode, struct fb_var_screeninfo *var);


int 
vmodeparser_init( char *database )
{
	if( !database )
		return 1;

	vmode_db_name = database;
	if (!(yyin = fopen(vmode_db_name, "r"))) {
		printf("Opening the video-mode database '%s': %s\n", vmode_db_name, strerror(errno));
		return 1;
	}
	yyparse();
	fclose(yyin);

	return 0;
}

int
vmodeparser_get_mode( struct fb_var_screeninfo *var, int ind, 
		      ulong *refresh, char **retname )
{
	VideoMode_t *vm;
	int i;

	for(i=0, vm=VideoModes; i<ind && vm; i++ )
		vm=vm->next;
	if( !vm )
		return -1;
	ConvertFromVideoMode( vm, var );

	if( refresh )
		*refresh = (ulong)(vm->vrate *65536);
	if( retname )
		*retname = vm->name;
	return 0;
}


void
vmodeparser_cleanup( void )
{
	VideoMode_t *vm, *next;

	for( vm=VideoModes; vm; vm=next ){
		/* name is allocated with malloc in modes.l */
		if( vm->name )
			free( vm->name );
		next = vm->next;
		free( vm );
	}
	VideoModes = NULL;
}

/* Calculate the Scan Rates for a Video Mode */
static int 
FillScanRates(struct VideoMode *vmode)
{
	u_int htotal = vmode->left+vmode->xres+vmode->right+vmode->hslen;
	u_int vtotal = vmode->upper+vmode->yres+vmode->lower+vmode->vslen;

	if (vmode->dblscan)
		vtotal <<= 2;
	else if (!vmode->laced)
		vtotal <<= 1;

	if (!htotal || !vtotal)
	return 0;

	if (vmode->pixclock) {
		vmode->drate = 1E12/vmode->pixclock;
		vmode->hrate = vmode->drate/htotal;
		vmode->vrate = vmode->hrate/vtotal*2;
	} else {
		vmode->drate = 0;
		vmode->hrate = 0;
		vmode->vrate = 0;
	}
	return 1;
}


/* Callback from the parser */
void 
AddVideoMode(const struct VideoMode *vmode)
{
	struct VideoMode *vmode2;

	vmode2 = malloc(sizeof(struct VideoMode));
	*vmode2 = *vmode;
	if (!FillScanRates(vmode2))
		Die("%s:%d: Bad video mode `%s'\n", vmode_db_name, line, vmode2->name);
	vmode2->next = VideoModes;
	VideoModes = vmode2;

}

/* Callback from the parser too */
void
Die(const char *fmt, ...)
{
	va_list ap;

	fflush(stdout);
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
	exit(1);
}


/* Conversion routine */
static void 
ConvertFromVideoMode(const struct VideoMode *vmode,
			 struct fb_var_screeninfo *var)
{
	memset(var, 0, sizeof(struct fb_var_screeninfo));
	var->xres = vmode->xres;
	var->yres = vmode->yres;
	var->xres_virtual = vmode->vxres;
	var->yres_virtual = vmode->vyres;
	var->bits_per_pixel = vmode->depth;
#if 0
	if (Opt_test)
		var->activate = FB_ACTIVATE_TEST;
	else
	var->activate = FB_ACTIVATE_NOW;
#endif
	var->accel_flags = vmode->accel_flags;
	var->pixclock = vmode->pixclock;
	var->left_margin = vmode->left;
	var->right_margin = vmode->right;
	var->upper_margin = vmode->upper;
	var->lower_margin = vmode->lower;
	var->hsync_len = vmode->hslen;
	var->vsync_len = vmode->vslen;
	if (vmode->hsync == HIGH)
		var->sync |= FB_SYNC_HOR_HIGH_ACT;
	if (vmode->vsync == HIGH)
		var->sync |= FB_SYNC_VERT_HIGH_ACT;
	if (vmode->csync == HIGH)
		var->sync |= FB_SYNC_COMP_HIGH_ACT;
	if (vmode->gsync == HIGH)
		var->sync |= FB_SYNC_ON_GREEN;
	if (vmode->extsync == TRUE)
		var->sync |= FB_SYNC_EXT;
	if (vmode->bcast == TRUE)
		var->sync |= FB_SYNC_BROADCAST;
	if (vmode->laced == TRUE)
		var->vmode = FB_VMODE_INTERLACED;
	else if (vmode->dblscan == TRUE)
		var->vmode = FB_VMODE_DOUBLE;
	else
		var->vmode = FB_VMODE_NONINTERLACED;
/*	var->vmode |= FB_VMODE_CONUPDATE; */
}
