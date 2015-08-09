/* 
 *   Creation Date: <2002/11/24 00:05:14 samuel>
 *   Time-stamp: <2002/12/19 00:41:46 samuel>
 *   
 *	<sdev.c>
 *	
 *	Routines for dispatching a sound hardware output ('sdev') component.	
 *
 *   MOL adaption:
 *
 *   Copyright (C) 1999, 2002 Samuel Rydh (samuel@ibrium.se)
 *
 *   Original code by Mark Cookson based on code by Kip Olson
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

#include <Memory.h>
#include <Errors.h>
#include <Sound.h>
#include <SoundInput.h>
#include <Components.h>
#include <Timer.h>
#include <FixMath.h>
#include <Script.h>
#include <SoundComponents.h>

#include "sdev.h"
#include "ComponentPrototypes.h"
#include "hardware.h"
#include "Structures.h"
#include "logger.h"
#include "LinuxOSI.h"

#define DEBUG		0


/************************************************************************/
/*	component manager methods					*/
/************************************************************************/

/*
 * SoundComponentOpen
 *
 * This routine is called when the Component Manager creates an instance of this
 * component. The routine should allocate global variables in the appropriate heap
 * and call SetComponentInstanceStorage() so the Component Manager can remember
 * the globals and pass them to all the method calls.
 *
 * NOTE: If the component is loaded into the application heap, the value returned by
 * GetComponentRefCon() will be 0.
 * NOTE: Do not attempt to initialize or access hardware in this call, since the
 * Component Manager will call SoundComponentOpen() BEFORE calling RegisterComponent().
 * Instead, initialize the hardware during InitOutputDevice(), described below.
 * NOTE: This routine is never called at interrupt time.
 */

pascal ComponentResult 
__SoundComponentOpen( void *unused, ComponentInstance self )
{
	/* find out if we were loaded in app heap or system heap */
	long a5 = GetComponentInstanceA5( self );
	ComponentResult result = noErr;

	CLEAR( GLOBAL );
	CLEAR( HW_GLOBAL );
	
	mol_audio_init( self );
	return result;
}

/*
 * SoundComponentClose
 *
 * This routine is called when the Component Manager is closing the instance of
 * this component. The routine should make sure all remaining data is written
 * to the hardware and that the hardware is completely turned off. It should
 * delete all global storage and close any other components that were opened.
 * 
 * NOTE: Be sure to check that the globals pointer passed in to this routine is
 * not set to NIL. If the SoundComponentOpen() routine fails for any reason, the Component
 * Manager will call this routine passing in a NIL for the globals.
 * NOTE: This routine is never called at interrupt time.
 */

pascal ComponentResult 
__SoundComponentClose( void *dummy, ComponentInstance self) 
{
	ReleaseHardware();

	/* close mixer */
	if( GLOBAL.sourceComponent ) {
		CloseMixerSoundComponent( GLOBAL.sourceComponent );
		GLOBAL.sourceComponent = NULL;
	}

	/* save preferences */
	mol_audio_cleanup( self );
	
	return noErr;
}

/*
 * SoundComponentRegister
 *
 * This routine is called once, usually at boot time, when the Component Manager
 * is first registering this component. This routine should check to see if the proper
 * hardware is installed and return 0 if it is. If the hardware is not installed,
 * the routine should return 1 and this component will not be registered. This is
 * also an opportunity to do one-time initializations and perhaps register this
 * component again if more than one hardware device is available. Global state information
 * can also be saved in the component refcon by calling SetComponentRefCon();
 *
 * NOTE: The cmpWantsRegisterMessage bit must be set in the component flags of the
 * component in order for this routine to be called.
 * NOTE: This routine is never called at interrupt time.
 */

pascal ComponentResult 
__SoundComponentRegister( void *dummy )
{
	NumVersion installedVersion;
	ComponentResult result;

	/* Check for hardware here. We are always installed, so we return 0
	 *
	 * we can only run if version 3.0 or greater of the sound manager is running
	 * we can check the entire long because the format of NumVersion is BCD data
	 */	
	result = kComponentDoesNotWantToRegister;
	installedVersion = SndSoundManagerVersion();
	if( installedVersion.majorRev >= 3 ) 	/* has 3.0 sound manager */
		result = kComponentWantsToRegister;
	return result;
}


/*
 * InitOutputDevice
 *
 * This routine is called once when the Sound Manager first opens this component.
 * The routine should initialize the hardware to default values, allocate the
 * appropriate mixer component and create any other memory that is required.
 *
 * NOTE: This routine is never called at interrupt time.
 */

pascal ComponentResult 
__InitOutputDevice( void *dummy, long actions )
{
	ComponentResult result;

	/* setup the hardware */
	if( !(result=SetupHardware()) ) {
		/* first create a mixer and tell it the type of data it should output. The
		 * description includes sample format, sample rate, sample size, number of channels
		 * and the size of your optimal interrupt buffer. If a mixer cannot be found that
		 * will output this type of data, an error will be returned.
		 */
		result = OpenMixerSoundComponent( &GLOBAL.thisSifter, 0, &GLOBAL.sourceComponent );
	}
	return result;
}

/*
 * SoundComponentGetInfo
 *
 * This routine returns information about this component to the Sound Manager. A
 * 4-byte OSType selector is used to determine the type and size of the information
 * to return. If the component does not support a selector, it should delegate this
 * call on up the component chain.
 *
 * NOTE: This can be called at interrupt time. However, selectors that return
 * a handle will not be called at interrupt time.
 */

pascal ComponentResult 
__SoundComponentGetInfo( void *dummy, SoundSource sourceID, OSType selector, void *infoPtr ) 
{
	ComponentResult result = noErr;
	SoundInfoListPtr listPtr;
	UnsignedFixed *lp;
	TimeRecord *latency;
	Handle h;
	short *sp;
	int i;
#if DEBUG
	char sel[5];
	sel[4]=0;
	*(long*)&sel = *(long*)&selector;
	lprintf("GetInfo '%s'\n", sel);
#endif

	switch( selector ) {
	case siHardwareVolumeSteps: /* #volume steps */
	case siHeadphoneVolumeSteps:
		*((short*)infoPtr) = 0x100;
		break;

	case siSampleSize: /* return current sample size */
		*((short*)infoPtr) = HW_GLOBAL.sampleSize;
		break;

	case siOutputLatency:
		lprintf("siOuputLatency\n");
		latency = (TimeRecord*)infoPtr;
		if( latency ) {
			latency->value.hi = 0;
			latency->value.lo = 25;		/* sound latency */
			latency->scale = 1000;		/* 1 ms scale */
			latency->base = NULL;
		}
		break;

	case siSampleSizeAvailable: /* return samples sizes available */
		/* space for sample sizes */
		h = NewHandle( sizeof(short) * kSampleSizesCount );
		if( h ) {
			listPtr = (SoundInfoListPtr) infoPtr;
			listPtr->count = 0;
			listPtr->infoHandle = h;

			/* store sample sizes in handle */			
			sp = (short*) *h; 
			for( i=0; i < kSampleSizesCount; i++ ) {
				if( HW_GLOBAL.sampleSizesActive[i]) {
					listPtr->count++;
					*sp++ = HW_GLOBAL.sampleSizes[i];
				}
			}
		} else {
			result = MemError();
		}
		break;
		
	case siSampleRate: /* return current sample rate */
		*((Fixed*)infoPtr) = HW_GLOBAL.sampleRate;
		break;
		
	case siSampleRateAvailable: /* return sample rates available */
		/* space for sample rates */
		if( !(h=NewHandle(sizeof(UnsignedFixed) * kSampleRatesCount)) )
			return MemError();
		listPtr = (SoundInfoListPtr)infoPtr;
		listPtr->count = 0;
		listPtr->infoHandle = h;
			
		lp = (UnsignedFixed*) *h;
			
		/* If the hardware supports a limited set of sample rates, then the list count
		 * should be set to the number of sample rates and this list of rates should be
		 * stored in the handle.
		 */
		for( i=0; i < kSampleRatesCount; i++ ) {
			if( HW_GLOBAL.sampleRatesActive[i] ) {
				listPtr->count++;
				*lp++ = HW_GLOBAL.sampleRates[i];
			}
		}
		break;
		
	case siNumberChannels: /* return current no. channels */
		*((short*)infoPtr) = HW_GLOBAL.numChannels;
		break;
		
	case siChannelAvailable: /* return channels available */
		if( !(h=NewHandle(sizeof(short) * kChannelsCount)) )
			return MemError();
		listPtr = (SoundInfoListPtr)infoPtr;
		listPtr->count = 0;
		listPtr->infoHandle = h;
			
		sp = (short*)*h;
		for( i=0; i < kChannelsCount; ++i ) {
			if( HW_GLOBAL.channelsActive[i]) {
				listPtr->count++;
				*sp++ = HW_GLOBAL.channels[i];
			}
		}
		break;
		
	case siHardwareVolume:
		*((long*)infoPtr) = HW_GLOBAL.volume;
		break;
		
	case siSpeakerVolume:
		*((long*)infoPtr) = HW_GLOBAL.speakerVolume;
		break;
		
	case siHardwareMute:
	case siSpeakerMute:
		*((short*)infoPtr) = HW_GLOBAL.hardwareMute;
		break;

	case siHardwareBusy:
		*((short*)infoPtr) = GLOBAL.hardwareOn;	/* ?? */
		break;
	default: 
#if 0
	{
		char *s[5];
		*(long*)s = selector;
		s[5] = 0;
		lprintf("Unknown selector: '%s'\n", s );
	}
#endif
		/* if you do not handle this selector, then delegate it up the chain */
		result = SoundComponentGetInfo( GLOBAL.sourceComponent, sourceID, selector, infoPtr );
		break;
	}
	return result;
}

/*
 * SoundComponentSetInfo
 *
 * This routine sets information about this component. A 4-byte OSType selector is
 * used to determine the type and size of the information to apply. If the component
 * does not support a selector, it should delegate this call on up the component chain.
 *
 * NOTE: This can be called at interrupt time.
 */

pascal ComponentResult 
__SoundComponentSetInfo(void *dummy, SoundSource sourceID, OSType selector, void *infoPtr)
{
	ComponentResult result = noErr;
	int i, setvol=false;
#if DEBUG
	long sel[2]; sel[0] = selector; sel[1]=0;
	lprintf("SetInfo '%s'\n", (char*)sel);
#endif

	switch( selector ) {
	case siSampleSize: { /* set sample size */
		int sampleSize = (int) infoPtr;
		
		result = siInvalidSampleSize;
		for( i=kSampleSizesCount-1; i >= 0; --i ) {
			if( (HW_GLOBAL.sampleSizesActive[i]) &&
			    (HW_GLOBAL.sampleSizes[i] == sampleSize)) {
				HW_GLOBAL.sampleSize = sampleSize;
				HW_GLOBAL.dirty = true;
				result = noErr;
				break;
			}
		}
		break;
	}

	case siSampleRate: { /* set sample rate */
		UnsignedFixed sampleRate = (UnsignedFixed) infoPtr;

		result = siInvalidSampleRate;
		for( i=kSampleRatesCount-1; i >= 0; --i ) {
			if( (HW_GLOBAL.sampleRatesActive[i]) &&
			    (HW_GLOBAL.sampleRates[i] == sampleRate)) {
				HW_GLOBAL.sampleRate = sampleRate;
				HW_GLOBAL.dirty = true;
				result = noErr;
				break;
			}
		}
		break;
	}

	case siNumberChannels: { /* set no. channels */
		int numChannels = (int) infoPtr;

		result = notEnoughHardware;
		for( i=kChannelsCount-1; i >= 0; --i ) {
			if( (HW_GLOBAL.channelsActive[i]) &&
			    (HW_GLOBAL.channels[i] == numChannels)) {
				HW_GLOBAL.numChannels = numChannels;
				HW_GLOBAL.dirty = true;
				result = noErr;
				break;
			}
		}
		break;
	}

	case siHardwareVolume:
		HW_GLOBAL.volume = (long) infoPtr;
		HW_GLOBAL.speakerVolume = (long) infoPtr;				
		HW_GLOBAL.dirty = true;
		setvol=true;
		break;

	case siHardwareMute:
	case siSpeakerMute:
		HW_GLOBAL.hardwareMute = (int) infoPtr;
		HW_GLOBAL.dirty = true;
		setvol=true;
		break;

	/* if you do not handle this selector, then call up the chain */
	default:
		result = SoundComponentSetInfo( GLOBAL.sourceComponent, sourceID, selector, infoPtr );
		break;
	}

	if( setvol )
		OSI_SoundSetVolume( HW_GLOBAL.volume, HW_GLOBAL.speakerVolume, HW_GLOBAL.hardwareMute );
	return result;
}

pascal ComponentResult 
__GetSource( void *dummy, SoundSource sourceID, ComponentInstance *source )
{
	/* lprintf("__GetSource\n"); */

	*source = GLOBAL.sourceComponent;
	return noErr;
}


/*
 * StartSource
 *
 * This routine is used to start sounds playing that are currently paused. It should
 * first delegate this call up the component chain so the rest of the chain can prepare
 * to play this sound. Then, if the hardware is not already started it should be
 * turned on.
 *
 * NOTE: This can be called at interrupt time.
 */

pascal ComponentResult 
__StartSource( void *dummy, short count, SoundSource *sources )
{
	ComponentResult result;

	/* lprintf("__StartSource %d %08x\n", count, (int)sources[0]); */
	
	/* tell the mixer to start these sources */
	result = SoundComponentStartSource( GLOBAL.sourceComponent, count, sources );

	/* make sure hardware interrupts are running */
	if( !result )
		ResumeHardware();

	return result;
}

pascal ComponentResult 
__StopSource( void *dummy, short count, SoundSource *sources )
{
	/* The mixer could have multiple sources, we should not stop
	 * the hardware unless there are no sources left...
	 */
	/* lprintf("__StopSource %d %08x\n", count, (int)sources[0] ); */

	/* StopHardware(); */
	return SoundComponentStopSource( GLOBAL.sourceComponent, count, sources );
}

pascal ComponentResult 
__PauseSource( void *dummy, short count, SoundSource *sources )
{
	lprintf("__PauseSource %d %08x\n", count, (int)sources[0] );

	/* PauseHardware(); */
	return SoundComponentPauseSource( GLOBAL.sourceComponent, count, sources );
}


/*
 * PlaySourceBuffer
 *
 * This routine is used to specify a new sound to play and conditionally start
 * the hardware playing that sound. It should first delegate this call up the component
 * chain so the rest of the chain can prepare to play this sound. Then, if the
 * hardware is not already started it should be turned on.
 *
 * NOTE: This can be called at interrupt time.
 */

pascal ComponentResult 
__SoundComponentPlaySourceBuffer( void *dummy, SoundSource sourceID,
				  SoundParamBlockPtr pb, long actions )
{
	ComponentResult result;

	/* lprintf("__PlaySourceBuf %08lx ID: %08x\n", actions, (int)sourceID ); */

	/* tell mixer to start playing this new buffer */
	result = SoundComponentPlaySourceBuffer( GLOBAL.sourceComponent, sourceID, pb, actions );
	if( result == noErr ) {
		/* if the kSourcePaused bit is set, then do not turn on your hardware 
		 * just yet (the assumption is that StartSource() will later be used to 
		 * start this sound playing). If this bit is not set, turn your
		 * hardware interrupts on. [Observation: QT calls PlaySourceBuf agin
		 * rather than StartSource]
		 */
		StartHardware();
		if( (actions & kSourcePaused) )
			PauseHardware();
		else
			ResumeHardware();
	}
	return result;
}
