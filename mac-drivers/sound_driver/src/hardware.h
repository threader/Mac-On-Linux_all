/* 
 *   Creation Date: <2002/11/24 00:05:14 samuel>
 *   Time-stamp: <2002/12/18 03:31:50 samuel>
 *   
 *	<hardware.h>
 *	
 *	Headers for routines for dealing with virtual sound hardware from 
 *	a 'sdev' component.
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

#ifndef _H_HARDWARE
#define _H_HARDWARE

#include <Errors.h>
#include <Script.h>
#include <SoundInput.h>
#include "Structures.h"

extern OSErr			SetupHardware( void );
extern void			ReleaseHardware( void );
extern void			StartHardware( void );
extern void			StopHardware( void );

extern void			PauseHardware( void );
extern void			ResumeHardware( void );

extern SoundComponentDataPtr	GetMoreSource( void );
extern Handle			NewHandleLockClear(long len, Boolean inSystemHeap);

extern void			mol_audio_init( ComponentInstance self );
extern void			mol_audio_cleanup( ComponentInstance self );

#endif   /* _H_HARDWARE */
