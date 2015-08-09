/* Mac-on-Linux ELF loader 

   Copyright (C) 2001 Samuel Rydh

   adapted from yaboot

   Copyright (C) 1999 Benjamin Herrenschmidt

   portions based on poof

   Copyright (C) 1999 Marius Vollmer

   portions based on quik
   
   Copyright (C) 1996 Paul Mackerras.

   Because this program is derived from the corresponding file in the
   silo-0.64 distribution, it is also

   Copyright (C) 1996 Pete A. Zaitcev
   		 1996 Maurizio Plaza
   		 1996 David S. Miller
   		 1996 Miguel de Icaza
   		 1996 Jakub Jelinek

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
*/

#include "mol_config.h"
#include "elfload.h"
#include "byteorder.h"

#define DEBUG		0
#define MAX_HEADERS	32


/************************************************************************/
/*	F U N C T I O N S						*/
/************************************************************************/

static void
fix_ehdr_byteorder( Elf32_Ehdr *e )
{
	BE16_FIELD( e->e_type );
	BE16_FIELD( e->e_machine );
	BE32_FIELD( e->e_version );
	BE32_FIELD( e->e_entry );
	BE32_FIELD( e->e_phoff );
	BE32_FIELD( e->e_shoff );
	BE32_FIELD( e->e_flags );
	BE16_FIELD( e->e_ehsize );
	BE16_FIELD( e->e_phentsize );
	BE16_FIELD( e->e_phnum );
	BE16_FIELD( e->e_shentsize );
	BE16_FIELD( e->e_shnum );
	BE16_FIELD( e->e_shstrndx );
}

static void
fix_phdr_byteorder( Elf32_Phdr *e )
{
	BE32_FIELD( e->p_type );
	BE32_FIELD( e->p_offset );
	BE32_FIELD( e->p_vaddr );
	BE32_FIELD( e->p_paddr );
	BE32_FIELD( e->p_filesz );
	BE32_FIELD( e->p_memsz );
	BE32_FIELD( e->p_flags );
	BE32_FIELD( e->p_align );
}


int
is_elf32( int fd, int offs )
{
	Elf32_Ehdr e;
	int size = sizeof(e);

	lseek( fd, offs, SEEK_SET );
	if( read(fd, &e, size) < size ) {
		printm("\nCan't read Elf32 image header\n");
		return 0;
	}
	fix_ehdr_byteorder(&e);
	
	return (e.e_ident[EI_MAG0]  == ELFMAG0	    &&
		e.e_ident[EI_MAG1]  == ELFMAG1	    &&
		e.e_ident[EI_MAG2]  == ELFMAG2	    &&
		e.e_ident[EI_MAG3]  == ELFMAG3	    &&
		e.e_ident[EI_CLASS] == ELFCLASS32  &&
		e.e_ident[EI_DATA]  == ELFDATA2MSB &&
		e.e_type            == ET_EXEC	    &&
		e.e_machine         == EM_PPC);
}

Elf32_Phdr *
elf32_readhdrs( int fd, int offs, Elf32_Ehdr *e )
{
	Elf32_Phdr *ph;
	int i, size = sizeof(Elf32_Ehdr);
	
	if( !is_elf32(fd, offs) ) {
		printm("elf32_getphds: not an ELF image\n");
		return NULL;
	}
	
	lseek( fd, offs, SEEK_SET );
	if( read(fd, e, size) < size ) {
		printm("\nCan't read Elf32 image header\n");
		return NULL;
	}
	fix_ehdr_byteorder( e );
	
#if DEBUG
	printm("Elf32 header:\n");
	printm(" e.e_type    = %d\n", (int)e->e_type);
	printm(" e.e_machine = %d\n", (int)e->e_machine);
	printm(" e.e_version = %d\n", (int)e->e_version);
	printm(" e.e_entry   = 0x%08x\n", (int)e->e_entry);
	printm(" e.e_phoff   = 0x%08x\n", (int)e->e_phoff);
	printm(" e.e_shoff   = 0x%08x\n", (int)e->e_shoff);
	printm(" e.e_flags   = %d\n", (int)e->e_flags);
	printm(" e.e_ehsize  = 0x%08x\n", (int)e->e_ehsize);
	printm(" e.e_phentsize = 0x%08x\n", (int)e->e_phentsize);
	printm(" e.e_phnum   = %d\n", (int)e->e_phnum);
#endif
	if (e->e_phnum > MAX_HEADERS) {
		printm("elfload: too many program headers (MAX_HEADERS)\n");
		return NULL;
	}

	size = sizeof(Elf32_Phdr) * e->e_phnum;
	if( !(ph=(Elf32_Phdr*)malloc(size)) )
		return NULL;

	/* now, we read the section header */
	lseek( fd, offs+e->e_phoff, SEEK_SET );
	if( read(fd, ph, size) != size ) {
		perrorm("read");
		free( ph );
		return NULL;
	}
	for( i=0; i<e->e_phnum; i++ )
		fix_phdr_byteorder( &ph[i] );
	
	return ph;
}
