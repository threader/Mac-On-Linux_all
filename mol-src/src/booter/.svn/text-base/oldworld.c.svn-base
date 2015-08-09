/* 
 *   Creation Date: <2000/12/28 22:01:59 samuel>
 *   Time-stamp: <2004/02/22 17:43:33 samuel>
 *   
 *	<oldworld.c>
 *	
 *	Oldworld booter
 *   
 *   Copyright (C) 2000, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#define WANTED_ROM_BASE	((char*)0x4fc00000)	/* simplifies debugging */

#define ROM_FIX

#include "mol_config.h"
#include <sys/mman.h>

#include "memory.h"
#include "res_manager.h"
#include "wrapper.h"
#include "debugger.h"
#include "promif.h"
#include "verbose.h"
#include "booter.h"

static int			romim_fd = -1;
static struct mmu_mapping	rmap;
static int			rom_writeable;

static void 
map_rom( char *rom_image ) 
{
	mol_kmod_info_t *info = get_mol_kmod_info();
	mol_device_node_t *dn;
	int len, *prop;

	/* "rom" is for oldworld, "boot-rom" for new-world */
	if( !(dn=prom_find_type("rom")) && !(dn=prom_find_devices("boot-rom")) )
		fatal("OF ROM entry is missing\n");
	
	if( !(prop=(int*)prom_get_property(dn, "reg", &len)) || len != sizeof(int[2]) )
		fatal("Bad ROM property\n");
	rom.mbase = prop[0];
	rom.size = prop[1];
	rom.flags = MAPPING_RO;

	if( rom_image ) {
		if( (romim_fd=open(rom_image, O_RDONLY)) == -1 )
			fatal("Could not open ROM-image '%s'\n", rom_image);

		rom_writeable = 1;
		rom.lvbase = mmap( WANTED_ROM_BASE, rom.size,
				   PROT_READ|PROT_WRITE|PROT_EXEC, MAP_PRIVATE,
				   romim_fd, 0 );
		if( rom.lvbase == MAP_FAILED ) {
			rom.lvbase = 0;
			perrorm("mmap");
			exit(1);
		}
		printm("%dK ROM mapped at %p from '%s'\n", rom.size/1024, rom.lvbase, rom_image );
		return;
	}

	/* check if a physical ROM image is available */
	if( info->rombase != rom.mbase || info->romsize != rom.size )
		fatal("Could not find a suitable physical ROM\n");

#ifdef ROM_FIX
	/* there is a bug (feature) in the kernel which prevents the mapping
	 * of the very last ROM page (0xfffff000)
	 */
	if( rom.size + rom.mbase == 0 )
		rom.size -= 0x1000;
#endif
	/* We will probably get a cache-inhibited ROM access. 
	 * It doesn't really matter since it is only used by the debugger.
	 */
	if( !(rom.lvbase=map_phys_mem(WANTED_ROM_BASE, rom.mbase, rom.size, PROT_READ)) )
		exit(1);

#ifdef ROM_FIX
	if( rom.size & 0x1000 ) {
		char *last_page;
		
		if( !(last_page=map_zero(rom.lvbase+rom.size, 0x1000)) )
			fatal("ROM_workaround failed\n");

		if( last_page != rom.lvbase + rom.size )
			fatal("ROM workaround failed!\n");

		rom.size += 0x1000;
		
		/* Unfortunately it doesn't work with read() either -
		 * we use this nasty hack to fill in the data.
		 */
		_copy_last_rompage( last_page );

		/* write protect page */
		if( mprotect(last_page, 0x1000, PROT_READ) )
			perrorm("init_rom: mprotect:");
		printm("<ROM_FIX> ");
	}
#endif
	printm("%dK ROM mapped at %p\n", rom.size/1024, rom.lvbase );
}

int
rom_is_writeable( void )
{
	return rom_writeable;
}

static void
oldworld_cleanup( void )
{
	if( rom.lvbase ) {
		_remove_mmu_mapping( &rmap );
		munmap( rom.lvbase, rom.size );
		rom.lvbase = 0;
	}

	if( romim_fd != -1 )
		close( romim_fd );
}

void
oldworld_startup( void )
{
	char *rom_filename = get_filename_res("rom_image");

	map_rom( rom_filename );
	rmap = rom;

	if( !rom_filename ) {
		printm("<Physical ROM mapping>\n");
		rmap.flags |= MAPPING_PHYSICAL;
		rmap.lvbase = (char*)rmap.mbase;
	}
	_add_mmu_mapping( &rmap );

	flush_icache_range( rom.lvbase, rom.lvbase + rom.size );

	gPE.booter_cleanup = oldworld_cleanup;
}
