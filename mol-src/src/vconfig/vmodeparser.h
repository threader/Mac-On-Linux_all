/* 
 *   Creation Date: <2000/01/09 22:38:39 samuel>
 *   Time-stamp: <2000/10/18 20:44:29 samuel>
 *   
 *	<vmodeparser.h>
 *	
 *	Interface to the vmode parser
 *   
 *   Copyright (C) 2000 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_VMODEPARSER
#define _H_VMODEPARSER

#include <linux/fb.h>
#include <stdio.h>
#include <sys/types.h>

extern int vmodeparser_init( char *database );
extern void vmodeparser_cleanup( void );
extern int vmodeparser_get_mode( struct fb_var_screeninfo *var, int ind, 
				 ulong *ret_refresh, char **ret_name );

#endif   /* _H_VMODEPARSER */
