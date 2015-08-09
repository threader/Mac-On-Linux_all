/*
	File:		ComponentDispatch.h

	Contains:	Header for common routines for dispatching for a sound component

	Written by: Mark Cookson	

	Copyright:	Copyright © 1996-1999 by Apple Computer, Inc., All Rights Reserved.

				You may incorporate this Apple sample source code into your program(s) without
				restriction. This Apple sample source code has been provided "AS IS" and the
				responsibility for its operation is yours. You are not permitted to redistribute
				this Apple sample source code as "Apple sample source code" after having made
				changes. If you're going to re-distribute the source, we require that you make
				it clear in the source that the code was descended from Apple sample source
				code, but that you've made changes.

	Change History (most recent first):
				8/16/1999	Karl Groethe	Updated for Metrowerks Codewarror Pro 2.1
				

*/

#ifndef __COMPONENTDISPATCH__
#define __COMPONENTDISPATCH__

#include <Components.h>
#include <Errors.h>
#include <SoundComponents.h>

#ifndef __COMPONENTPROTOTYPES__
#include "ComponentPrototypes.h"
#endif

//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
// types
//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

#if GENERATINGPOWERPC

// These structs are use in PowerMac builds to cast the
// ComponentParameters passed into our component's entry point.

enum {
	uppSoundComponentEntryPointProcInfo = kPascalStackBased
		| RESULT_SIZE(SIZE_CODE(sizeof(ComponentResult)))
		| STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(ComponentParameters *)))
		| STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(void *)))
};

// These are used to create the routine descriptor to call each function.

enum {
        upp__SoundComponentOpenProcInfo = kPascalStackBased
                | RESULT_SIZE(SIZE_CODE(sizeof(ComponentResult)))
                | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(void *)))
                | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(ComponentInstance)))
};

enum {
        upp__SoundComponentCloseProcInfo = kPascalStackBased
                | RESULT_SIZE(SIZE_CODE(sizeof(ComponentResult)))
                | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(void*)))
                | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(ComponentInstance)))
};

enum {
        upp__SoundComponentRegisterProcInfo = kPascalStackBased
                | RESULT_SIZE(SIZE_CODE(sizeof(ComponentResult)))
                | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(void*)))
};

enum {
        upp__InitOutputDeviceProcInfo = kPascalStackBased
                | RESULT_SIZE(SIZE_CODE(sizeof(ComponentResult)))
                | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(void*)))
                | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(long)))
};

enum {
        upp__StartSourceProcInfo = kPascalStackBased
                | RESULT_SIZE(SIZE_CODE(sizeof(ComponentResult)))
                | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(void*)))
                | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(short)))
                | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(SoundSource *)))
};

enum { /* SR */
        upp__StopSourceProcInfo = kPascalStackBased
                | RESULT_SIZE(SIZE_CODE(sizeof(ComponentResult)))
                | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(void*)))
                | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(short)))
                | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(SoundSource *)))
};

enum { /* SR */
        upp__PauseSourceProcInfo = kPascalStackBased
                | RESULT_SIZE(SIZE_CODE(sizeof(ComponentResult)))
                | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(void*)))
                | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(short)))
                | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(SoundSource *)))
};

enum { /* SR */
        upp__GetSourceProcInfo = kPascalStackBased
                | RESULT_SIZE(SIZE_CODE(sizeof(ComponentResult)))
                | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(void*)))
                | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(SoundSource)))
                | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(ComponentInstance *)))
};

enum {
        upp__SoundComponentGetInfoProcInfo = kPascalStackBased
                | RESULT_SIZE(SIZE_CODE(sizeof(ComponentResult)))
                | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(void*)))
                | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(SoundSource)))
                | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(OSType)))
                | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(void *)))
};

enum {
        upp__SoundComponentSetInfoProcInfo = kPascalStackBased
                | RESULT_SIZE(SIZE_CODE(sizeof(ComponentResult)))
                | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(void*)))
                | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(SoundSource)))
                | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(OSType)))
                | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(void *)))
};

enum {
        upp__SoundComponentPlaySourceBufferProcInfo = kPascalStackBased
                | RESULT_SIZE(SIZE_CODE(sizeof(ComponentResult)))
                | STACK_ROUTINE_PARAMETER(1, SIZE_CODE(sizeof(void*)))
                | STACK_ROUTINE_PARAMETER(2, SIZE_CODE(sizeof(SoundSource)))
                | STACK_ROUTINE_PARAMETER(3, SIZE_CODE(sizeof(SoundParamBlockPtr)))
                | STACK_ROUTINE_PARAMETER(4, SIZE_CODE(sizeof(long)))
};

#define CallComponentFunctionWithStorageUniv(storage, params, funcName) \
        CallComponentFunctionWithStorage(storage, params, &funcName##RD)
#define CallComponentFunctionUniv(params, funcName) \
        CallComponentFunction(params, &funcName##RD)
#define INSTANTIATE_ROUTINE_DESCRIPTOR(funcName) RoutineDescriptor funcName##RD = \
        BUILD_ROUTINE_DESCRIPTOR (upp##funcName##ProcInfo, funcName)

INSTANTIATE_ROUTINE_DESCRIPTOR(__SoundComponentOpen);
INSTANTIATE_ROUTINE_DESCRIPTOR(__SoundComponentClose);
INSTANTIATE_ROUTINE_DESCRIPTOR(__SoundComponentRegister);
INSTANTIATE_ROUTINE_DESCRIPTOR(__InitOutputDevice);

INSTANTIATE_ROUTINE_DESCRIPTOR(__StopSource);	/* SR */
INSTANTIATE_ROUTINE_DESCRIPTOR(__GetSource);	/* SR */
INSTANTIATE_ROUTINE_DESCRIPTOR(__PauseSource);	/* SR */

INSTANTIATE_ROUTINE_DESCRIPTOR(__StartSource);
INSTANTIATE_ROUTINE_DESCRIPTOR(__SoundComponentGetInfo);
INSTANTIATE_ROUTINE_DESCRIPTOR(__SoundComponentSetInfo);
INSTANTIATE_ROUTINE_DESCRIPTOR(__SoundComponentPlaySourceBuffer);

#else //GENERATING68K

#define CallComponentFunctionWithStorageUniv(storage, params, funcName) \
        CallComponentFunctionWithStorage(storage, params, (ComponentFunctionUPP)funcName)
#define CallComponentFunctionUniv(params, funcName) \
        CallComponentFunction(params, (ComponentFunctionUPP)funcName)
#endif

#endif //#ifndef __COMPONENTDISPATCH__
