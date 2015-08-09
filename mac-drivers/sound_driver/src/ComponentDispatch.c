/* 
 *   Creation Date: <2002/11/24 00:05:14 samuel>
 *   Time-stamp: <2002/12/18 16:13:57 samuel>
 *   
 *	<sdev.c>
 *	
 *	Common routines for dispatching for any sound component
 *
 *   MOL adaption:
 *
 *	Copyright (C) 1999, 2002 Samuel Rydh (samuel@ibrium.se)
 *
 *   Original code by Mark Cookson
 *
 *	Copyright (c) 1993-1999 by Apple Computer, Inc., All Rights Reserved.
 *
 *	You may incorporate this Apple sample source code into your program(s) without
 *	restriction. This Apple sample source code has been provided "AS IS" and the
 *	responsibility for its operation is yours. You are not permitted to redistribute
 *	this Apple sample source code as "Apple sample source code" after having made
 *	changes. If you're going to re-distribute the source, we require that you make
 *	it clear in the source that the code was descended from Apple sample source
 *	code, but that you've made changes.
 */

#include "ComponentDispatch.h"
#include "logger.h"
#include "LinuxOSI.h"

#define DEBUG	0
#define LOG	lprintf

#if 1 /* ? */
ProcInfoType __procinfo=uppSoundComponentEntryPointProcInfo;
#endif
pascal ComponentResult SoundComponentEntryPoint( ComponentParameters *params, void *globals);
RoutineDescriptor MainRD = BUILD_ROUTINE_DESCRIPTOR( uppSoundComponentEntryPointProcInfo, 
						     SoundComponentEntryPoint );

/* Sound Component Entry Point */
pascal ComponentResult 
SoundComponentEntryPoint( ComponentParameters *params, void *globals )
{
	ComponentResult result;
	short selector = params->what;

	/* Unfortunately, the register-mechanism seems to no work
	 * (if the relevant resource bits are set, we NEVER get called...)
	 */
	if( !MOLIsRunning() )
		return badComponentSelector;		// Ummm....

	if( selector < 0 ) {
		/* standard component selectors */
		switch( selector - kComponentRegisterSelect ) {
		case kComponentRegisterSelect - kComponentRegisterSelect:
			result = CallComponentFunctionWithStorageUniv((Handle) globals, params, __SoundComponentRegister);
			break;

		case kComponentVersionSelect - kComponentRegisterSelect:
			return kSoundComponentVersion;
			break;

		case kComponentCanDoSelect - kComponentRegisterSelect:
			result = __SoundComponentCanDo(0, *((short *) &params->params[0]));
			break;

		case kComponentCloseSelect - kComponentRegisterSelect:
			result = CallComponentFunctionWithStorageUniv((Handle) globals, params, __SoundComponentClose);
			break;

		case kComponentOpenSelect - kComponentRegisterSelect:
			result = CallComponentFunctionWithStorageUniv((Handle) globals, params, __SoundComponentOpen);
			break;

		default:
			result = badComponentSelector;
			break;
		}
	} else if( selector < kDelegatedSoundComponentSelectors ) {
		/* selectors that cannot be delegated */
		switch( selector ) {
		case kSoundComponentInitOutputDeviceSelect:
			result = CallComponentFunctionWithStorageUniv((Handle) globals, params, __InitOutputDevice);
			break;

		case kSoundComponentGetSourceSelect:	/* SR */
			result = CallComponentFunctionWithStorageUniv((Handle) globals, params, __GetSource);
			break;

		default:
		case kSoundComponentSetSourceSelect:
		case kSoundComponentGetSourceDataSelect:
		case kSoundComponentSetOutputSelect:
			result = badComponentSelector;
			break;
		}
	} else {
		/* selectors that can be delegated */
		switch( selector ) {
		case kSoundComponentGetInfoSelect:
			result = CallComponentFunctionWithStorageUniv((Handle) globals, params, __SoundComponentGetInfo);
			break;

		case kSoundComponentSetInfoSelect:
			result = CallComponentFunctionWithStorageUniv((Handle) globals, params, __SoundComponentSetInfo);
			break;

		case kSoundComponentStartSourceSelect:
			result = CallComponentFunctionWithStorageUniv((Handle) globals, params, __StartSource);
			break;

		case kSoundComponentPauseSourceSelect: /* SR */
			result = CallComponentFunctionWithStorageUniv((Handle) globals, params, __PauseSource );
			break;

		case kSoundComponentStopSourceSelect: /* SR */
			result = CallComponentFunctionWithStorageUniv((Handle) globals, params, __StopSource );
			break;

		case kSoundComponentPlaySourceBufferSelect:
			result = CallComponentFunctionWithStorageUniv((Handle) globals, params, __SoundComponentPlaySourceBuffer);
			break;

		default:
			result = DelegateComponentCall( params, GLOBAL.sourceComponent );
			break;
		}
	}
	return result;
}


static pascal ComponentResult
__SoundComponentCanDo( void *dummy, short selector )
{
	ComponentResult result;

	/* Unfortunately, the register-mechanism seems to no work
	 * (if the relevant resource bits are set, we NEVER get called...)
	 */
	if( !MOLIsRunning() )
		return false;

	switch( selector ) {
	case kComponentRegisterSelect:
	case kComponentVersionSelect:
	case kComponentCanDoSelect:
	case kComponentCloseSelect:
	case kComponentOpenSelect:
	case kSoundComponentGetSourceSelect:		/* SR */
	case kSoundComponentInitOutputDeviceSelect:
	/* selectors that can be delegated */
	case kSoundComponentStartSourceSelect:
	case kSoundComponentGetInfoSelect:
	case kSoundComponentSetInfoSelect:
	case kSoundComponentPlaySourceBufferSelect:
	case kSoundComponentStopSourceSelect:		/* SR */
	case kSoundComponentPauseSourceSelect:		/* SR */
		result = true;
		break;

	default:
		result = false;
		break;
	}
	return result;
}
