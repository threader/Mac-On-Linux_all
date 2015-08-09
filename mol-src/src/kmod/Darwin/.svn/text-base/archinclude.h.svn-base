/* 
 *   Creation Date: <2003/05/27 16:12:29 samuel>
 *   Time-stamp: <2004/04/10 22:22:31 samuel>
 *   
 *	<archinclude.h>
 *	
 *	Headers included under darwin
 *   
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_ARCHINCLUDE
#define _H_ARCHINCLUDE

#ifdef __cplusplus
extern "C" {
#endif

#include "mol_config.h"
#include "kconfig.h"

#ifdef __cplusplus
}
#endif

#ifndef __ASSEMBLY__
#ifdef __cplusplus
#include <sys/systm.h>
#include <IOKit/IOLib.h>
#else
#include <sys/param.h>
#include <sys/types.h>
#include <sys/systm.h>
#include <libkern/libkern.h>
#include <kern/task.h>
#include <kern/thread.h>
#include <IOKit/IOLib.h>
#include <libkern/OSTypes.h>
#endif

typedef uint64_t		u64;
typedef int64_t			s64;
typedef uint32_t		u32;
typedef int32_t			s32;
typedef uint16_t		u16;
typedef int16_t			s16;
typedef uint8_t			u8;
typedef int8_t			s8;

#define printk 			printf
#ifdef __cplusplus
extern "C" {
#endif

#include "dbg.h"

#ifdef __cplusplus
}
#endif

#define	ENOSYS_MOL		ENOSYS
#define EFAULT_MOL		EFAULT

#endif	 /* __ASSEMBLY__ */

#define IS_LINUX		0
#define IS_DARWIN		1

#endif   /* _H_ARCHINCLUDE */

