/* 
 *   Creation Date: <2002/11/24 01:02:00 samuel>
 *   Time-stamp: <2003/02/23 15:38:58 samuel>
 *   
 *	<loader.c>
 *	
 *	simple sound component loader
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include <Types.h>
#include <Memory.h>
#include <OSUtils.h>

#include "logger.h"
#include "LinuxOSI.h"
#include "sound_sh.h"

void 
main( void )
{
	if( !MOLIsRunning() )
		return;

	if( OSI_SoundCntl1(kSoundCheckAPI, OSI_SOUND_API_VERSION) < 0 )
		return;
	OSI_SoundCntl1( kSoundClassicDriverVersion, CLASSIC_SOUND_DRIVER_VERSION );

	RegisterComponentResourceFile( CurResFile(), registerComponentGlobal );
}
