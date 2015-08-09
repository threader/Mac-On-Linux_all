/* 
 *   Creation Date: <2001/05/05 16:44:17 samuel>
 *   Time-stamp: <2002/10/24 23:45:48 samuel>
 *   
 *	<elfload.h>
 *	
 *	Elf loader
 *   
 *   Copyright (C) 2001, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_ELFLOAD
#define _H_ELFLOAD

#include "elf.h"

extern int		is_elf32( int fd, int offs );
extern Elf32_Phdr 	*elf32_readhdrs( int fd, int offs, Elf32_Ehdr *e );

#endif   /* _H_ELFLOAD */
