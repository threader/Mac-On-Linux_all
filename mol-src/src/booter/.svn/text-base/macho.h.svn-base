/* 
 *   Creation Date: <2004/02/24 00:48:44 samuel>
 *   Time-stamp: <2004/02/28 14:38:48 samuel>
 *   
 *	<macho.h>
 *	
 *	
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */

#ifndef _H_MACHO
#define _H_MACHO

typedef struct mach_header {
	unsigned long	magic;
	int		cputype;
	int		cpu_subtype;
	unsigned long	filetype;
	unsigned long	ncmds;
	unsigned long	sizeofcmds;
	unsigned long	flags;
} mach_header_t;

#define MH_MAGIC			0xfeedface

/* filetype */
#define MH_EXECUTE			0x2

/* cputype */
#define CPU_TYPE_POWERPC		18

/* cpusubtype */
#define CPU_SUBTYPE_POWPERPC_ALL	0
#define CPU_SUBTYPE_POWERPC_750		9
#define CPU_SUBTYPE_POWERPC_7400	10
#define CPU_SUBTYPE_POWERPC_7450	11

/* flags (some of them) */
#define MH_NOUNDEFS			1	/* no undefined references */

typedef struct load_command {
	unsigned long	cmd;
	unsigned long	cmdsize;
} load_command_t;

#define LC_SEGMENT			1
#define LC_SYMTAB			2
#define LC_THREAD			4
#define LC_UNIXTHREAD			5

typedef struct segment_command {
	unsigned long	cmd;
	unsigned long	cmdsize;		/* sizeof(segment_command) + n*sizeof(section) */
	char		segname[16];
	unsigned long	vmaddr;
	unsigned long	vmsize;
	unsigned long	fileoff;
	unsigned long	filesize;
	int		maxprot;
	int		initprot;		
	unsigned long	nsects;			/* #sections data structs following this segment */
	unsigned long	flags;
} segment_command_t;

#define SG_HIGHVM		1
#define SG_NORELOC		4

typedef struct section {
	char		sectname[16];		/* e.g. __text or __data */
	char		segname[16];
	unsigned long	addr;			/* virt addr */
	unsigned long	size;
	unsigned long	offset;			/* file offset to section */
	unsigned long	align;
	unsigned long	reloff;			/* offset of first relocation entry */
	unsigned long	nreloc;			/* #relocation entries */
	unsigned long	flags;
	unsigned long	reserved1;
	unsigned long	reserved2;
} section_t;

#define S_REGULAR		0
#define S_ZEROFILL		1
#define S_CSTRING_LITERALS	2
#define S_4BYTE_LITERALS	3
#define S_8BYTE_LITERALS	4
#define S_LITERAL_POINTERS	5

typedef struct thread_command {
	unsigned long	cmd;
	unsigned long	cmdsize;
	unsigned int	flavor;
	unsigned int	count;
	/* valid for PPC */
	unsigned int	srr0;			/* nip */
	unsigned int	srr1;			/* msr */
	unsigned int	gprs[32];
	unsigned int	cr, xer, lr, ctr, mq;
	unsigned int	vrsave;
} thread_command_t;


#endif   /* _H_MACHO */
