/* 
 *   Creation Date: <2003/01/04 23:49:14 samuel>
 *   Time-stamp: <2003/02/23 13:01:12 samuel>
 *   
 *	<MolAudioDevice.cpp>
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

#include "mol_config.h"

#include <IOKit/audio/IOAudioControl.h>
#include <IOKit/audio/IOAudioLevelControl.h>
#include <IOKit/audio/IOAudioToggleControl.h>
#include <IOKit/audio/IOAudioDefines.h>
#include <IOKit/IOLib.h>

#include "MolAudioDevice.h"
#include "MolAudioEngine.h"
#include "osi_calls.h"
#include "sound_sh.h"

#define super IOAudioDevice
OSDefineMetaClassAndStructors( MolAudioDevice, IOAudioDevice );

IOService *
MolAudioDevice::probe( IOService *provider, SInt32 *score )
{
	if( OSI_SoundCntl1(kSoundCheckAPI, OSI_SOUND_API_VERSION) < 0 )
		return NULL;
	OSI_SoundCntl1( kSoundOSXDriverVersion, OSX_SOUND_DRIVER_VERSION );

	return this;
}

bool
MolAudioDevice::initHardware( IOService *provider )
{
	if( !super::initHardware(provider) )
		return false;

	// add the hardware init code here
	
	setDeviceName("Mac-on-Linux Audio Device");
	setDeviceShortName("MolAudio");
	setManufacturerName("Ibrium HB");

	//#error Put your own hardware initialization code here...and in other routines!!
	
	if( !createAudioEngine() )
		return false;

	return true;
}

void
MolAudioDevice::free()
{
	super::free();
}

bool
MolAudioDevice::createAudioEngine()
{
	bool result = false;
	MolAudioEngine *audioEngine = NULL;
	IOAudioControl *control;
	
	audioEngine = new MolAudioEngine;
	if( !audioEngine )
		goto Done;
	
	// Init the new audio engine
	// The audio engine subclass could be defined to take any number of parameters for its
	// initialization - use it like a constructor
	if( !audioEngine->init() )
		goto Done;
	
	// Create a left & right output volume control with an int range from 0 to 65535
	// and a db range from -22.5 to 0.0
	// Once each control is added to the audio engine, they should be released
	// so that when the audio engine is done with them, they get freed properly
	control = IOAudioLevelControl::createVolumeControl( 
			65535,					// Initial value
			0,					// min value
			65535,					// max value
			(-22 << 16) + (32768),			// -22.5 in IOFixed (16.16)
			0,					// max 0.0 in IOFixed
			kIOAudioControlChannelIDDefaultLeft,
			kIOAudioControlChannelNameLeft,
			0,					// control ID - driver-defined
			kIOAudioControlUsageOutput );
	if( !control )
		goto Done;
	
	control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)volumeChangeHandler, this);
	audioEngine->addDefaultAudioControl(control);
	control->release();
	
	control = IOAudioLevelControl::createVolumeControl(
			65535,					// Initial value
			0,					// min value
			65535,					// max value
			(-22 << 16) + (32768),			// min -22.5 in IOFixed (16.16)
			0,					// max 0.0 in IOFixed
			kIOAudioControlChannelIDDefaultRight,	// Affects right channel
			kIOAudioControlChannelNameRight,
			0,					// control ID - driver-defined
			kIOAudioControlUsageOutput);
	if( !control )
		goto Done;
		
	control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)volumeChangeHandler, this);
	audioEngine->addDefaultAudioControl(control);
	control->release();

	// Create an output mute control
	control = IOAudioToggleControl::createMuteControl(
			false,					// initial state - unmuted
			kIOAudioControlChannelIDAll,		// Affects all channels
			kIOAudioControlChannelNameAll,
			0,					// control ID - driver-defined
			kIOAudioControlUsageOutput );

	if( !control )
		goto Done;
		
	control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)outputMuteChangeHandler, this);
	audioEngine->addDefaultAudioControl(control);
	control->release();

#if 0
	// Create a left & right input gain control with an int range from 0 to 65535
	// and a db range from 0 to 22.5
	control = IOAudioLevelControl::createVolumeControl(
			65535,					// Initial value
			0,					// min value
			65535,					// max value
			0,					// min 0.0 in IOFixed
			(22 << 16) + (32768),			// 22.5 in IOFixed (16.16)
			kIOAudioControlChannelIDDefaultLeft,
			kIOAudioControlChannelNameLeft,
			0,					// control ID - driver-defined
			kIOAudioControlUsageInput );

	if( !control )
		goto Done;
	
	control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)gainChangeHandler, this);
	audioEngine->addDefaultAudioControl(control);
	control->release();
	
	control = IOAudioLevelControl::createVolumeControl(
			65535,					// Initial value
			0,					// min value
			65535,					// max value
			0,					// min 0.0 in IOFixed
			(22 << 16) + (32768),			// max 22.5 in IOFixed (16.16)
			kIOAudioControlChannelIDDefaultRight,	// Affects right channel
			kIOAudioControlChannelNameRight,
			0,					// control ID - driver-defined
			kIOAudioControlUsageInput);

	if( !control )
		goto Done;
		
	control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)gainChangeHandler, this);
	audioEngine->addDefaultAudioControl(control);
	control->release();

	// Create an input mute control
	control = IOAudioToggleControl::createMuteControl(
			false,					// initial state - unmuted
			kIOAudioControlChannelIDAll,		// Affects all channels
			kIOAudioControlChannelNameAll,
			0,					// control ID - driver-defined
			kIOAudioControlUsageInput);

	if( !control )
		goto Done;
		
	control->setValueChangeHandler((IOAudioControl::IntValueChangeHandler)inputMuteChangeHandler, this);
	audioEngine->addDefaultAudioControl(control);
	control->release();
#endif
	// Active the audio engine - this will cause the audio engine to have start() and initHardware() called on it
	// After this function returns, that audio engine should be ready to begin vending audio services to the system
	activateAudioEngine(audioEngine);
	// Once the audio engine has been activated, release it so that when the driver gets terminated,
	// it gets freed
	audioEngine->release();
	
	result = true;
	
Done:

	if( !result && (audioEngine != NULL) ) {
		audioEngine->release();
	}

	return result;
}

IOReturn
MolAudioDevice::volumeChangeHandler( IOService *target, IOAudioControl *volumeControl,
				     SInt32 oldValue, SInt32 newValue )
{
	IOReturn result = kIOReturnBadArgument;
	MolAudioDevice *audioDevice;
	
	audioDevice = (MolAudioDevice *)target;
	if( audioDevice ) {
		result = audioDevice->volumeChanged(volumeControl, oldValue, newValue);
	}
	
	return result;
}

IOReturn
MolAudioDevice::volumeChanged( IOAudioControl *volumeControl, SInt32 oldValue, SInt32 newValue )
{
	//printm("MolAudioDevice[%p]::volumeChanged(%p, %ld, %ld)\n", this, volumeControl, oldValue, newValue);
	
	if( volumeControl ) {
		//IOLog("\t-> Channel %ld\n", volumeControl->getChannelID());
	
		// Add hardware volume code change 
		if( volumeControl->getChannelID() == kIOAudioControlChannelIDDefaultRight )
			_vol_right = newValue >> 8;
		else
			_vol_left = newValue >> 8;

		uint vol = (_vol_left << 16) | _vol_right;
		//printm("vol-right: %d vol-left: %d\n", vol_right, vol_left );
		OSI_SoundSetVolume( vol, vol, _vol_mute );
	}
	return kIOReturnSuccess;
}
	
IOReturn
MolAudioDevice::outputMuteChangeHandler( IOService *target, IOAudioControl *muteControl,
					 SInt32 oldValue, SInt32 newValue)
{
	IOReturn result = kIOReturnBadArgument;
	MolAudioDevice *audioDevice;
	
	audioDevice = (MolAudioDevice *)target;
	if( audioDevice ) {
		result = audioDevice->outputMuteChanged(muteControl, oldValue, newValue);
	}

	return result;
}

IOReturn
MolAudioDevice::outputMuteChanged( IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
	//printm("MolAudioDevice[%p]::outputMuteChanged(%p, %ld, %ld)\n", this, muteControl, oldValue, newValue);

	// Add output mute code here
	_vol_mute = newValue;

	uint vol = (_vol_left << 16) | _vol_right;
	//printm("vol-right: %d vol-left: %d\n", vol_right, vol_left );
	OSI_SoundSetVolume( vol, vol, _vol_mute );

	return kIOReturnSuccess;
}

#if 0
IOReturn
MolAudioDevice::gainChangeHandler( IOService *target, IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue)
{
	IOReturn result = kIOReturnBadArgument;
	MolAudioDevice *audioDevice;
	
	audioDevice = (MolAudioDevice *)target;
	if( audioDevice ) {
		result = audioDevice->gainChanged(gainControl, oldValue, newValue);
	}
	
	return result;
}

IOReturn
MolAudioDevice::gainChanged( IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue)
{
	IOLog("MolAudioDevice[%p]::gainChanged(%p, %ld, %ld)\n", this, gainControl, oldValue, newValue);
	
	if( gainControl ) {
		IOLog("\t-> Channel %ld\n", gainControl->getChannelID());
	}
	
	// Add hardware gain change code here 

	return kIOReturnSuccess;
}

IOReturn
MolAudioDevice::inputMuteChangeHandler( IOService *target, IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
	IOReturn result = kIOReturnBadArgument;
	MolAudioDevice *audioDevice;
	
	audioDevice = (MolAudioDevice *)target;
	if( audioDevice ) {
		result = audioDevice->inputMuteChanged(muteControl, oldValue, newValue);
	}
	
	return result;
}

IOReturn
MolAudioDevice::inputMuteChanged( IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue)
{
	IOLog("MolAudioDevice[%p]::inputMuteChanged(%p, %ld, %ld)\n", this, muteControl, oldValue, newValue);
	
	// Add input mute change code here
	
	return kIOReturnSuccess;
}
#endif
