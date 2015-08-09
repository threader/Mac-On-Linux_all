/*	.___________________________________________________________________________________.
  	| This function shows how to store a private property into the Name Registry,		|
  	| replacing any existing instance of this property. The property contains a pointer	|
  	| to the driver globals, which a test application may use for debugging. 			|
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
 * Store a private property into the name registry with a pointer to our globals. The
 * test function (or MacsBug) can use this for debugging. Errors are logged and ignored.
 */
void
PublishDriverDebugInfo(void)
{
		OSErr					status;
		DriverGlobalPtr			driverGlobalPtr;
		RegPropertyValueSize	size;

		Trace(PublishDriverDebugInfo);
		
		/*
		 * Globalize the script so that the test driver can locate it using the
		 * Name Registry.
		 */
		driverGlobalPtr = &GLOBAL;
		/*
		 * Does the property currently exist (with the correct size)?
		 */
		status = RegistryPropertyGetSize(		/* returns noErr if the property exists	*/
					&GLOBAL.deviceEntry,
					kDriverGlobalsPropertyName,
					&size
				);
		if (status == noErr) {
			/*
			 * Replace the current value of the property with its new value. In this
			 * example, we are replacing the entire value and, potentially, changing
			 * its size. In production software, you may need to read an existing
			 * property and modify its contents. This shouldn't be too hard to do.
			 */
			status = RegistryPropertySet(		/* Update existing value				*/
						&GLOBAL.deviceEntry,					/* RegEntryID			*/
						kDriverGlobalsPropertyName,				/* Property name		*/
						(RegPropertyValue *) &driverGlobalPtr,	/* -> Property contents	*/
						sizeof driverGlobalPtr					/* Property size		*/
					);
			CheckStatus(status, "PublishDebugInfo - RegistryPropertySet");
		}
		else {
			status = RegistryPropertyCreate(	/* Make a new property					*/
						&GLOBAL.deviceEntry,					/* RegEntryID			*/
						kDriverGlobalsPropertyName,				/* Property name		*/
						(RegPropertyValue *) &driverGlobalPtr,	/* -> Property contents	*/
						sizeof driverGlobalPtr					/* Property size		*/
					);
			CheckStatus(status, "PublishDebugInfo - RegistryPropertyCreate");
		}
}


