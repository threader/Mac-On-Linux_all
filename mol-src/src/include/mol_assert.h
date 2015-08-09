/* 
 *   Creation Date: <2001/01/27 15:12:25 samuel>
 *   Time-stamp: <2003/03/01 16:03:46 samuel>
 *   
 *	<mol_assert.h>
 *	
 *	Assert macros
 *   
 *   Copyright (C) 2000, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_MOL_ASSERT
#define _H_MOL_ASSERT

#ifdef ENABLE_ASSERT
#define assert( cond ) \
	({ if( !(cond) ) { printm("Assertion failed in %s() (%s, line %d)\n", __FUNCTION__, __FILE__, __LINE__); exit(1); }})
#else
#define assert( cond )
#endif

#endif   /* _H_MOL_ASSERT */
