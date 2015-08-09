/* 
 *   Creation Date: <1999/03/20 07:30:18 samuel>
 *   Time-stamp: <2002/12/29 15:59:34 samuel>
 *   
 *	<util.h>
 *	
 *   Copyright (C) 1999, 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 */
 
#ifndef _H_UTIL
#define _H_UTIL

#include "global.h"

extern OSStatus		AddDriveToQueue( long flags, long size, int drvrRef, int *driveNum );
extern OSStatus		UpdateDriveQueue( long flags, long numBlocks, int drvrRef, int driveNum );
extern OSStatus		RemoveDriveFromQueue( int driveNum );


/******** useful macros ***********/

#ifndef MIN
#define MIN(x,y)	(((x)<(y)) ? (x) : (y))
#endif

#ifndef CLEAR
#define CLEAR(what)	BlockZero((char*)&what, sizeof what)
#endif

/******** trace / logging  ***********/

#ifdef DO_TRACE
#define Trace(str) 	lprintf("<BLK-TRACE> %s\n", #str )
#else
#define Trace(str)	do {} while(0)
#endif

#if DO_LOG
#define Log(str) 	lprintf("<BLK-LOG> %s",str )
#else
#define Log(str)	do {} while(0)
#endif


#endif   /* _H_UTIL */
