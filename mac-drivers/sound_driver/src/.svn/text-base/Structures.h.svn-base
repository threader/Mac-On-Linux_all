/* 
 *   Creation Date: <2002/11/24 00:05:14 samuel>
 *   Time-stamp: <2003/02/01 18:14:03 samuel>
 *   
 *	<Structures.h>
 *	
 *	Header for routines which deal with virtual sound hardware from a 'sdev' component.
 *
 *   MOL adaption:
 *
 *   Copyright (C) 1999, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
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

#ifndef _H_STRUCTURES
#define _H_STRUCTURES

#include <Files.h>
#include <SoundComponents.h>
#include <Timer.h>
#include <Types.h>
#include "sdev.h"


#define kSoundComponentVersion	0x30000			/* version for this component */
#define kDelegateSifterCall	((ComponentRoutine)-1L)	/* flag that selector should be delegated instead of called */

#ifndef kFix1
#define kFix1			((Fixed)(0x00010000))	/* 1.0 in fixed point */
#endif

#define kComponentWantsToRegister 	0		/* component wants to be registered */
#define kComponentDoesNotWantToRegister	1		/* component doesn't want to be registered */

struct SoundComponentGlobals {
	/* these are general purpose variables that every component will need */
	ComponentInstance	sourceComponent;	/* component to call when hardware needs more data */
	SoundComponentData	thisSifter;		/* description of data this component outputs */
	Boolean			inSystemHeap;		/* loaded in app or system heap? */
	int			hardwareOn;

	/* these are variables specific to this component implementation */
};

#define GLOBAL (gSoundComponentGlobals)
extern struct SoundComponentGlobals gSoundComponentGlobals;

#endif   /* _H_STRUCTURES */
