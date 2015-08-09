/* 
 *   Creation Date: <2003/05/22 22:37:56 samuel>
 *   Time-stamp: <2003/05/28 17:39:33 samuel>
 *   
 *	<init.c>
 *	
 *	support for MPC107 based CPU-card
 *   
 *   Copyright (C) 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/mman.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include "molcpu.h"
#include "memory.h"
#include "cpuagent.h"
#include "elfload.h"
#include "mpcio.h"
#include "mpc107.h"

typedef struct mpc107_carea	carea_t;
typedef struct cpuagent_info	cpuagent_info_t;

char *_mpc_pcsr;

static struct {
	int		fd;
	cpuagent_info_t	info;
	
	carea_t		*ca;

	char		*mem;
	const char	*mem_cached;
	char		*pcsr;
	//dma_ch_t	*dma0;
} mpc;


void
molcpu_mainloop( void )
{
	printm("molcpu_mainloop\n");
	for( ;; )
		;
}

void
molcpu_mainloop_prep( void )
{
	printm("molcpu_mainloop_prep\n");
}

void
molcpu_arch_init( void )
{
	printm("molcpu_arch_init\n");
}

void
molcpu_arch_cleanup( void )
{
	printm("molcpu_arch_cleanup\n");
}

void
check_kmod_version( void )
{
}

void
map_mregs( void ) 
{
	/* for now */
	mregs = (mac_regs_t*)&mpc.mem[0x100000];
}

/* also serves as a generic cleanup */
void
unmap_mregs( void )
{
	int fd = mpc.fd;

	mregs = NULL;

	if( fd != -1 ) {
		flock(fd, LOCK_UN );
		close(fd);
	}
	if( mpc.mem )
		munmap( (char*)mpc.mem, mpc.info.ram_size );
	if( mpc.mem_cached )
		munmap( (char*)mpc.mem_cached, mpc.info.ram_size );
	if( mpc.pcsr )
		munmap( (char*)mpc.pcsr, 0x1000 );
	if( mpc.ca )
		munmap( mpc.ca, 0x1000 );
	mpc.fd = -1;
}

static int
load_mpckernel( void )
{
	const char image_name[] = "/BK/mol-work/src/kmod/mpckernel";
	Elf32_Ehdr ehdr;
	Elf32_Phdr *phdr;
	int fd, i;

	if( !(fd=open(image_name, O_RDONLY)) ) {
		perrorm("Could not open '%s'", image_name );
		return 1;
	}
	if( !is_elf32(fd, 0) || !(phdr=elf32_readhdrs(fd, 0, &ehdr)) ) {
		printm("%s is not an ELF image\n", image_name );
		return 1;
	}
	for( i=0; i<ehdr.e_phnum; i++ ) {
		size_t s = MIN( phdr[i].p_filesz, phdr[i].p_memsz );
		/* printm("filesz: %08X memsz: %08X p_offset: %08X p_vaddr %08X\n", 
		   phdr[i].p_filesz, phdr[i].p_memsz, phdr[i].p_offset, phdr[i].p_vaddr ); */

		if( (uint)(phdr[i].p_paddr + s) >= mpc.info.ram_size ) {
			printm("Bogus ELF segment\n");
			continue;
		}
		lseek( fd, phdr[i].p_offset, SEEK_SET );
		if( read(fd, mpc.mem + phdr[i].p_paddr, s) != s )
			fatal("read failed\n");
	}
	free( phdr );
	close( fd );

	/* ehdr.e_entry; */
	return 0;
}

static int
mpc_reset( void )
{
	int val;
        
	printm("Resetting CPU");
	ioctl( mpc.fd, CPUAGENT_RESET );
	for( mpc.ca->omr0=0 ; mpc.ca->omr0 != 0x12345678 ; )
		read( mpc.fd, &val, 4 );
	printm(" [success]\n");
	return 0;
}


int
open_session( void )
{
	cpuagent_info_t	info;
	int fd, pvr;

	if( (fd=open("/dev/molmpc", O_RDWR)) < 0 ) {
		perrorm("Opening /dev/molmpc");
		return 1;
	}
	if( flock(fd, LOCK_EX | LOCK_NB) < 0 ) {
		printm("The MPC107 CPU board is already in use\n");
		return 1;
	}
	if( ioctl(fd, CPUAGENT_GETINFO, &info) ) {
		perrorm("ioctl CPUAGENT_GETINFO");
		return 1;
	}
	pvr = info.cpu_pvr >> 16;
	printf("PowerPC %s %dMB, PVR %08x [MPC107 %d.%d]\n",
	       (pvr == 0x8) ? "G3" : 
	       (pvr == 0xc) ? "G4 (74000)" :
	       (pvr == 0x8000) ? "G4 (7450)" :
	       (pvr == 0x800c) ? "G4 (7410)" : 
	       "<unknown>",
	       info.ram_size / 0x100000, info.cpu_pvr,
	       (info.card_rev >> 4) & 0xf, info.card_rev & 0xf );

	mpc.mem = mmap( (void*)0x20000000, info.ram_size, PROT_READ | PROT_WRITE, 
		       MAP_SHARED, fd, info.uncached_ram_base );
	if( mpc.mem == MAP_FAILED ) {
		perrorm("mmap ram");
		return 1;
	}
	mpc.mem_cached = mmap( (void*)0x50000000, info.ram_size, PROT_READ | PROT_WRITE, 
			      MAP_SHARED, fd, info.cached_ram_base );
	if( mpc.mem_cached == MAP_FAILED ) {
		perrorm("mmap cached ram");
		return 1;
	}
	_mpc_pcsr = mpc.pcsr = mmap( NULL, 0x1000, PROT_READ | PROT_WRITE,
				     MAP_SHARED, fd, info.io_base );
	if( mpc.pcsr == MAP_FAILED ) {
		perrorm("mmap pcsr");
		return 1;
	}
	mpc.ca = mmap( (void*)0x40000000, 0x1000, PROT_READ | PROT_WRITE,
		      MAP_SHARED, fd, info.carea_base );
	if( mpc.ca == MAP_FAILED ) {
		perrorm("mmap carea");
		return 1;
	}
	mpc.info = info;
	mpc.fd = fd;
	//mpc.dma0 = (dma_ch_t*)(mpc.pcsr + PCSR_DMR0);

	if( mpc_reset() )
		return 1;
	if( load_mpckernel() )
		return 1;

	/* boot mpckernel */
	pcsr_write( 0x1, PCSR_IDBR );
#if 1
	printm("DONE\n");
	sleep(1);
	printm("memory: %08lx %08lx\n", *(ulong*)mpc.mem, *((ulong*)mpc.mem + 1) );
	return 1;
#endif
	return 0;
}

int
fast_multiplexer( int selector, int arg1, int arg2, int arg3, int arg4 )
{
	printm("multiplexer: %d\n", selector );
	return 0;
}
