/* 
 *   Creation Date: <2001/06/16 18:43:50 samuel>
 *   Time-stamp: <2004/02/28 15:12:56 samuel>
 *   
 *	<elf.c>
 *	
 *	ELF loader
 *   
 *   Copyright (C) 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */

#include "mol_config.h"
#include <sys/param.h>
#include "memory.h"
#include "elfload.h"
#include "mac_registers.h"
#include "loader.h"


int
load_elf( const char *image_name )
{
	Elf32_Ehdr ehdr;
	Elf32_Phdr *phdr;
	ulong mem_top;
	int fd, i;

	if( !(fd=open(image_name, O_RDONLY)) )
		fatal("Could not open '%s'\n", image_name );
	if( !is_elf32(fd, 0) ) {
		close(fd);
		return 1;
	}
	if( !(phdr=elf32_readhdrs(fd, 0, &ehdr)) )
		fatal("elf32_readhdrs failed\n");

	mem_top = 0;
	for( i=0; i<ehdr.e_phnum; i++ ) {
		size_t s = MIN( phdr[i].p_filesz, phdr[i].p_memsz );
		char *addr;
		/* printm("filesz: %08X memsz: %08X p_offset: %08X p_vaddr %08X\n", 
		   phdr[i].p_filesz, phdr[i].p_memsz, phdr[i].p_offset, phdr[i].p_vaddr ); */

		if( mem_top < phdr[i].p_memsz + phdr[i].p_paddr )
			mem_top = phdr[i].p_memsz + phdr[i].p_paddr;

		if( !(addr=transl_mphys(phdr[i].p_paddr)) ) {
			printm("ELF: program segment not in RAM\n");
			continue;
		}
		lseek( fd, phdr[i].p_offset, SEEK_SET );
		if( read(fd, addr, s) != s )
			fatal("read failed\n");
		flush_icache_range( addr, addr+s );
	}
	mregs->nip = ehdr.e_entry;
	free( phdr );
	close( fd );

	return 0;
}
