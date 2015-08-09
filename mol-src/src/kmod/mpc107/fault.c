/* 
 *   Creation Date: <2003/05/27 17:41:28 samuel>
 *   Time-stamp: <2003/05/27 17:44:20 samuel>
 *   
 *	<fault.c<2>>
 *	
 *	
 *   
 *   Copyright (C) 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#include "archinclude.h"

ulong 
get_phys_page( ulong va, int request_rw )
{
	return 0;
}

extern void
__store_PTE( int ea, unsigned long *slot, int pte0, int pte1 )
{
	
}
