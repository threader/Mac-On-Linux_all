/* 
 *   Creation Date: <2002/11/24 00:05:14 samuel>
 *   Time-stamp: <2002/12/16 23:25:46 samuel>
 *   
 *	<sdev.r>
 *	
 *	Derived from AIFF_writer.r.
 *
 *   Defines needed to make a fat sound output component 'thng' resource.
 *
 *   Original code by Mark Cookson	
 *
 *   Copyright (c) 1996-1999 by Apple Computer, Inc., All Rights Reserved.
 *
 *	You may incorporate this Apple sample source code into your program(s) without
 *	restriction. This Apple sample source code has been provided "AS IS" and the
 *	responsibility for its operation is yours. You are not permitted to redistribute
 *	this Apple sample source code as "Apple sample source code" after having made
 *	changes. If you're going to re-distribute the source, we require that you make
 *	it clear in the source that the code was descended from Apple sample source
 *	code, but that you've made changes.
 *
 *   Adaption for MOL:
 *
 *   Copyright (C) 1999, 2002 Samuel Rydh (samuel@ibrium.se)
 */

#define UseExtendedThingResource	1		/* we want the extended thng resource */
#define SystemSevenOrLater 		1		/* we only run with System 7, right? */
#define FATCOMPONENT			0

#include <SysTypes.r>
#include <Types.r>
#include <Components.r>

#define kSDEVComponentID	128
#define kSDEVSubType		'MOL-'			/* OS Type for component (was AIFW) */

#define kSDEVVersion		0x00030001		/* version for this sifter */
#define kManufacturerType	'MOL-'			/* manufacturer */

#define k68KCodeResType		'cdec'
#define kPPCCodeResType		'cdek'

#define k8BitRawIn		(1 << 0)		/* data description */
#define k8BitTwosIn		(1 << 1)
#define k16BitIn		(1 << 2)
#define kStereoIn		(1 << 3)
#define k8BitRawOut		(1 << 8)
#define k8BitTwosOut		(1 << 9)
#define k16BitOut		(1 << 10)
#define kStereoOut		(1 << 11)
#define kReverse		(1 << 16)		/* function description */
#define kRateConvert		(1 << 17)
#define kCreateSoundSource	(1 << 18)
#define kHighQuality		(1 << 22)		/* performance description */
#define kNonRealTime		(1 << 23)

#define compFlags	( k8BitRawIn | k8BitRawOut | k16BitIn | k16BitOut \
			/* | k8BitTwosIn | k8BitTwosOut */ | kStereoOut \
				| kStereoIn | kHighQuality )

// AIFF writer output component, 68K, PPC

resource 'thng' (kSDEVComponentID, "MOL audio", locked) {
	kSoundOutputDeviceType,					// 'sdev' 
	kSDEVSubType,						// mol specific
	kManufacturerType,
#if 1
	cmpWantsRegisterMessage,				// flags 
	kAnyComponentFlagsMask,					// components flags mask
#else
	0,0,
#endif
	0,0,							// 68K code type, code id
	'STR ',	kSDEVComponentID,				// name
	'STR ',	kSDEVComponentID+1,				// info
	'ICON', kSDEVComponentID,				// icon
	kSDEVVersion,						// version
	componentDoAutoVersion | componentHasMultiplePlatforms \
	| componentLoadResident,				// registration flags
	128,							// icon family ID
	{
		compFlags,
		kPPCCodeResType, kSDEVComponentID,		// code
		platformPowerPC					// platform
	};
};
