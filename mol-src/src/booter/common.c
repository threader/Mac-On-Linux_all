/* 
 *   Creation Date: <2001/06/16 18:43:50 samuel>
 *   Time-stamp: <2004/03/13 16:58:19 samuel>
 *   
 *	<elf.c>
 *	
 *	common startup code
 *   
 *   Copyright (C) 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/param.h>
#include "booter.h"
#include "elfload.h"
#include "memory.h"
#include "mac_registers.h"
#include "res_manager.h"
#include "processor.h"
#include "../drivers/include/pci.h"
#include "mol_assert.h"
#include "os_interface.h"
#include "wrapper.h"
#include "mmu_mappings.h"
#include "pseudofs.h"
#include "loader.h"

static mmu_mapping_t	mregs_map;


/************************************************************************/
/*	common code							*/
/************************************************************************/

static void
common_cleanup( void )
{
	if( mregs_map.id )
		_remove_mmu_mapping( &mregs_map );
}

static void
common_startup( char *image_name )
{
	if( load_elf(image_name) )
		if( load_macho(image_name) )
			fatal("%s is not a ELF nor a Mach-O image\n", image_name );			

	gPE.booter_cleanup = common_cleanup;
}


/* mapin mregs at physical address specified by client */ 
static int
osip_mapin_mregs( int sel, int *params )
{
	ulong mphys = params[0];

	if( mregs_map.id )
		_remove_mmu_mapping( &mregs_map );

	if( mphys ) {
		mregs_map.mbase = mphys;
		mregs_map.lvbase = 0;	/* offset into mregs */
		mregs_map.size = NUM_MREGS_PAGES * 0x1000;
		mregs_map.flags = MAPPING_MREGS | MAPPING_RO;
		if( mphys < ram.size )
			mregs_map.flags |= MAPPING_PUT_FIRST;
		_add_mmu_mapping( &mregs_map );
		
		/* printm("mregs mapin @%08lx -> %0lx\n", mphys, (ulong)mregs_map.lvbase ); */
	}
	return 0;
}

/* EMUACCEL_xxx, nip -- branch_target */
static int
osip_emuaccel( int sel, int *params )
{
	int ret, emuaccel_inst = params[0];
	int par = params[1];
	int addr = params[2];
	
	if( !emuaccel_inst )
		return _mapin_emuaccel_page( addr );

	ret = _alloc_emuaccel_slot( emuaccel_inst, par, addr );
	/* printm("emuaccel [%d]: %08x (%d)\n", ret, addr, emuaccel_inst ); */

	return ret;
}


/************************************************************************/
/*	generic ELF booting						*/
/************************************************************************/

void
elf_startup( void )
{
	char *name;

	if( !(name=get_filename_res("elf_image")) )
		missing_config_exit("elf_image");

	common_startup( name );

	/* this is mostly for debugging */
	if( get_bool_res("assign_pci") )
		pci_assign_addresses();

	mregs->msr = 0;
	mregs->spr[ S_SDR1 ] = ram.size - 65536;
	mregs->gpr[1] = ram.size - 65536 - 32;

	/* os_interface_add_proc( OSI_MAPIN_MREGS, osip_mapin_mregs ); */
}


/************************************************************************/
/*	MacOS X boot loader						*/
/************************************************************************/

void
osx_startup( void )
{
	char *name;
	
	if( !(name=get_filename_res("bootx_image")) )
		missing_config_exit("bootx_image");
	
	common_startup( name );

	pci_assign_addresses();

	os_interface_add_proc( OSI_MAPIN_MREGS, osip_mapin_mregs );
	os_interface_add_proc( OSI_EMUACCEL, osip_emuaccel );
}


/************************************************************************/
/*	OpenFirmware booting (newworld MacOS, linux)			*/
/************************************************************************/

void
of_startup( void )
{
	char *name;

	/* the OF loader requires 32 MB of RAM */
	if( ram.size < 32 * 0x100000 )
		fatal("Specify at least 32 MB of RAM!\n");

	if( !(name=get_filename_res("of_image")) )
		missing_config_exit("of_image");

	common_startup( name );
	pci_assign_addresses();

	/* MacOS uselessly loads/clears the BAT register in the idle
	 * loop. This has a performance impact which we work around by
	 * avoiding flushing the BAT mapping.
	 */
	if( is_newworld_boot() )
		mregs->use_bat_hack = 1;
}
