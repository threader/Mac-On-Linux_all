/* 
 *   Creation Date: <2001/10/20 18:07:01 samuel>
 *   Time-stamp: <2001/10/28 21:31:11 samuel>
 *   
 *	<mregs.h>
 *	
 *	Architecture dependent part of mregs (for i386)
 *   
 *   Copyright (C) 2001 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_MREGS
#define _H_MREGS

typedef struct eppc_mregs {
	char	*ram_lvptr;	/* for easy assembly access */
} arch_mregs_t;

#endif   /* _H_MREGS */
