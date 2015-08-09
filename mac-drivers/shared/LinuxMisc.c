/* 
 *	<misc.c>
 *	
 *	 Mac-on-linux util routines
 *   
 *   Copyright (C) 1999, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */
 
#include "LinuxOSI.h"

int
MOLIsRunning( void )
{
	static int cached = -2;
	long v;

	if( cached < 0 ) {
		cached = 0;
		if( Gestalt( MOL_GESTALT_SELECTOR, &v ) == noErr )
			cached = (v==MOL_GESTALT_VALUE);
	}
	return cached;
}
