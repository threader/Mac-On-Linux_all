/* 
 *   Creation Date: <2002/11/24 00:05:14 samuel>
 *   Time-stamp: <2004/01/06 01:44:57 samuel>
 *   
 *	<hardware.c>
 *
 *
 *   MOL adaption:
 *
 *   Copyright (C) 1999, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
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

#include "hardware.h"
#include <Devices.h>
#include <DriverServices.h>
#include "logger.h"
#include "LinuxOSI.h"
#include "IRQUtils.h"
#include "sound_sh.h"

#define DEBUG 			0
#define LOG			lprintf

struct SoundComponentGlobals	gSoundComponentGlobals;
HardwareGlobals			gHWGlobals;
static int			hw_initialized=0;

static struct {
	int			fragsize;		/* size of hwbuf */
	char			*buf;
	unsigned long		buf_phys;

	SoundComponentDataPtr	sourceDataPtr;		/* pointer to source data structure */
} hw;


/************************************************************************/
/*	misc								*/
/************************************************************************/

static unsigned long
GetPhysicalAddr( char *ptr )
{
	LogicalToPhysicalTable tab;
	unsigned long count=defaultPhysicalEntryCount;

	tab.logical.address = ptr; 
	tab.logical.count = 4;
	if( GetPhysical( &tab, &count) != noErr ) {
		LOG("GetPhysical return an error!\n");
		return NULL;
	}
	return (unsigned long)tab.physical[0].address;
}


/************************************************************************/
/*	hardware							*/
/************************************************************************/

/* This routine returns the component data pointer to your mixer source. If there
 * is no source or it is empty, it will call down the chain to fill it up.
 */
static SoundComponentDataPtr 
GetMoreSource( void ) 
{
	SoundComponentDataPtr siftPtr = hw.sourceDataPtr;
	ComponentResult result;

	if( !siftPtr || !siftPtr->sampleCount ) {
		/* no data - better get some */
		result = SoundComponentGetSourceData( GLOBAL.sourceComponent, &hw.sourceDataPtr );
		siftPtr = hw.sourceDataPtr;

		/* lprintf("data: %08x %08x -- %d\n", (int)siftPtr, 
			   (int)siftPtr->buffer, siftPtr->sampleCount ); */

		if( result != noErr || !siftPtr || !siftPtr->sampleCount )
			siftPtr = NULL;
	}
	return siftPtr;
}

void
StartHardware( void )
{
	GLOBAL.hardwareOn = 1;
	hw.sourceDataPtr = NULL;
	OSI_SoundCntl( kSoundStart );
}

void
StopHardware( void )
{
	OSI_SoundCntl( kSoundStop );
	GLOBAL.hardwareOn = 0;
	hw.sourceDataPtr = NULL;
}

void
PauseHardware( void )
{
	OSI_SoundCntl( kSoundPause );
}

void
ResumeHardware( void )
{
	/* OSI_SoundCntl( kSoundStart ); */
	OSI_SoundCntl( kSoundResume );
}


#define SamplesToBytes( samples, shift )	(samples << shift)
#define BytesToSamples( bytes, shift )		(bytes >> shift)

static int
PrepareSndFragment( SoundComponentDataPtr siftPtr, char *buf, int size )
{
	int bytesToCopy, sourceShift;

	sourceShift = ((siftPtr->numChannels == 2)? 1:0) + ((siftPtr->sampleSize == 16)? 1:0);
	bytesToCopy = SamplesToBytes( siftPtr->sampleCount, sourceShift /*XX*/ );

	if( bytesToCopy > size )
		bytesToCopy = size;

	BlockMove( siftPtr->buffer, buf, bytesToCopy );

	/* update source pointer (MacOS seems to use these fields
	 * to track our progress and reclaim buffer space
	 */
	siftPtr->buffer += bytesToCopy;
  	siftPtr->sampleCount -= BytesToSamples( bytesToCopy, sourceShift );
	return bytesToCopy;
}

/************************************************************************/
/*	interrupts							*/
/************************************************************************/

static long
SoftInterrupt( void *p1, void *p2 )
{
	SoundComponentDataPtr siftPtr;
	int size = 0;

	if( !GLOBAL.hardwareOn )
		return 0;

	/* lprintf("sound-irq\n"); */

	while( size < hw.fragsize && (siftPtr=GetMoreSource()) ) {
		size += PrepareSndFragment( siftPtr, hw.buf + size, hw.fragsize - size );
		//lprintf("buffer = %08lx %d/%d\n", siftPtr->buffer, size, hw.fragsize );
	}

	if( size > 0 )
		OSI_SoundWrite( hw.buf_phys, size, 1 /*restart*/ );

	if( size != hw.fragsize ) {
		/* this is what Apple's AIFF writer does */ 
		OSI_SoundCntl( kSoundFlush );
		StopHardware();	/* XXXX */
	} else {
		/* get more for next time */
		GetMoreSource();
	}
	return 0;
}

static InterruptMemberNumber
HardwareInterrupt( InterruptSetMember ISTmember, void *refCon, UInt32 theIntCount )
{
	if( !OSI_SoundIrqAck() )
		return kIsrIsNotComplete;

	/* QueueSecondaryInterruptHandler( SoftInterrupt, NULL, NULL, NULL );  */
	SoftInterrupt( NULL, NULL );
	
	return kIsrIsComplete;
}


/************************************************************************/
/*	hardware init / cleanup						*/
/************************************************************************/

static struct {
	int macrate, molrate;
} rate_ctab[ kSampleRatesCount ] = {
	/* these are the only rates that seems to work well in general */
	{ rate44khz,	44100 },
	{ rate22050hz,	22050 }
};

static void
convert_snd_format( int format, int rate, int stereo, int *molformat, int *molrate )
{
	int f=0, r, i;

	for( r=0, i=0; i<sizeof(rate_ctab)/sizeof(rate_ctab[0]); i++ )
		if( rate_ctab[i].macrate == rate )
			r = rate_ctab[i].molrate;

	switch( format ) {
	case kTwosComplement:
		f = kSoundFormat_S16_BE;
		break;
	case kOffsetBinary:
		f = kSoundFormat_U8;
		break;
	}
	if( !r || !f ) {
		lprintf("convert_snd_format: internal error\n");
		r = 44100;
		f = kSoundFormat_S16_BE;
	}
	if( stereo )
		f |= kSoundFormat_Stereo;
	*molformat = f;
	*molrate = r;
}

OSErr
SetupHardware( void ) 
{
	OSErr result = noErr;
	SoundComponentData *s;
	int n, molformat, molrate;

	if( hw_initialized )
		LOG("ERROR: Sound 'hardware' already initialized!\n");

	s = &GLOBAL.thisSifter;
	s->flags = 0;
	s->format = (HW_GLOBAL.sampleSize == 8) ? kOffsetBinary : kTwosComplement;
	s->sampleRate = HW_GLOBAL.sampleRate;
	s->sampleSize = HW_GLOBAL.sampleSize;
	s->numChannels = HW_GLOBAL.numChannels;

	/* Let linux know about the sound format */
		
	convert_snd_format( s->format, s->sampleRate, s->numChannels == 2, &molformat, &molrate );
	OSI_SoundCntl2( kSoundSetFormat, molformat, molrate );
	OSI_SoundSetVolume( HW_GLOBAL.volume, HW_GLOBAL.speakerVolume, HW_GLOBAL.hardwareMute );

	/* determine dubbelbuffer size and allocate transfer buffer */
	hw.fragsize = OSI_SoundCntl( kSoundGetBufsize );
	if( hw.fragsize <= 0 || hw.fragsize > 256 * 1024 ) {
		lprintf("Warning: bad sound fragment size %d\n", hw.fragsize );
		hw.fragsize = 32 * 1024;
	}
	hw.buf = (char*)MemAllocatePhysicallyContiguous( hw.fragsize, TRUE );
	if( !hw.buf ) {
		LOG("Failed to allocate memory!\n");
		return MemError();
	}
	hw.buf_phys = GetPhysicalAddr( hw.buf );

	/* specify optimal interrupt buffer size */
	n = HW_GLOBAL.numChannels * ((HW_GLOBAL.sampleSize == 8) ? 1:2);
	s->sampleCount = hw.fragsize / n;

	/* install interrupt */
	if( InstallIRQ_NT( "mol-audio", NULL, HardwareInterrupt, &HW_GLOBAL.irqInfo ) != noErr ){
		LOG("Failed to install sound interrupt!\n");
		return noHardwareErr;
	}
	OSI_SoundCntl1( kSoundRouteIRQ, HW_GLOBAL.irqInfo.aapl_int );

	hw_initialized = 1;
	return result;
}

void 
ReleaseHardware( void )
{
	if( !hw_initialized )
		return;

	/* lprintf("ReleaseHardware\n"); */

	OSI_SoundCntl( kSoundFlush );
	StopHardware();

	UninstallIRQ( &HW_GLOBAL.irqInfo );

	MemDeallocatePhysicallyContiguous( hw.buf );
	hw_initialized = 0;
}


/************************************************************************/
/*	prefs								*/
/************************************************************************/

static void
ReadPrefs( ComponentInstance self )
{
	Handle h, componentName;
	ComponentDescription componentDesc;
	OSErr err;
	
	if( !(componentName=NewHandle(0)) )
		return;

	if( GetComponentInfo((Component) self, &componentDesc, componentName, NULL, NULL) ) {
		DisposeHandle( componentName );
		return;
	}
	if( !(h=NewHandleClear(sizeof(HW_GLOBAL))) ) {
		DisposeHandle( componentName );
		return;
	}	
	HLock( componentName );
	err = GetSoundPreference( componentDesc.componentSubType, (StringPtr) *componentName, h );
	if( !err && GetHandleSize(h) == sizeof(HW_GLOBAL) ) {
		HardwareGlobals *prefs;
		HLock(h);
		prefs = (HardwareGlobals*)*h;

		HW_GLOBAL.volume = prefs->volume;
		HW_GLOBAL.speakerVolume = prefs->speakerVolume;
		HW_GLOBAL.hardwareMute = prefs->hardwareMute;		
	}
	DisposeHandle( h );
	DisposeHandle( componentName );
}

static void
SavePrefs( ComponentInstance self )
{
	Handle h, componentName;
	ComponentDescription componentDesc;
	OSErr err = noErr;

	if( !HW_GLOBAL.dirty )
		return;

	if( !(componentName=NewHandle(0)) )
		return;

	if( !GetComponentInfo((Component) self, &componentDesc, componentName, NULL, NULL) ) {
		HW_GLOBAL.dirty = 0;
			
		h = NewHandle( sizeof(HW_GLOBAL) );
		HLock(h);
		HLock(componentName);
		BlockMove( &HW_GLOBAL, *h, sizeof(HW_GLOBAL) );
		
		SetSoundPreference(componentDesc.componentSubType, (StringPtr) *componentName, h);
		DisposeHandle( h );
	}
	DisposeHandle( componentName );
}


/************************************************************************/
/*	early init / cleanup (don't touch the hardware yet)		*/
/************************************************************************/

static struct {
	int	stereo,	format, size;
} ratelist[] = {
	{ 1,	kTwosComplement,	16	},
	{ 1,	kOffsetBinary,		8	},
	{ 0,	kTwosComplement,	16	},
	{ 0,	kOffsetBinary,		8	}
};

void 
mol_audio_init( ComponentInstance self )
{
	int j, i, n, ind, single_format, mformat, mrate;
	int has_stereo, has_mono, has_16, has_8;
	
	CLEAR( HW_GLOBAL );
	
	/* setup sample rates */	
	for( i=0; i<kSampleRatesCount; i++ )
		HW_GLOBAL.sampleRates[i] = rate_ctab[i].macrate;

	/* check which modes are supported... */
	n = sizeof(ratelist)/sizeof(ratelist[0]);
	single_format = 0;
	for( ind=-1, j=0; j<n && !single_format; j++ ) {
		int success=0;
		
		/* if no primary mode has been found yet, try again with 
		 * a simpler format/stereo setting 
		 */
		for( i=0; ind<0 && i<kSampleRatesCount; i++ )
			HW_GLOBAL.sampleRatesActive[i] = 1;

		for( i=0; i<kSampleRatesCount; i++ ) {
			/* only probe rates compatible with the primary mode */
			if( !HW_GLOBAL.sampleRatesActive[i] )
				continue;
			convert_snd_format( ratelist[j].format, HW_GLOBAL.sampleRates[i], 
					    ratelist[j].stereo, &mformat, &mrate );
			if( OSI_SoundCntl2(kSoundQueryFormat, mformat, mrate ) < 0 ) {
				/* invalid rate */
				if( ind < 0 ) {
					HW_GLOBAL.sampleRatesActive[i] = 0;
				} else {
					/* format incompatible with the primary mode */
					single_format = 1;
				}
			} else {
				success = 1;
			}
		}
		/* primary mode found? */ 
		if( success && ind < 0 )
			ind = j;
	}
	if( ind < 0 ) {
		lprintf("No modes seem to work. Using 44kHz 16-bit stereo anyway.\n");
		single_format = 1;
		ind = 0;
		for( i=0; i<kSampleRatesCount; i++ )
			HW_GLOBAL.sampleRatesActive[i] = (HW_GLOBAL.sampleRates[i] == rate44khz);
	}

	/* covering all possible cases without bloating is difficult... */
	has_stereo = ratelist[ind].stereo;
	has_16 = (ratelist[ind].size == 16);
	if( single_format ) {
		has_mono = !has_stereo;
		has_8 = !has_16;
	} else {
		has_mono = 1;
		has_8 = 1;
	}

	/* setup sample sizes */			
	HW_GLOBAL.sampleSizes[0] = 8;		/* 8-bit */
	HW_GLOBAL.sampleSizes[1] = 16;		/* 16-bit */
	HW_GLOBAL.sampleSizesActive[0] = has_8;
	HW_GLOBAL.sampleSizesActive[1] = has_16;

	/* setup channels */
	HW_GLOBAL.channels[0] = 1;		/* mono */
	HW_GLOBAL.channels[1] = 2;		/* stereo */
	HW_GLOBAL.channelsActive[0] = has_mono;
	HW_GLOBAL.channelsActive[1] = has_stereo;

	/* defaults */
	HW_GLOBAL.sampleSize = has_mono ? 16 : 8;
	HW_GLOBAL.numChannels = has_stereo ? 2:1;
	for( i=0; i<kSampleRatesCount; i++ )
		if( HW_GLOBAL.sampleRatesActive[i] ) {
			HW_GLOBAL.sampleRate = HW_GLOBAL.sampleRates[i];
			break;
		}
	HW_GLOBAL.dirty = true;
#if 0
	lprintf("--- [ind = %d] Supported Formats: %s%s%s%s --- \n", ind,
		has_stereo ? "Stereo " :"",
		has_mono ? "Mono " : "",
		has_16 ? "16-bit " : "",
		has_8 ? "8-bit " : "");
	for( i=0; i<kSampleRatesCount; i++ ) {
		if( HW_GLOBAL.sampleRatesActive[i] ) {
			convert_snd_format( kTwosComplement, HW_GLOBAL.sampleRates[i], 1,
					    &mformat, &mrate );
			lprintf("%5d Hz %c\n", mrate,
				(HW_GLOBAL.sampleRates[i] == HW_GLOBAL.sampleRate) ? '*' : ' ' );
		}
	}
	lprintf("-------------------------------------\n");
#endif

	/* initial hardware settings for user-prefs */
	HW_GLOBAL.volume = ((long)256 << 16) | 256;
	HW_GLOBAL.speakerVolume = ((long)256 << 16) | 256;
	HW_GLOBAL.hardwareMute = 0;

	ReadPrefs( self );
}

void 
mol_audio_cleanup( ComponentInstance self )
{
	SavePrefs( self );
}
