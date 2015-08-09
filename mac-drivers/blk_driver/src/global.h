/* 
 *   Creation Date: <1999/03/20 07:30:18 samuel>
 *   Time-stamp: <2002/12/24 18:02:44 samuel>
 *   
 *	<global.h>
 *	
 *	Native MOL Block Driver
 *   
 *   Copyright (C) 1999, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *
 */
 
#ifndef _H_GLOBAL
#define _H_GLOBAL

typedef struct DriverGlobal {
	DriverRefNum	refNum;			/* Driver refNum for PB... */
	DCtlEntry	*dce;			/* DCtlEntry pointer */
	
	RegEntryID	deviceEntry;		/* Name Registry Entry ID */

	UInt32		openCount;
} DriverGlobal, *DriverGlobalPtr;

extern DriverGlobal	gDriverGlobal;		/* All interesting globals */
#define GLOBAL		(gDriverGlobal)		/* GLOBAL.field for references */

#define CLEAR(what)	BlockZero((char*)&what, sizeof what)

/* move somewhere else? */
extern OSErr		Initialize( void );
extern OSStatus		DriverControlCmd( CntrlParam *, IOCommandID cmdID );
extern OSStatus		DriverStatusCmd( CntrlParam * );
extern OSStatus		DriverKillIOCmd( ParmBlkPtr );
extern OSStatus		DriverReadCmd( IOParam *, int is_immediate, IOCommandID ioCommandID );
extern OSStatus		DriverWriteCmd( IOParam *, int is_immediate, IOCommandID ioCommandID );

extern void		DoVariousMOLThings( void );

typedef unsigned long	ulong;
typedef unsigned short	ushort;
typedef unsigned int	uint;
typedef unsigned char	uchar;

#endif   /* _H_GLOBAL */
