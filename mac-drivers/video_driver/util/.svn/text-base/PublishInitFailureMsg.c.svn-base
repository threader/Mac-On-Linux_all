/*									PublishInitFailureMsg.c								*/
/*
 * PublishInitFailureMsg.c
 * Copyright © 1994-95 Apple Computer Inc. All rights reserved.
 *
 */
/*	.___________________________________________________________________________________.
  	| This function stores a string into the Name Registry. It is called if an			|
  	| initialization function detects an error that would prevent the driver from		|
  	| loading. This may help debugging										 			|
	.___________________________________________________________________________________.
*/

#include "logger.h"

#ifndef __VideoDriverPrivate_H__
#include "VideoDriverPrivate.h"
#endif

#ifndef __VideoDriverPrototypes_H__
#include "VideoDriverPrototypes.h"
#endif

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Store an error value and text string into the registry.
 */
void
PublishInitFailureMsg(									/* Initialization failure		*/
		OSStatus				errorStatus,			/* Writes this value and this	*/
		ConstStr255Param		messageText				/* text into the name registry	*/
	)
{
		OSStatus				status;
		RegPropertyValueSize	size;
		Str255					work;
		SInt32					failureCode;

		Trace(PublishInitFailureMsg);

		PStrCopy(work, (messageText == NULL) ? "\pUnknown reason" : messageText);
		if (work[0] > 254) work[0] = 254;
		work[++work[0]] = 0;							/* C string terminator			*/
		failureCode = errorStatus;

		/*
		 * Does the property currently exist - if so, exit so as we only want the first.
		 */
		status = RegistryPropertyGetSize(		/* returns noErr if the property exists	*/
					&GLOBAL.deviceEntry,
					kDriverFailTextPropertyName,
					&size
				);
		if (status != noErr) {
			status = RegistryPropertyCreate(	/* Make a new property					*/
						&GLOBAL.deviceEntry,					/* RegEntryID			*/
						kDriverFailTextPropertyName,				/* Property name		*/
						(RegPropertyValue *) &work[1],			/* -> Property contents	*/
						work[0]									/* Property size		*/
					);
			CheckStatus(status, "PublishInitFailureMsg - RegistryPropertyCreate");
		} else
		{
			status = RegistryPropertySet(		&GLOBAL.deviceEntry,
												kDriverFailTextPropertyName,
												(RegPropertyValue *) &work[1],
												work[0]	);
		}
		status = RegistryPropertyGetSize(		/* returns noErr if the property exists	*/
					&GLOBAL.deviceEntry,
					kDriverFailCodePropertyName,
					&size
				);
		if (status != noErr) {
			status = RegistryPropertyCreate(	/* Make a new property					*/
						&GLOBAL.deviceEntry,					/* RegEntryID			*/
						kDriverFailCodePropertyName,				/* Property name		*/
						(RegPropertyValue *) &failureCode,			/* -> Property contents	*/
						sizeof(SInt32)								/* Property size		*/
					);
			CheckStatus(status, "PublishInitFailureMsg - RegistryPropertyCreate (2)");
		} else
		{
			status = RegistryPropertySet(	&GLOBAL.deviceEntry,
											kDriverFailCodePropertyName,
											(RegPropertyValue *) &failureCode,
											sizeof(SInt32)	);
		}
}


