/* 
 *   Creation Date: <2002/11/29 01:08:52 samuel>
 *   Time-stamp: <2004/01/06 01:43:15 samuel>
 *   
 *	<sound_sh.h>
 *	
 *	
 *   
 *   Copyright (C) 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_SOUND_SH
#define _H_SOUND_SH

/* This number is used to prompt the user to upgrade the driver */
#define OSX_SOUND_DRIVER_VERSION	2
#define CLASSIC_SOUND_DRIVER_VERSION	2

/* This number is used to prevent old drivers from beeing loaded */
#define OSI_SOUND_API_VERSION		2


/* OSI_SOUND_CNTL selectors */
enum { 
	kSoundCheckAPI		= -1,
	kSoundStop		= 0,
	kSoundStart		= 1,
	kSoundGetBufsize	= 2,		/* used to be called kSoundGetFragsize */
	kSoundRouteIRQ		= 3,
	kSoundFlush		= 4,
	kSoundResume		= 5,
	kSoundQueryFormat	= 6,
	kSoundSetFormat		= 7,
	kSoundPause		= 9,
	kSoundSetupRing		= 10,
	kSoundGetRingSample	= 11,
	kSoundOSXDriverVersion	= 12,
	kSoundClassicDriverVersion = 13,
	kSoundGetSampleOffset	= 14,		/* hands-off zone (in frames) */
	kSoundGetSampleLatency	= 15		/* internal buffering (in frames) */
};

enum {
	kSoundFormat_U8		= 1,
	kSoundFormat_S16_BE	= 2,
	kSoundFormat_S16_LE	= 4,
	kSoundFormat_Stereo	= 16,
	kSoundFormat_Len1Mask	= kSoundFormat_U8
};

#endif   /* _H_SOUND_SH */
