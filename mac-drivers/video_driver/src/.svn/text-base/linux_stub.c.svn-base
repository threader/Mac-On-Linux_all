/* 
 *   Creation Date: <1999/03/20 07:32:53 samuel>
 *   Time-stamp: <2002/12/07 21:26:05 samuel>
 *   
 *	<osi_interface.c>
 *	
 *	Linux callback stub
 *   
 *   Copyright (C) 1999, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "linux_stub.h"
#include <Video.h>
#include <LinuxOSI.h>

int
OSI_SetVMode( int modeID, int depthMode )
{
	return OSI_SetVMode_( modeID, depthMode - kDepthMode1 );
}

int
OSI_GetVModeInfo( int modeID, int depthMode, osi_get_vmode_info_t *ret )
{
	int err;

	if( !ret )
		return -1;
	err = OSI_GetVModeInfo_( modeID, depthMode-kDepthMode1, ret );
	if( !err ) {
		ret->cur_depth_mode += kDepthMode1;
		/* make sure depth is MacOS compatile */
		switch( ret->depth ) {
		case 15:
			ret->depth = 16;
			break;			
		case 24:
			ret->depth = 32;
			break;
		}
	}
	return err;
}
