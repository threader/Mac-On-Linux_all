/* 
 *   Creation Date: <2002/07/23 21:42:55 samuel>
 *   Time-stamp: <2002/07/23 21:51:38 samuel>
 *   
 *	<MolLib.cpp>
 *	
 *	
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

extern "C" {
#include <sys/systm.h>
}
#include <IOKit/IOLib.h>
#include "osi_calls.h"

int
printm( const char *fmt, ... )
{
	char *p, buf[256];
	va_list args;
	int i;
	
	va_start( args, fmt );
	i = vsnprintf( buf, sizeof(buf), fmt, args );
	va_end( args );
	
	for( p="<*> "; *p; p++ )
		OSI_PutC( *p );
	for( p=buf; *p; p++ )
		OSI_PutC( *p );
	return i;
}
