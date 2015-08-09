/* 
 *   Creation Date: <2003/01/05 00:05:00 samuel>
 *   Time-stamp: <2003/02/23 00:30:29 samuel>
 *   
 *	<MolAudioDevice.h>
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

#ifndef _H_MOLAUDIODEVICE
#define _H_MOLAUDIODEVICE


#include <IOKit/audio/IOAudioDevice.h>

//#define MolAudioDevice com_MyCompany_driver_MolAudioDevice

class MolAudioDevice : public IOAudioDevice {

	friend class MolAudioEngine;
 private:
	int			_vol_right;
	int			_vol_left;
	int			_vol_mute;

 public:
	OSDeclareDefaultStructors( MolAudioDevice )

	virtual bool		start( IOService *provider );
	virtual IOService	*probe( IOService *provider, SInt32 *score );

	virtual bool		initHardware(IOService *provider);
	virtual bool		createAudioEngine();
	virtual void		free();
    
	static IOReturn		volumeChangeHandler( IOService *target, IOAudioControl *volumeControl,
						     SInt32 oldValue, SInt32 newValue);
	virtual IOReturn 	volumeChanged( IOAudioControl *volumeControl, SInt32 oldValue,
					       SInt32 newValue);
    
	static IOReturn		outputMuteChangeHandler( IOService *target, IOAudioControl *muteControl,
							 SInt32 oldValue, SInt32 newValue);
	virtual IOReturn	outputMuteChanged( IOAudioControl *muteControl, SInt32 oldValue,
						   SInt32 newValue);

#if 0
	static IOReturn		gainChangeHandler( IOService *target, IOAudioControl *gainControl,
						   SInt32 oldValue, SInt32 newValue);
	virtual IOReturn	gainChanged( IOAudioControl *gainControl, SInt32 oldValue, SInt32 newValue);
    
	static IOReturn		inputMuteChangeHandler( IOService *target, IOAudioControl *muteControl,
							SInt32 oldValue, SInt32 newValue);
	virtual IOReturn	inputMuteChanged( IOAudioControl *muteControl, SInt32 oldValue, SInt32 newValue);
#endif
};


#endif   /* _H_MOLAUDIODEVICE */


