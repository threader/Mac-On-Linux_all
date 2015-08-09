/* 
 *   MOL adaption of BootX
 *
 *   (C) 2001-2004 Samuel Rydh, <samuel@ibrium.se>
 */
/*
 * Copyright (c) 2000, Apple Computer, Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * The contents of this file constitute Original Code as defined in and
 * are subject to the Apple Public Source License Version 1.1 (the
 * "License").  You may not use this file except in compliance with the
 * License.  Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 * 
 * This Original Code and all software distributed under the License are
 * distributed on an "AS IS" basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE OR NON-INFRINGEMENT.  Please see the
 * License for the specific language governing rights and limitations
 * under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */

#include "config.h"
#include "sl.h"
#include "osi_calls.h"
#include "mol.h"

/* Allows proper building within MOL source tree */
#ifdef MOL_PROPER
	#undef MOL_PROPER
#endif

#include "boothelper_sh.h"
#include "ablk_sh.h"
#include "pseudofs_sh.h"
#include "video_sh.h"
#include "molversion.h"

extern void entry( void );
extern void unexpected_excep( int trap );


/************************************************************************/
/*	Globals								*/
/************************************************************************/

long	gImageLastKernelAddr;
long	gImageFirstBootXAddr = kLoadAddr;

long	gKernelEntryPoint;
long 	gDeviceTreeAddr;
long	gDeviceTreeSize;
long	gBootArgsAddr;
long	gBootArgsSize;
long	gSymbolTableAddr;
long	gSymbolTableSize;
char	gTempStr[4096];

CICell	gMemoryMapPH;
CICell	gChosenPH;
long 	*gDeviceTreeMMTmp;

long	gBootFileType;
char	gBootFile[256];
long	gBootMode;

char	gRootDir[256];
char	gBootDevice[256];


long	gBootSourceNumber = -1;
long	gBootSourceNumberMax = 0;


/************************************************************************/
/*	Node matching							*/
/************************************************************************/

static int
match_one( CICell ph, char *name, char *key )
{
	char buf[256];

	if( GetPropLen(ph, name) <= 0 || GetProp(ph, name, buf, sizeof(buf)) < 0 )
		return 1;
	return strcmp( buf, key ) ? 1:0;
}

long
MatchThis( CICell ph, char *string )
{
	char buf[256], *p;
	if( !match_one(ph, "name", string) || !match_one(ph, "model", string) )
		return 0;
	if( GetPropLen(ph, "compatible") <= 0 || GetProp(ph, "compatible", buf, sizeof(buf)) < 0 )
		return -1;
	p = buf;
	while( *p != 0 ) {
		if( !strcmp(p, string) )
			return 0;
		p += strlen(p) + 1;
	}
	return -1;
}


/************************************************************************/
/*	Memory allocations						*/
/************************************************************************/

/* Memory allocations
 *
 *	0x4000		kImageAddr		decoded kernel
 *			Kernel allocations (drivers etc)
 *			---
 *			BootX data (growing downwards)
 *	0x01400000 -	kLoadAddr		used for loading things
 *	0x01c00000
 *
 *	0x01D00000 -	kMallocAddr
 *	0x01E00000
 */

static void
fail_to_boot( char *reason )
{
	printm("--> Boot loader failure: %s\n",
	       reason ? reason : "unspecified error" );
	OSI_Exit();
}
void
unexpected_excep( int trap )
{
	printm("Error: Exception %x occured\n", trap );
	OSI_Exit();
}

void *
AllocateBootXMemory( long size )
{
	long addr = gImageFirstBootXAddr - size;

	if( addr < gImageLastKernelAddr) 
		fail_to_boot("Out of memory");
  
	gImageFirstBootXAddr = addr;

	// printm("AllocateBootXMemory, %lx bytes @%08lX\n", size ,addr);
	return (void *)addr;
}

long 
AllocateKernelMemory( long size )
{
	long addr = gImageLastKernelAddr;
  
	gImageLastKernelAddr += (size + 0xFFF) & ~0xFFF;

	if( gImageLastKernelAddr > gImageFirstBootXAddr )
		fail_to_boot("Out of memory");

	// printm("AllocateKernelMemory %lx @%lx\n",size, addr );
	return addr;
}

long
AllocateMemoryRange( char *rangeName, long start, long length )
{
	long *buffer;
	
	// printm("AllocateMemoryRange '%s' @%08lX (len %lx)\n", rangeName, start, length );

	buffer = AllocateBootXMemory(2 * sizeof(long));
	if( !buffer ) 
		return -1;
  
	buffer[0] = start;
	buffer[1] = length;

	if( SetProp( gMemoryMapPH, rangeName, (char*)buffer, sizeof(long[2])) == -1 )
		return -1;
	return 0;
}


/************************************************************************/
/*	startup splash							*/
/************************************************************************/

static void
startup_splash( void )
{
	int x, y, s, i, width, height;
	osi_fb_info_t fb;
	char *pp, *p;

	width = 0;
	
	if( OSI_GetFBInfo( &fb ) ) {
		printm("startup_splash: No video\n");
		return;
	}
	if( fb.depth == 8 ) {
		for( i=0; i<256; i++ )
			OSI_SetColor( i, i * 0x010101 );
	}
	
	pp = (char*)fb.mphys;
	for( y=0; y<fb.h; y++, pp += fb.rb ) {
		if( fb.depth == 24 || fb.depth == 32 ) {
			ulong *p = (ulong*)pp;
			for( x=0; x<fb.w; x++ )
				*p++ = 0xffffcc;
		} else if( fb.depth == 16 || fb.depth == 15 ) {
			ushort *p = (ushort*)pp;
			for( x=0; x<fb.w; x++ )
				*p++ = (0x12 << 10) | (0x12 << 5) | 0x11;
		} else {
			char *p = (char*)pp;
			for( x=0; x<fb.w; x++ )
				*p++ = 0xcc;
		}
	}

	// Only draw logo in 24-bit mode (for now)
	if( fb.depth != 24 && fb.depth != 32 )
		return;

	for( i=0; i<2; i++ ) {
		char buf[64];
		if( !BootHGetStrResInd( "bootlogo", buf, sizeof(buf), 0, i) )
			return;
		*(!i ? &width : &height) = strtol(buf, NULL, 10);
	}
	if( (s=width * height * 3) > 0x20000 || s<=0 )
		return;
	
	p = malloc( s );
	if( pseudo_load_file("bootlogo", p, s) != s ) {
		printm("failed to load boot logo\n");
	} else {
		int dx, dy;
		dx = (fb.w - width)/2;
		dy = (fb.h - height)/3;
			
		pp = (char*)fb.mphys + dy * fb.rb + dx * 4;
			
		for( y=0 ; y<height; y++, pp += fb.rb ) {
			ulong *d = (ulong*)pp;
			for( x=0; x<width; x++, p+=3, d++ )
				*d = ((int)p[0] << 16) | ((int)p[1] << 8) | p[2];
		}
	}
	free( p );
}


/************************************************************************/
/*	startup disk							*/
/************************************************************************/

static int
device_bootable( char *vol ) 
{
	char dirspec[80];
	long flags, time;

	// assume it is bootable if /mach_kernel and /System/Library exist
	strcpy( dirspec, vol );
	strcat( dirspec, ",\\" );
	if( GetFileInfo( dirspec, "mach_kernel", &flags, &time ) != -1 ) {
		strcat( dirspec, "System" );
		if( GetFileInfo(dirspec, "Library", &flags, &time ) != -1 )
			return 1;
	}
	return 0;
}

enum { kDiskTypeAny=-1, kDiskTypeCD=1, kDiskTypeNotCD };

static int
find_bootable_partition( const char *ofpath, int disktype )
{
	char buf[80];
	int par, ih, is_cd;

	for( par=1 ;; par++ ) {
		sprintf( buf, "%s:%d", ofpath, par );
		if( (ih=Open(buf)) == -1 ) {
			if( par == 1 ) {
				par = -1;	// non-partitioned device
				continue;
			}
			return -1;
		}
		is_cd = IsCD( ih );
		Close( ih );

		if( disktype != kDiskTypeAny ) {
			if( is_cd && disktype != kDiskTypeCD )
				continue;
			if( !is_cd && disktype != kDiskTypeNotCD )
				continue;
		}

		if( device_bootable(buf) ) {
			printm("Candidate boot volume: %s\n", buf );
			SetProp( gChosenPH, "boot-device", buf, strlen(buf)+1 );
			return 0;
		}
		if( !par )
			break;
	}
	return 1;
}

static int
try_scsi_boot( int disktype )
{
	char ofpath[80];
	int id;

	for( id=0; id <= 31; id++ ) {
		if( id == 7 )
			continue;
		sprintf( ofpath, "/mol-scsi/disk@%x", id );
		if( !find_bootable_partition(ofpath, disktype) )
			return 0;
	}
	return 1;
}

static int
try_blkdev_boot( int boot_hint )
{
	ablk_disk_info_t dinfo;
	int channel, unit;
	char ofpath[80];

	/* examine blkdevs (prefere boot-hint volumes) */
	for( channel=0 ;; channel++ ) {			    
		for( unit=0 ;; unit++ ) {
			sprintf( ofpath, "/mol-blk@%x/disk@%x", channel, unit );
			if( OSI_ABlkDiskInfo(channel, unit, &dinfo) )
				break;
			if( boot_hint != !!(dinfo.flags & ABLK_BOOT_HINT) )
				continue;
			if( !find_bootable_partition(ofpath, kDiskTypeAny) )
				return 0;
		}
		if( !unit )
			break;
	}
	return 1;
}

static void
find_bootable_dev( void )
{
	int cdboot;
	char ch;
	
	/* boot from CD? */
	cdboot = BootHGetStrRes("cdboot", &ch, sizeof(ch)) != 0;

	/* try to boot from SCSI-CD */
	if( cdboot && !try_scsi_boot(kDiskTypeCD) )
		return;

	/* try blkdev devices (ablk driver) */
	if( !try_blkdev_boot(1) || !try_blkdev_boot(0) )
		return;

	/* try SCSI non-CD devices */
	if( !try_scsi_boot(kDiskTypeNotCD) )
		return;

	/* try SCSI CD booting (if we didn't earlier) */
	if( !cdboot && !try_scsi_boot(kDiskTypeCD) )
		return;
}


/************************************************************************/
/*	Startup								*/
/************************************************************************/

static void
SetUpBootArgs( void )
{
	int cnt, dash, graphics_boot = 1;
	osi_fb_info_t fb;
	char *p, buf[256];
	boot_args_ptr args;
	Boot_Video *v;

	// Allocate some memory for the BootArgs.
	gBootArgsSize = sizeof(boot_args);
	gBootArgsAddr = AllocateKernelMemory(gBootArgsSize);
	
	// Add the BootArgs to the memory-map.
	AllocateMemoryRange("BootArgs", gBootArgsAddr, gBootArgsSize);
	
	args = (boot_args_ptr)gBootArgsAddr;
	
	args->Revision = kBootArgsRevision;
	args->Version = kBootArgsVersion;
	args->machineType = 0;
	
	// Copy kernel args from resource
	if( !BootHGetStrRes("cmdline", buf, sizeof(buf)) )
		strcpy(buf, "");
	
	strcpy( args->CommandLine, buf );
	for( dash=0, p=args->CommandLine ; *p ; p++ ) {
		if( *p == '-' )
			dash=1;
		if( dash ) {
			if( *p == 's' || *p == 'v' )
				graphics_boot=0;
			if( *p == ' ' || *p == '\t' )
				dash=0;
		}
	}
	if( strlen(args->CommandLine) )
		printm("Command line: '%s'\n", args->CommandLine );

	// Video
	v = &args->Video;
	v->v_baseAddr = v->v_width = v->v_height = v->v_rowBytes = v->v_depth = 0;
	if( !OSI_GetFBInfo( &fb ) ) {
		v->v_baseAddr = fb.mphys;
		v->v_width = fb.w, v->v_height = fb.h;
		v->v_rowBytes = fb.rb;
		v->v_depth = fb.depth;
	}
	v->v_display = graphics_boot;
	//printm("Framebuffer at %08lX (%dx%d)\n", v->v_baseAddr, v->v_width, v->v_height );

	// Memory size
	for( cnt=0; cnt < kMaxDRAMBanks; cnt++ ) {
		args->PhysicalDRAM[cnt].base = 0;
		args->PhysicalDRAM[cnt].size = 0;
	}
	args->PhysicalDRAM[0].base = 0;
	args->PhysicalDRAM[0].size = BootHGetRamSize();

	// The video parameters are initialized in bootx_startup()
	
	// Add the DeviceTree to the memory-map.
	// The actuall address and size must be filled in later.
	AllocateMemoryRange("DeviceTree", 0, 0);
	
	if( FlattenDeviceTree() < 0 )
		fail_to_boot("FlattenDeviceTree failed\n");

	// Fill in the address and size of the device tree.
	gDeviceTreeMMTmp[0] = gDeviceTreeAddr;
	gDeviceTreeMMTmp[1] = gDeviceTreeSize;
	
	args->deviceTreeP = (void *)gDeviceTreeAddr;
	args->deviceTreeLength = gDeviceTreeSize;
	args->topOfKernelData = AllocateKernelMemory(0);
}

void
entry( void ) 
{
	extern char __vectors[], __vectors_end[];
	int s=0, i, is_10_1=0;
	ulong cnt;

	// Copy exception vectors
	memcpy( (char*)0x0, __vectors, __vectors_end - __vectors);
	printm("\n==================================================\n");
	printm("MacOS X Boot Loader %s\n", MOL_RELEASE );
	
	// Prepare for allocations
	gImageLastKernelAddr = (ulong)0;
	gImageFirstBootXAddr = (ulong)kLoadAddr;

	// Setup malloc
	malloc_init( (char *)kMallocAddr, kMallocSize );

	// Draw startup screen
	startup_splash();

	// Initialize memory map
	if( (gMemoryMapPH=CreateNode("/chosen/memory-map/")) == -1 )
		fail_to_boot("Failed to create /chosen/memory-map\n");

	// Determine boot device
	gChosenPH = FindDevice("/chosen");
	find_bootable_dev();
	if( (s=GetProp( gChosenPH, "boot-device", gBootDevice, sizeof(gBootDevice))) == -1 )
		fail_to_boot("No bootable disk found\n");
	gBootDevice[s] = 0;

	// Load kernel to kLoadAddr
      	s=pseudo_load_file( "mach_kernel", (char*)kLoadAddr, kImageSize );
	if( s > 0 ) {
		printm("Linux-side mach kernel loaded\n");
		sprintf(gBootFile, "%s,\\mach_kernel", gBootDevice );
	}
	if( s <= 0 ) {
		sprintf(gBootFile, "%s,\\mach_kernel.mol", gBootDevice );
		//printm("Trying to load %s\n", gBootFile);
		if( (s=LoadFile(gBootFile)) < 0 ) {
			sprintf(gBootFile, "%s,\\mach_kernel", gBootDevice );
			//printm("Trying to load %s\n", gBootFile );
			s = LoadFile(gBootFile);
		}
	}
	if( s <= 0 )
		fail_to_boot("failed to load the mach kernel\n");

	// Some heuristic to determine if this is MacOS 10.1 or 10.2 (or later)
	is_10_1 = s < 3400000;
	printm("%s %s(%d bytes)\n", gBootFile, is_10_1?"[10.1] ":"", s );
	DecodeMachO();

	// MOL acceleration helper (do this before drivers are loaded)
	if (setup_kvm_acceleration())
		setup_mol_acceleration();

	// Set the 'rootpath' propery used by OS X to determine the root volume
	SetProp( gChosenPH, "rootpath", gBootFile, strlen(gBootFile) + 1 );
	ConvertFileSpec( gBootFile, gRootDir, NULL );
	strcat( gRootDir, ",\\" );

	// Load linux-side mkext drivers
	for( i=0 ;; i++ ) {
		char buf[80];
		if( !i )
			sprintf( buf, "mkext%s", is_10_1? "1" : "" );
		else
			sprintf( buf, "mkext-%d", i );

		if( (s=pseudo_load_file( buf, (char*)kLoadAddr, kImageSize )) < 0 ) {
			if( i )
				break;
			fail_to_boot("failed to load linux side mkext\n");
		}
//		printm("MacOS X mkext driver archive read (%d bytes)\n", s );
		if( AddDriverMKext() )
			fail_to_boot("Failed to add drivers package!\n");
	}

	// Load drivers from boot disk
	gBootFileType = kBlockDeviceType;
	//gBootMode = kBootModeSafe;
	LoadDrivers(gRootDir);
#if 1
	// Temporary hack (untill we have interrupts)
	SetProp( FindDevice("/pci/MacOnLinuxVideo"), "Ignore VBL", "yes", 4 );
#endif
	// Fill in the BootArgs structure
	SetUpBootArgs();

	printm("==================================================\n\n");

	// Flush icache
	for( cnt=0; cnt < gImageLastKernelAddr; cnt += 0x20 ) {
		extern ulong gEmuAccelPage;
		// don't flush the emuaccel page...
		if( gEmuAccelPage && cnt == gEmuAccelPage )
			cnt += 0x1000;
		__asm__ volatile("dcbst 0,%0 ; sync ; icbi 0,%0 ; sync ; isync" :: "r" (cnt));
	}
	CloseProm();
	
	// Start kernel
	(*(void(*)())gKernelEntryPoint)( gBootArgsAddr, kMacOSXSignature );

	fail_to_boot("bootx_startup returned (should not happen)\n");
}
