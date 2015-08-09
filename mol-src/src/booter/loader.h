/* 
 *   Creation Date: <2004/02/28 15:07:33 samuel>
 *   Time-stamp: <2004/02/28 15:08:16 samuel>
 *   
 *	<loader.h>
 *	
 *	Loader API
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */

#ifndef _H_LOADER
#define _H_LOADER

extern int	load_macho( const char *image_name );
extern int	load_elf( const char *image_name );

#endif   /* _H_LOADER */
