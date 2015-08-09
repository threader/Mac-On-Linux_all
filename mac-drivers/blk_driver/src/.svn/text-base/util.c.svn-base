/* 
 *   Creation Date: <1999/03/20 07:30:18 samuel>
 *   Time-stamp: <2002/12/29 16:41:45 samuel>
 *   
 *	<util.c>
 *	
 *   Copyright (C) 1999, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 */
 
#include "headers.h"
#include "logger.h"
#include "util.h"
#include "global.h"


typedef struct MyDrvQEl {		/* Custom drive queue element. */
	long		flags;
	QElemPtr	qLink;
	short		qType;
	short		dQDrive;
	short		dQRefNum;
	short		dQFSID;
	unsigned short	dQDrvSz;
	unsigned short	dQDrvSz2;
	
	/* MOL fields... */
	unsigned long	id;

} MyDrvQEl, *MyDrvQElPtr;

/* Find the first unused drive number, allocate and initialize a drive queue
 * element (including the drive flags), and add the drive queue element
 * to the drive queue.
 */
OSStatus 
AddDriveToQueue( long flags, long numBlocks, int drvrRef, int *driveNum )
{
	MyDrvQElPtr newDrivePtr;
	QHdrPtr driveQHdr;
	DrvQEl *drivePtr;
	static int id=0x2000;
	
	driveQHdr = GetDrvQHdr();

	/* find first free drive number (by convention, start with drvnum 8 for harddisks) */
	*driveNum = 9;		/* drive numbers 1-4 are reserved */
		
	for( ;; ) {
		/* For an explanation of the extra +1 check, see below */
		for( drivePtr=(DrvQEl*)driveQHdr->qHead ; drivePtr ; drivePtr=(DrvQEl*)drivePtr->qLink )
			if( *driveNum == drivePtr->dQDrive || *driveNum == drivePtr->dQDrive + 1 )
				break;
		
		if( !drivePtr )
			break;
		else
			++(*driveNum);
	}

	/* allocate new drive queue element */
	newDrivePtr = (MyDrvQElPtr)PoolAllocateResident(sizeof(MyDrvQEl), true /*clear*/ );
	if( !newDrivePtr )
		return memFullErr;
	
	newDrivePtr->flags	= flags;
	newDrivePtr->qType	= 1;				/* see IM vol.4 p.181 */
	newDrivePtr->dQDrive	= *driveNum;			/* dQDrive and dQRefNum are filled */
	newDrivePtr->dQRefNum	= drvrRef;			/* in by AddDrive */
	newDrivePtr->dQFSID	= 0;				/* HFS */
	newDrivePtr->dQDrvSz	= numBlocks & 0x0000FFFF;	/* dQDrvSz  = LoWord of size */
	newDrivePtr->dQDrvSz2	= numBlocks >> 16;		/* dQDrvSz2 = HiWord of size */
			
	/* AddDrive seems to add an entry with driveNum-1 -- I haven't seen this
	 * documented anywhere. Also, this routine originates form Apple sample source
	 * code and does not expect this behaviour. As a workaround, we lookup the assigned
	 * reference number in the table.
	 */
	newDrivePtr->id = id;
			 
	/* WARNING: The PPC glue for AddDrive in InterfaceLib is broken for MacOS < 8.5.
	 * Refer to the "Monster Disk Tech Note" for more info
	 */
	AddDrive( drvrRef, *driveNum, (DrvQEl*)&newDrivePtr->qLink );
		
	for( drivePtr=(DrvQEl*)driveQHdr->qHead; drivePtr; drivePtr=(DrvQEl*)drivePtr->qLink ) {
		if( newDrivePtr->dQRefNum == drvrRef && newDrivePtr->id == id ) {
			*driveNum = newDrivePtr->dQDrive;
			break;
		}
	}

	if( !drivePtr ) {
		Log("Failed looking up drive number!\n");
		return nsDrvErr;
	}
	id++;
	return noErr;
}

/* XXX: Is this the correct thing to do? */
OSStatus
UpdateDriveQueue( long flags, long numBlocks, int drvrRef, int driveNum ) 
{
	QHdrPtr driveQHdr = GetDrvQHdr();
	DrvQElPtr drivePtr;

	drivePtr = (DrvQEl*)driveQHdr->qHead;
	while( drivePtr && (driveNum != drivePtr->dQDrive) )
		drivePtr = (DrvQEl *)drivePtr->qLink;
	
	if( drivePtr && drivePtr->dQRefNum == drvrRef ) {
		MyDrvQEl *p = (MyDrvQEl*)((char*)drivePtr - offsetof(MyDrvQEl, qLink));
		lprintf("Updating id %d\n", p->dQDrive );
		p->flags = flags;
		p->dQDrvSz = numBlocks & 0x0000FFFF;
		p->dQDrvSz2 = numBlocks >> 16;
		p->dQFSID = 0; /* HFS */
		return noErr;
	}
	lprintf("UpdateDriveQueue: Error\n");
	return nsDrvErr;
}


/* Find the drive queue element for driveNum in the drive queue and Dequeue it.
 * Return pointer to the drive queue element removed in *drivePtr.
 */
OSStatus 
RemoveDriveFromQueue( int driveNum )
{
	OSStatus err = nsDrvErr;
	QHdrPtr driveQHdr = GetDrvQHdr();
	DrvQElPtr drivePtr;
	
	drivePtr = (DrvQEl*)driveQHdr->qHead;
	while( drivePtr && (driveNum != drivePtr->dQDrive) )
		drivePtr = (DrvQEl *)drivePtr->qLink;
	
	if( drivePtr ) {
		err = Dequeue( (QElem*)drivePtr, driveQHdr );
		if( !err ) {
			char *p = (char*)drivePtr - offsetof(MyDrvQEl, qLink);
			PoolDeallocate( p );
		}
	}
	return err;
}
