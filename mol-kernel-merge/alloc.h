/* 
 *   Copyright (C) 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   Memory allocation and mappings
 *   
 *	CHECK THIS FOR REMOVAL
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_ALLOC
#define _H_ALLOC

#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <asm/uaccess.h>
#include <asm/cacheflush.h>
#include <asm/io.h>

static inline void *map_phys_range(ulong paddr, ulong size, char **ret_addr)
{
	/* Warning: This works only for certain addresses... */
	*ret_addr = phys_to_virt(paddr);
	return (void *)(-2);	/* dummy */
}
static inline void unmap_phys_range(void *handle) {}

#endif				/* _H_ALLOC */
