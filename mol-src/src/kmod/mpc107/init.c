/* 
 *   Creation Date: <2003/05/27 17:28:35 samuel>
 *   Time-stamp: <2003/05/28 15:34:50 samuel>
 *   
 *	<init.c>
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
#include "kernel_vars.h"

int reloc_virt_offs = 0;

void
dec_mod_count( void )
{
}

void
inc_mod_count( void )
{
}

kernel_vars_t *
alloc_kvar_pages( void )
{
	return NULL;
}

void
free_kvar_pages( kernel_vars_t *kv )
{
}

int
relocate_code( void )
{
	return 0;
}

void
relocation_cleanup( void )
{
}

void
remove_hooks( void )
{
}

int
write_hooks( void )
{
	return 0;
}

int
arch_mmu_init( void )
{
	return 0;
}

int
start( void )
{
	char *p = 0;
	int i;
	for( i=0; i<16; i++ )
		p[i] = i;
}
