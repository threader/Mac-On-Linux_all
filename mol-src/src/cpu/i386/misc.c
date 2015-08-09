/* 
 *   Creation Date: <2004/06/12 18:47:10 samuel>
 *   Time-stamp: <2004/06/12 23:09:42 samuel>
 *   
 *	<misc.c>
 *	
 *	
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */

#include "mol_config.h"
#include "wrapper.h"
#include "timer.h"
#include "memory.h"
#include "molcpu.h"
#include "mac_registers.h"

int
mol_ioctl( int cmd, int p1, int p2, int p3 )
{
	printf("mol ioctl %d\n", cmd );
	return 0;
}

int
mol_ioctl_simple( int cmd )
{
	printf("mol ioctl %d\n", cmd );
	return 0;
}


void
flush_icache_range( char *start, char *stop )
{
}

ulong
get_tbl( void )
{
	return 0;
}

ullong
get_mticks_( void )
{
	return 0;
}

void
get_timestamp( timestamp_t *s )
{
	s->hi = s->lo = 0;
}

void
molcpu_arch_init( void )
{
}

void
molcpu_arch_cleanup( void )
{
}

void
wrapper_init( void )
{
}

void
wrapper_cleanup( void )
{
}

int
open_session( void )
{
	return 0;
}

void
close_session( void )
{
}

void
molcpu_mainloop_prep( void )
{
}

ulong
get_cpu_frequency( void )
{
	return 1400 * 1000000;
}

mol_kmod_info_t *
get_mol_kmod_info( void ) 
{
        static mol_kmod_info_t info;
        static int once=0;
        
        if( !once ) {
                memset( &info, 0, sizeof(info) );
                //_get_info( &info );
        }
        return &info;
}

void
molcpu_mainloop( void )
{
}

void
save_fpu_completely( struct mac_regs *mregs )
{
}

void
reload_tophalf_fpu( struct mac_regs *mregs )
{
}

void
shield_fpu( struct mac_regs *mregs )
{
}

