/* 
 *   MOL adaption of BootX
 *
 *   (C) 2001, 2002 Samuel Rydh, <samuel@ibrium.se>
 */
/*
 * Copyright (c) 2000 Apple Computer, Inc. All rights reserved.
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
/*
 *  macho.c - Functions for decoding a Mach-o Kernel.
 *
 *  Copyright (c) 1998-2000 Apple Computer, Inc.
 *
 *  DRI: Josh de Cesare
 */

#include "darwin_headers.h"
#include "mach-o/fat.h"
#include "mach-o/loader.h"

#include "sl.h"

static long DecodeSegment(long cmdBase);
static long DecodeUnixThread(long cmdBase);
static long DecodeSymbolTable(long cmdBase);

static long gPPCOffset=0;


/************************************************************************/
/* DecodeMachO								*/
/************************************************************************/

long 
DecodeMachO( void )
{
	struct fat_header  *fH;
	struct fat_arch    *fA;
	struct mach_header *mH;
	long   ncmds, cmdBase, cmd, cmdsize, headerBase, headerAddr, headerSize;
	long   cnt, ret=-1;
	
	// Test for a fat header.
	fH = (struct fat_header *)kLoadAddr;
	if (fH->magic == FAT_MAGIC) {
		fA = (struct fat_arch *)(kLoadAddr + sizeof(struct fat_header));
		// see if the there is one for PPC.
		for (cnt = 0; cnt < fH->nfat_arch; cnt++) {
			if (fA[cnt].cputype == CPU_TYPE_POWERPC) {
				gPPCOffset = fA[cnt].offset;
			}
		}
	}
	
	// offset will be the start of the 
	headerBase = kLoadAddr + gPPCOffset;
	cmdBase = headerBase + sizeof(struct mach_header);
	
	mH = (struct mach_header *)(headerBase);
	if (mH->magic != MH_MAGIC) 
		return -1;
	
#if 0
	printm("  magic:      %lx\n", mH->magic);
	printm("  cputype:    %x\n",  mH->cputype);
	printm("  cpusubtype: %x\n",  mH->cpusubtype);
	printm("  filetype:   %lx\n",  mH->filetype);
	printm("  ncmds:      %lx\n",  mH->ncmds);
	printm("  sizeofcmds: %lx\n",  mH->sizeofcmds);
	printm("  flags:      %lx\n",  mH->flags);
#endif
	
	ncmds = mH->ncmds;
	
	for (cnt = 0; cnt < ncmds; cnt++) {
		cmd = ((long *)cmdBase)[0];
		cmdsize = ((long *)cmdBase)[1];
		
		switch (cmd) {
			
		case LC_SEGMENT:
			ret = DecodeSegment(cmdBase);
			break;

		case LC_UNIXTHREAD:
			ret = DecodeUnixThread(cmdBase);
			break;
			
		case LC_SYMTAB:
			ret = DecodeSymbolTable(cmdBase);
			break;
			
		default:
			printm("Ignoring cmd type %ld.\n", cmd);
			break;
		}
		
		if (ret != 0) 
			return -1;
		
		cmdBase += cmdsize;
	}
	
	// Save the mach-o header.
	headerSize = cmdBase - headerBase;
	headerAddr = AllocateKernelMemory(headerSize);
	bcopy((char *)headerBase, (char *)headerAddr, headerSize);
	
	// Add the Mach-o header to the memory-map.
	AllocateMemoryRange("Kernel-__HEADER", headerAddr, headerSize);
	
	return ret;
}


/************************************************************************/
/* Private Functions							*/
/************************************************************************/

static long
DecodeSegment(long cmdBase)
{
	struct segment_command *segCmd;
	char   rangeName[32];
	char   *vmaddr, *fileaddr;
	long   vmsize, filesize;
	
	segCmd = (struct segment_command *)cmdBase;
	
	vmaddr = (char *)segCmd->vmaddr;
	vmsize = segCmd->vmsize;
	
	fileaddr = (char *)(kLoadAddr + gPPCOffset + segCmd->fileoff);
	filesize = segCmd->filesize;
	
#if 0
	printm("segname: %s, vmaddr: %x, vmsize: %lx, fileoff: %x, filesize: %lx, nsects: %ld, flags: %lx.\n",
	       segCmd->segname, vmaddr, vmsize, (int)fileaddr, filesize,
	       segCmd->nsects, segCmd->flags);
#endif
	
	// Add the Segment to the memory-map.
	sprintf(rangeName, "Kernel-%s", segCmd->segname);
	AllocateMemoryRange(rangeName, (long)vmaddr, vmsize);
	
	// Handle special segments first.
#if 0	
	// If it is the vectors, save them in their special place.
	if ((strcmp(segCmd->segname, "__VECTORS") == 0) &&
	    ((long)vmaddr < (kVectorAddr + kVectorSize)) &&
	    (vmsize != 0) && (filesize != 0)) {
		
		// Copy the first part into the save area.
		bcopy(fileaddr, gVectorSaveAddr,
		      (filesize <= kVectorSize) ? filesize : kVectorSize);
		
		// Copy the rest into memory.
		if (filesize > kVectorSize)
			bcopy(fileaddr + kVectorSize, (char *)kVectorSize,
			      filesize - kVectorSize);
		
		return 0;
	}
#endif
	
	// It is nothing special, so do the usual. Only copy sections
	// that have a filesize.  Others are handle by the original bzero.
	if (filesize != 0) {
		bcopy(fileaddr, vmaddr, filesize);
	}
	
	// Adjust the last address used by the kernel
	if ((long)vmaddr + vmsize > AllocateKernelMemory(0)) {
		AllocateKernelMemory((long)vmaddr + vmsize - AllocateKernelMemory(0));
	}
	
	return 0;
}


static long
DecodeUnixThread(long cmdBase)
{
	struct ppc_thread_state *ppcThreadState;
	
	// The PPC Thread State starts after the thread command stuct plus,
	// 2 longs for the flaver an num longs.
	ppcThreadState = (struct ppc_thread_state *)
		(cmdBase + sizeof(struct thread_command) + 8);
	
	gKernelEntryPoint = ppcThreadState->srr0;
	//printm("gKernelEntryPoint = %lx\n", gKernelEntryPoint);
	return 0;
}


static long 
DecodeSymbolTable(long cmdBase)
{
	struct symtab_command *symTab, *symTableSave;
	long   tmpAddr, symsSize, totalSize;
	
	symTab = (struct symtab_command *)cmdBase;
	
#if 0
	printm("symoff: %lx, nsyms: %lx, stroff: %lx, strsize: %lx\n",
	       symTab->symoff, symTab->nsyms, symTab->stroff, symTab->strsize);
#endif
	
	symsSize = symTab->stroff - symTab->symoff;
	totalSize = symsSize + symTab->strsize;
	
	gSymbolTableSize = totalSize + sizeof(struct symtab_command);
	gSymbolTableAddr = AllocateKernelMemory(gSymbolTableSize);
	
	// Add the SymTab to the memory-map.
	AllocateMemoryRange("Kernel-__SYMTAB", gSymbolTableAddr, gSymbolTableSize);
	
	symTableSave = (struct symtab_command *)gSymbolTableAddr;
	tmpAddr = gSymbolTableAddr + sizeof(struct symtab_command);
	
	symTableSave->symoff = tmpAddr;
	symTableSave->nsyms = symTab->nsyms;
	symTableSave->stroff = tmpAddr + symsSize;
	symTableSave->strsize = symTab->strsize;
	
	bcopy((char *)(kLoadAddr + gPPCOffset + symTab->symoff),
	      (char *)tmpAddr, totalSize);

	return 0;
}

