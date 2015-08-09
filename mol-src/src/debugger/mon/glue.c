/* 
 *   Creation Date: <2003/09/03 20:55:28 samuel>
 *   Time-stamp: <2003/09/03 20:57:32 samuel>
 *   
 *	<glue.c>
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

#include "glue.h"
#include "bfd.h"

bfd_vma
bfd_getb32( const unsigned char *p ) 
{
	return ld_be32( (unsigned long*)p );
}

bfd_vma
bfd_getl32( const unsigned char *p ) 
{
	return ld_le32( (unsigned long*)p );
}

