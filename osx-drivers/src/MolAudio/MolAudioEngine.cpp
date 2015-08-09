/* 
 *   Creation Date: <2003/01/04 23:37:17 samuel>
 *   Time-stamp: <2003/08/11 20:46:38 samuel>
 *   
 *	<MolAudioEngine.cpp>
 *	
 *	MOL Audio Engine
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 */

#include "mol_config.h"
#include <IOKit/IOLib.h>
#include <IOKit/IOFilterInterruptEventSource.h>
#include "MolAudioEngine.h"
#include "osi_calls.h"
#include "sound_sh.h"

#define INITIAL_SAMPLE_RATE	44100
#define NUM_CHANNELS		2
#define BIT_DEPTH		16

#define BUF_SIZE		0x4000


#define super IOAudioEngine
OSDefineMetaClassAndStructors(MolAudioEngine, IOAudioEngine)


bool
MolAudioEngine::init()
{
	if( !super::init(NULL) )
		return false;
	return true;
}

bool
MolAudioEngine::initHardware( IOService *provider )
{
	IOAudioSampleRate initialSampleRate;
	IOAudioStream *audioStream;
	IOWorkLoop *workLoop;
	
	if( !super::initHardware(provider) )
		return false;

	// MOL initialization
	_format = kSoundFormat_Stereo | kSoundFormat_S16_BE;
	_bpf = 4;

	for( _rate=INITIAL_SAMPLE_RATE; _rate >= 22050 ; _rate /= 2 )
		if( OSI_SoundCntl2( kSoundQueryFormat, _format, _rate ) >= 0 )
			break;
	if( _rate < 22050 ) {
		printm("Sample rate not supported - audio disabled\n");
		return false;
	}

	OSI_SoundCntl2( kSoundSetFormat, _format, _rate );
	printm("MolAudio 1.%d\n", OSX_SOUND_DRIVER_VERSION );

	// Setup the initial sample rate for the audio engine
	initialSampleRate.whole = _rate;
	initialSampleRate.fraction = 0;
	
	setDescription("Mac-on-Linux Audio Engine");
	setSampleRate( &initialSampleRate );

	// Setup
	// Set the number of sample frames in each buffer
	setNumSampleFramesPerBuffer( BUF_SIZE/_bpf );

	// The driver is block-based (but we zero-fill)...
	setSampleOffset( OSI_SoundCntl(kSoundGetSampleOffset)  );
	setSampleLatency( OSI_SoundCntl(kSoundGetSampleLatency) );
	//numErasesPerBuffer = 16;

	// Get the workloop
	if( !(workLoop=getWorkLoop()) )
		return false;

	// Create an interrupt event source through which to receive interrupt callbacks
	// In this case, we only want to do work at primary interrupt time, so
	// we create an IOFilterInterruptEventSource which makes a filtering call
	// from the primary interrupt interrupt who's purpose is to determine if
	// our secondary interrupt handler is to be called.  In our case, we
	// can do the work in the filter routine and then return false to
	// indicate that we do not want our secondary handler called
	_irqSource = IOFilterInterruptEventSource::filterInterruptEventSource(
				this, MolAudioEngine::interruptHandler,
				MolAudioEngine::interruptFilter,
				audioDevice->getProvider() );
	workLoop->addEventSource( _irqSource );

	// Allocate our input and output buffers - a real driver will likely need to 
	// allocate its buffers differently
	if( !(_obuf=(SInt16 *)IOMallocContiguous(BUF_SIZE, 0x1000, &_obuf_phys)) )
		return false;
	OSI_SoundCntl2( kSoundSetupRing, BUF_SIZE, _obuf_phys );
	
//	if( !(_ibuf=(SInt16 *)IOMallocContiguous(BUF_SIZE, 0x1000, &_ibuf_phys)) )
//		return false;
	
	// Create an IOAudioStream for each buffer and add it to this audio engine
	audioStream = createNewAudioStream( kIOAudioStreamDirectionOutput, _obuf, BUF_SIZE );
	if( !audioStream )
		return false;
	
	addAudioStream(audioStream);
	audioStream->release();

#if 0
	printm("Don't run erase head...\n");
	// Don't erase...
	setRunEraseHead( 0 );
#endif

#if 0
	audioStream = createNewAudioStream( kIOAudioStreamDirectionInput, _ibuf, INPUT_BUFFER_SIZE );
	if( !audioStream )
		return false;

	addAudioStream(audioStream);
	audioStream->release();
#endif	
	return true;
}

void
MolAudioEngine::free()
{
	printm("MolAudioEngine - free\n");
	
	// We need to free our resources when we're going away
	
	if( _obuf ) {
		IOFreeContiguous( _obuf, BUF_SIZE );
		_obuf = NULL;
	}
#if 0	
	if( _ibuf ) {
		IOFreeContiguous( _ibuf, INPUT_BUFFER_SIZE );
		_ibuf = NULL;
	}
#endif	
	super::free();
}

IOAudioStream *
MolAudioEngine::createNewAudioStream( IOAudioStreamDirection direction, void *sampleBuffer, UInt32 sampleBufferSize )
{
	IOAudioStream *audioStream = new IOAudioStream;
	
	// For this sample device, we are only creating a single format and allowing 44.1KHz and 48KHz
	if( !audioStream )
		return NULL;
	
	if( !audioStream->initWithAudioEngine( this, direction, 1 )) {
		audioStream->release();
	} else {
		IOAudioSampleRate rate;
		IOAudioStreamFormat format = {
			2,						// num channels
			kIOAudioStreamSampleFormatLinearPCM,		// sample format
			kIOAudioStreamNumericRepresentationSignedInt,	// numeric format
			BIT_DEPTH,					// bit depth
			BIT_DEPTH,					// bit width
			kIOAudioStreamAlignmentHighByte,		// high byte aligned - unused because bit depth == bit width
			kIOAudioStreamByteOrderBigEndian,		// big endian
			true,						// format is mixable
			0						// driver-defined tag - unused by this driver
		};
			
		// As part of creating a new IOAudioStream, its sample buffer needs to be set
		// It will automatically create a mix buffer should it be needed
		audioStream->setSampleBuffer( sampleBuffer, sampleBufferSize );
		
		// Add the sample rates we support
		rate.fraction = 0;

		if( OSI_SoundCntl2( kSoundQueryFormat, kSoundFormat_S16_BE | kSoundFormat_Stereo, 44100 ) >= 0 ) {
			//printm("Adding 44100\n");
			rate.whole = 44100;
			audioStream->addAvailableFormat(&format, &rate, &rate, (IOAudioStream::AudioIOFunction)clip16BitSamples);
		}
		if( OSI_SoundCntl2( kSoundQueryFormat, kSoundFormat_S16_BE | kSoundFormat_Stereo, 48000 ) == 0 ) {
			//printm("Adding 44800\n");
			rate.whole = 48000;
			audioStream->addAvailableFormat(&format, &rate, &rate, (IOAudioStream::AudioIOFunction)clip16BitSamples);
		}

		if( OSI_SoundCntl2( kSoundQueryFormat, kSoundFormat_S16_BE | kSoundFormat_Stereo, 22050 ) == 0 ) {
			//printm("Adding 22050\n");
			rate.whole = 22050;
			audioStream->addAvailableFormat(&format, &rate, &rate, (IOAudioStream::AudioIOFunction)clip16BitSamples);
		}
		//rate.whole = 48000;
		//audioStream->addAvailableFormat(&format, &rate, &rate);
		
		// Finally, the IOAudioStream's current format needs to be indicated
		audioStream->setFormat( &format );
	}
	return audioStream;
}

void
MolAudioEngine::stop( IOService *provider )
{
	IOLog("MolAudioEngine[%p]::stop(%p)\n", this, provider);
	
	// Add code to shut down hardware (beyond what is needed to simply stop the audio engine)
	// If nothing more needs to be done, this function can be removed

	super::stop(provider);
}
	
IOReturn
MolAudioEngine::performAudioEngineStart()
{
	//printm("--- sound start ---\n");
	
	// When performAudioEngineStart() gets called, the audio engine should be started from the beginning
	// of the sample buffer.  Because it is starting on the first sample, a new timestamp is needed
	// to indicate when that sample is being read from/written to.  The function takeTimeStamp() 
	// is provided to do that automatically with the current time.
	// By default takeTimeStamp() will increment the current loop count in addition to taking the current
	// timestamp.  Since we are starting a new audio engine run, and not looping, we don't want the loop count
	// to be incremented.  To accomplish that, false is passed to takeTimeStamp(). 
	
	// The audio engine will also have to take a timestamp each time the buffer wraps around
	// How that is implemented depends on the type of hardware - PCI hardware will likely
	// receive an interrupt to perform that task
	takeTimeStamp(false);
	
	// Add audio - I/O start code here
	OSI_SoundCntl( kSoundStop );
	OSI_SoundCntl( kSoundStart );
	_irqSource->enable();
	
	return kIOReturnSuccess;
}

IOReturn
MolAudioEngine::performAudioEngineStop()
{
	//printm("--- sound stop ---\n");
	
	// Add audio - I/O stop code here
	OSI_SoundCntl( kSoundFlush );
	OSI_SoundCntl( kSoundStop );
	_irqSource->disable();

	return kIOReturnSuccess;
}
	
UInt32
MolAudioEngine::getCurrentSampleFrame()
{
	/* printm("--- getCurrentSampleFrame --\n"); */
	
	// In order for the erase process to run properly, this function must return the current location of
	// the audio engine - basically a sample counter
	// It doesn't need to be exact, but if it is inexact, it should err towards being before the current location
	// rather than after the current location.  The erase head will erase up to, but not including the sample
	// frame returned by this function.  If it is too large a value, sound data that hasn't been played will be 
	// erased.

	return OSI_SoundCntl(kSoundGetRingSample) % (BUF_SIZE/_bpf);
}
	
IOReturn
MolAudioEngine::performFormatChange(IOAudioStream *audioStream, const IOAudioStreamFormat *newFormat, const IOAudioSampleRate *newSampleRate)
{
	// Since we only allow one format, we only need to be concerned with sample rate changes
	// In this case, we only allow 2 sample rates - 44100 & 48000, so those are the only ones
	// that we check for
	if( newSampleRate ) {
		switch( newSampleRate->whole ) {
		case 44100:
			printm("MolAudio: 44.1kHz selected\n");
			_rate = 44100;
			// Add code to switch hardware to 44.1khz
			break;

		case 48000:
			printm("MolAudio: 48kHz selected\n");
			_rate = 48000;
			// Add code to switch hardware to 48kHz
			break;

		case 22050:
			printm("MolAudio: 22.050kHz selected\n");
			_rate = 22050;
			// Add code to switch hardware to 22.050kHz
			break;

		default:
			// This should not be possible since we only specified 44100 and 48000 as valid sample rates
			printm("Internal Error - unknown sample rate selected.\n");
			break;
		}
		OSI_SoundCntl2( kSoundSetFormat, _format, _rate );
	}
	return kIOReturnSuccess;
}


/************************************************************************/
/*	interrupt handling						*/
/************************************************************************/

void
MolAudioEngine::interruptHandler( OSObject *owner, IOInterruptEventSource *source, int count )
{
	// Since our interrupt filter always returns false, this function will never be called
	// If the filter returned true, this function would be called on the IOWorkLoop
	return;
}

bool
MolAudioEngine::interruptFilter( OSObject *owner, IOFilterInterruptEventSource *source )
{
	MolAudioEngine *audioEngine = OSDynamicCast( MolAudioEngine, owner );

	// We've cast the audio engine from the owner which we passed in when we created the
	// interrupt event source
	if( audioEngine ) {
		// Then, filterInterrupt() is called on the specified audio engine
		audioEngine->filterInterrupt( source->getIntIndex() );
	}
	return false;
}

void
MolAudioEngine::filterInterrupt( int index )
{
	UInt32 timestamp[2];
	
	// In the case of our simple device, we only get interrupts when the audio engine loops to the
	// beginning of the buffer.  When that happens, we need to take a timestamp and increment
	// the loop count.  The function takeTimeStamp() does both of those for us.  Additionally,
	// if a different timestamp is to be used (other than the current time), it can be passed
	// in to takeTimeStamp()

	if( OSI_SoundIRQAck(timestamp) ) {

		AbsoluteTime stamp;
		stamp.hi = timestamp[0];
		stamp.lo = timestamp[1];
		takeTimeStamp( true, &stamp );
	}
}

