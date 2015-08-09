/* 
 *   Creation Date: <2002/11/24 00:05:14 samuel>
 *   Time-stamp: <2002/12/19 03:33:08 samuel>
 *   
 *	<hardware.c>
 *
 *
 *   MOL adaption:
 *
 *   Copyright (C) 1999, 2002 Samuel Rydh (samuel@ibrium.se)
 *
 *   Original code by Mark Cookson
 *
 *   Copyright (c) 1993-1999 by Apple Computer, Inc., All Rights Reserved.
 *
 *	You may incorporate this Apple sample source code into your program(s) without
 *	restriction. This Apple sample source code has been provided "AS IS" and the
 *	responsibility for its operation is yours. You are not permitted to redistribute
 *	this Apple sample source code as "Apple sample source code" after having made
 *	changes. If you're going to re-distribute the source, we require that you make
 *	it clear in the source that the code was descended from Apple sample source
 *	code, but that you've made changes.
 */

#ifndef __SDEV_H__
#define __SDEV_H__

#define kHackBlasterSifterID		128
#define kHackBlasterPanelID		130
#define kHackBlasterSubType		'hack'			/* OS Type for component */

#ifndef forRez

#include "IRQUtils.h"
#include <Components.h>

enum {
	kSampleSizesCount = 2,					/* no. sample sizes supported */
	kSampleRatesCount = 2,					/* no. sample rates supported */
	kChannelsCount = 2					/* no. channels supported */
};

/* hardware configuration structure */

typedef struct {
	long		volume;					// current hardware volume
	long		speakerVolume;				// current speaker volume
	int		hardwareMute;				// mute hardware

	Boolean		dirty;					// true if globals need to be saved in prefs file

	UnsignedFixed	sampleRate;				// current sample rate
	short		sampleSize;				// current sample size
	short		numChannels;				// current num channels

	short		sampleSizes[kSampleSizesCount];
	short		sampleSizesActive[kSampleSizesCount];
	unsigned long	sampleRates[kSampleRatesCount];
	short		sampleRatesActive[kSampleRatesCount];
	short		channels[kChannelsCount];
	short		channelsActive[kChannelsCount];
		
	IRQInfo		irqInfo;
} HardwareGlobals, *HardwareGlobalsPtr;

extern HardwareGlobals gHWGlobals;

#define HW_GLOBAL (gHWGlobals)

#define CLEAR(what)	BlockZero((char*)&what, sizeof what)

#endif /* forRez */
#endif /* __SDEV_H__ */
