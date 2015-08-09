/* 
 *   Creation Date: <2003/01/04 23:57:26 samuel>
 *   Time-stamp: <2003/08/07 01:39:42 samuel>
 *   
 *	<MolAudioEngine.h>
 *	
 *	
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_MOLAUDIOENGINE
#define _H_MOLAUDIOENGINE

#include <IOKit/audio/IOAudioEngine.h>
#include "MolAudioDevice.h"

class IOFilterInterruptEventSource;
class IOInterruptEventSource;

//#define MolAudioEngine com_MyCompany_driver_SampleAudioEngine

class MolAudioEngine : public IOAudioEngine {
	OSDeclareDefaultStructors( MolAudioEngine )

private:	
	SInt16				*_obuf;
	SInt16				*_ibuf;

	IOFilterInterruptEventSource	*_irqSource;

	IOPhysicalAddress		_obuf_phys;
//	IOPhysicalAddress		_ibuf_phys;

	// mol sound format (common for all channels)
	int				_format;
	int				_rate;
	int				_bpf;		// bytes per frame
	
public:
	virtual bool		init();
	virtual void		free();
	
	virtual bool		initHardware( IOService *provider );
	virtual void		stop( IOService *provider );

	// interrupt handling
	static void 		interruptHandler( OSObject *owner, IOInterruptEventSource *source, int count );
	static bool		interruptFilter( OSObject *owner, IOFilterInterruptEventSource *source );
	virtual void 		filterInterrupt(int index);

	// audio stream
	IOAudioStream		*createNewAudioStream( IOAudioStreamDirection direction, 
						       void *sampleBuffer, UInt32 sampleBufferSize );

	virtual IOReturn	performAudioEngineStart();
	virtual IOReturn	performAudioEngineStop();
	
	virtual UInt32		getCurrentSampleFrame();
	
	virtual IOReturn	performFormatChange( IOAudioStream *audioStream, 
						     const IOAudioStreamFormat *newFormat,
						     const IOAudioSampleRate *newSampleRate );

	// clipping
	virtual IOReturn	clipOutputSamples( const void *mixBuf, void *sampleBuf, 
						   UInt32 firstSampleFrame, UInt32 numSampleFrames,
						   const IOAudioStreamFormat *streamFormat,
						   IOAudioStream *audioStream );
	virtual IOReturn	convertInputSamples( const void *sampleBuf, void *destBuf,
						     UInt32 firstSampleFrame, UInt32 numSampleFrames,
						     const IOAudioStreamFormat *streamFormat,
						     IOAudioStream *audioStream );
};

extern "C" {
IOReturn clip16BitSamples( const void *mixBuf, void *sampleBuf, UInt32 firstSampleFrame,
			   UInt32 numSampleFrames, const IOAudioStreamFormat *streamFormat, IOAudioStream *audioStream);
};


#endif   /* _H_MOLAUDIOENGINE */
