/*
 *  MOL adaption 11/01/2001
 * 
 *  Samuel Rydh, <samuel@ibrium.se>
 *
 */
/*
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 1.2 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 */

#ifndef _H_XBOOT
#define _H_XBOOT

//#include "prom.h"
#include "memory.h"

// Externs for main.c
typedef struct 
{	
	long	kernelEntryPoint;

	long	symbolTableAddr;
	long	symbolTableSize;

	long	bootArgsAddr;
	long	bootArgsSize;

	long	deviceTreeAddr;
	long	deviceTreeSize;
	long	*deviceTreeMMTmp;

	ulong	memoryMapPH;
} bootx_globals_t;
extern bootx_globals_t gBootX;

// Externs from bootx.c
extern void 	*AllocateBootXMemory(long size);
extern long 	AllocateKernelMemory(long size);
extern long 	AllocateMemoryRange(char *rangeName, long start, long length);

// Externs from device_tree.c
extern void 	FlattenDeviceTree(void);

// Externs for macho.c
extern long 	DecodeMachO(void);

// Externs from driver.c
extern long 	AddDriverMKext( void );

#define TO_MAC(addr)	((int)((int)(addr) - (int)ram.lvbase))
#define TO_LV(addr)	((int)((int)(addr) + ram.lvbase))

#endif   /* _H_XBOOT */
