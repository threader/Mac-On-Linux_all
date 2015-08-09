/* 
 *   Creation Date: <1999/03/20 07:32:53 samuel>
 *   Time-stamp: <2003/07/16 23:36:33 samuel>
 *   
 *	<mol.h>
 *	
 *	Global definitions
 *   
 *   Copyright (C) 1999, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_MOL
#define _H_MOL

typedef struct {
	UInt16			busID;
	UInt8			simSRsrcID;
	UInt8			simSlotNumber;

	char			*SIMstaticPtr;		/* unused */

	RegEntryIDPtr		regEntryIDPtr;

	SCSIUPP	       		enteringSIMProc;
	SCSIUPP		       	exitingSIMProc;
	SCSIMakeCallbackUPP	makeCallbackProc;

	/* Interrupt info */
	InterruptSetMember     	intSetMember;
	void			*intRefCon;
	InterruptHandler	intOldHandlerFunction;
	InterruptEnabler	intEnablerFunction;
	InterruptDisabler	intDisablerFunction;
} DriverGlobal;

extern DriverGlobal		gGlobal;
#define GLOBAL			gGlobal


/* handy definitions */
#define EnteringSIM() 		CallSCSIProc( gGlobal.enteringSIMProc )
#define ExitingSIM() 		CallSCSIProc( gGlobal.exitingSIMProc )
#define MakeCallback(pb) 	CallSCSICallbackProc( gGlobal.makeCallbackProc, pb )

/* Is there a prototype for BlockZero somewhere? */
/* extern void BlockZero( char *ptr, unsigned long len );*/
#define CLEAR(what)		BlockZero((char*)&what, sizeof what)

#define MIN_PB_SIZE		sizeof(SCSIExecIOPB)

/************************************************************************/
/*	external functions						*/
/************************************************************************/

extern void			driver_init( void );
extern void			get_sim_procs( struct SIMInitInfo *info );
extern InterruptMemberNumber	PCIInterruptHandler( InterruptSetMember intMember, 
						     void *refCon, UInt32 theIntCount );
extern void			ReregisterBus( void );

#endif   /* _H_MOL */
