/* 
 *   Creation Date: <2004/02/24 00:47:00 samuel>
 *   Time-stamp: <2004/02/28 15:08:18 samuel>
 *   
 *	<macho.c>
 *	
 *	Mach-O loader
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */

#include "mol_config.h"
#include "macho.h"
#include "memory.h"
#include "mac_registers.h"
#include "loader.h"

#ifdef CONFIG_KVM
#include "../cpu/kvm/kvm.h"
#endif

static int
is_macho( int fd )
{
	mach_header_t hd;

	if( pread(fd, &hd, sizeof(hd), 0) != sizeof(hd) )
		return 0;
	
	if( hd.magic != MH_MAGIC )
		return 0;
	if( hd.filetype != MH_EXECUTE || hd.cputype != CPU_TYPE_POWERPC )
		goto err;
	if( !(hd.flags & MH_NOUNDEFS) )
		goto err;
	return 1;
 err:
	printm("unsupported mach-o format\n");
	return 0;
}

static void
read_segment( int fd )
{
	segment_command_t seg;
	char *dest;
	int i;
	
	if( read(fd, &seg, sizeof(seg)) != sizeof(seg) )
		fatal("reading macho segment\n");

 	/* printm("segname: %s\n", seg.segname );
	printm("vmaddr: %08lx\n", seg.vmaddr );
	printm("vmsize: %08lx\n", seg.vmsize );
	printm("fileoff: %08lx\n", seg.fileoff );
	printm("filesize: %08lx\n", seg.filesize );
	printm("nsects: %ld\n", seg.nsects );
	printm("flags: %lx\n", seg.flags ); */

	/* read sections */
	for( i=0; i<seg.nsects; i++ ) {
		section_t sec;

		if( read(fd, &sec, sizeof(sec)) != sizeof(sec) )
			fatal("failed to read section");

		/* printm("SECTION [%s/%s]: virt %08lx size %08lx offset %08lx\n",
		   sec.sectname, sec.segname, sec.addr, sec.size, sec.offset ); */
		
		if( !(dest=transl_mphys(sec.addr)) ) {
			printm("Mach-O: section not in RAM\n");
			return;
		}
		if( sec.flags & S_ZEROFILL )
			continue;
		if( pread(fd, dest, sec.size, sec.offset) != sec.size )
			fatal("Failed to read segment data\n");

		flush_icache_range( dest, dest+sec.size );
	}
}

static void
read_thread( int fd )
{
	thread_command_t th;
	int i;

	if( read(fd, &th, sizeof(th)) != sizeof(th) )
		fatal("reading macho thread\n");

#ifdef CONFIG_KVM
	kvm_regs_kvm2mol();
#endif

	for( i=0; i<32; i++ )
		mregs->gpr[i] = th.gprs[i];
	mregs->nip = th.srr0;
	mregs->link = th.lr;
	mregs->cr = th.cr;
	mregs->xer = th.xer;
	mregs->ctr = th.ctr;
	/* mregs->vrsave = th.vrsave; */
	mregs->msr = 0;

#ifdef CONFIG_KVM
	kvm_regs_mol2kvm();
#endif
}

int
load_macho( const char *image_name )
{
	load_command_t cmd;
	mach_header_t hd;
	int i, pos, fd;
	
	if( (fd=open(image_name, O_RDONLY)) < 0 )
		fatal_err("opening %s", image_name );

	if( !is_macho(fd) ) {
		close( fd );
		return 1;
	}

	if( read(fd, &hd, sizeof(hd)) != sizeof(hd) )
		fatal("failed to read Mach-O header");	

	pos = sizeof(hd);
	for( i=0; i<hd.ncmds; i++, pos+=cmd.cmdsize ) {
		if( pread(fd, &cmd, sizeof(cmd), pos) != sizeof(cmd) )
			break;
		lseek( fd, pos, SEEK_SET );

		if( cmd.cmd == LC_SYMTAB )
			continue;

		/* printm("\nLoad-CMD [%d]: %ld\n", i, cmd.cmd ); */
		switch( cmd.cmd ) {
		case LC_SEGMENT:
			read_segment( fd );
			break;

		case LC_THREAD:
		case LC_UNIXTHREAD:
			read_thread( fd );
			break;
		}
	}
	close( fd );
	return 0;
}
