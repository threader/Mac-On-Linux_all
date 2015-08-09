/* 
 *   Creation Date: <2004/01/25 17:59:39 samuel>
 *   Time-stamp: <2004/03/06 19:21:29 samuel>
 *   
 *	<context.h>
 *	
 *	VSID allocation
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_CONTEXT
#define _H_CONTEXT

#define CTX_MASK		0xfffff		/* VSID mask >> 4 */

#if 0 /* doesn't compile under 10.3 */
#include <ppc/mappings.h>			/* for incrVSID */
#else
extern unsigned int		incrVSID;
#endif

/*
 * OSX algorithm:
 * 
 *	VSID = ((context * incrVSID) & 0x0fffff) + (((va>>28) << 20) & 0xf00000)
 *
 */

extern int munge_inverse;

#define MUNGE_ADD_NEXT		( incrVSID )
#define MUNGE_MUL_INVERSE	( munge_inverse )
#define MUNGE_ESID_ADD		0x100000	/* top 4 is segment id */
#define MUNGE_CONTEXT(c)	(((c) * MUNGE_ADD_NEXT) & CTX_MASK)

/* mol_contexts == darwin_context * 16 + esid */
#define PER_SESSION_CONTEXTS	0x10000		/* more than we will need (10^6) */
#define FIRST_MOL_CONTEXT(sess)	((CTX_MASK - PER_SESSION_CONTEXTS*((sess)+1)) << 4)
#define LAST_MOL_CONTEXT(sess)	(((CTX_MASK - PER_SESSION_CONTEXTS*(sess)) << 4) | 0xf)

#endif   /* _H_CONTEXT */
