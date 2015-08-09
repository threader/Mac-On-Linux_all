/*	.___________________________________________________________________________________.
  	| These routines, after modification, will be useful for other drivers. There are	|
  	| several general-purpuse routines that simplify access to the Name Registry. There	|
	| are also some routines that are specific to the sample driver that will be useful	|
	| for other drivers after modification.												|
	.___________________________________________________________________________________.
*/

#include "logger.h"

#ifndef __VideoDriverPrivate_H__
#include "VideoDriverPrivate.h"
#endif

#ifndef __VideoDriverPrototypes_H__
#include "VideoDriverPrototypes.h"
#endif


#define CopyOSTypeToCString(osTypePtr, resultString) do {		\
		BlockCopy(osTypePtr, resultString, sizeof (OSType));	\
		resultString[sizeof (OSType)] = 0;						\
	} while (0)


/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * RetrieveDriverNVRAMParameter retrieves the single property stored in non-volatile
 * memory (NVRAM). By convention, this property is named using our registered
 * creator code. Because the PCI system stores properties indexed by physical slot
 * number, we may retrieve an incorrect property if the user moves cards around.
 * To protect against this, we check the property name.
 *
 * This function must be called from an initialization context.
 *
 * Return status:
 *	noErr			Success: the NVRAM property was retrieved.
 *	nrNotFoundErr	Failure: there was no NVRAM property.
 *	paramErr		Failure: there was an NVRAM property, but not ours.
 *							(We deleted it.)
 *	other			Failure: unexpected Name Registry error.
 */
OSStatus
RetrieveDriverNVRAMProperty(
		RegEntryIDPtr			regEntryIDPtr,		/* Driver's Name Registery ID	*/
		OSType					driverCreatorID,	/* Registered creator code		*/
		UInt8					driverNVRAMRecord[8]
	)
{
		OSStatus				status;
		RegPropertyIter			cookie;
		RegPropertyNameBuf		propertyName;
		RegPropertyValueSize	regPropertyValueSize;
		RegPropertyModifiers	propertyModifiers;
		Boolean					done;
		char					creatorNameString[sizeof (OSType) + 1];

		/*
		 * Search our properties for one with the NVRAM modifier set.
		 */
		status = RegistryPropertyIterateCreate(regEntryIDPtr, &cookie);
		if (status == noErr) {
			while (status == noErr) {
				/*
				 * Get the next property and check its modifier. Break if this is the
				 * NVRAM property (there can be only one for our entry ID).
				 */
				status = RegistryPropertyIterate(&cookie, propertyName, &done);
				if (status == noErr && done == FALSE) {
					status = RegistryPropertyGetMod(
								regEntryIDPtr,
								propertyName,
								&propertyModifiers
							);
					CheckStatus(status, "RegistryPropertyGetMod NVRAM failed");
					if (status == noErr
					 && (propertyModifiers & kRegPropertyValueIsSavedToNVRAM) != 0)
						break;
				}
				/*
				 * There was no NVRAM property. Return nrNotFoundErr by convention.
				 */
				if (status == noErr && done)
					status = nrNotFoundErr;
			}
			RegistryPropertyIterateDispose(&cookie);
			/*
			 * If status == noErr, we have found an NVRAM property. Now,
			 *	1. If it is our creator code, we have found the property, so
			 *		we retrieve the values and store them in the driver's globals.
			 *	2. If it was found with a different creator code, the user has
			 *		moved our card to a slot that previously had a different card
			 *		installed, so delete this property and return (noErr) to use
			 *		the factory defaults.
			 *	3. If it was not found, the property was not set, so we exit
			 *		(noErr) -- the caller will have pre-set the values to
			 *		"factory defaults."
			 */
			if (status == noErr) {
				/*
				 * Cases 1 or 2, check the property.
				 */
				CopyOSTypeToCString(&driverCreatorID, creatorNameString);
				if (CStrCmp(creatorNameString, propertyName) == 0) {	/* Match 	*/
					status = RegistryPropertyGetSize(
								regEntryIDPtr,
								propertyName,
								&regPropertyValueSize
							);
					CheckStatus(status, "RegistryPropertyGetSize NVRAM failed");
					if (status == noErr
					 && regPropertyValueSize == sizeof driverNVRAMRecord) {
						status = RegistryPropertyGet(
									regEntryIDPtr,
									propertyName,
									driverNVRAMRecord,
									&regPropertyValueSize
								);
						CheckStatus(status, "RegistryPropertyGet NVRAM failed");
					}
				}
				else {
					/*
					 * This is an NVRAM property, but it isn't ours. Delete the
					 * property and return an error status so the caller uses
					 * "factory settings"
					 */
					status = RegistryPropertyDelete(
								regEntryIDPtr,
								propertyName
							);
					CheckStatus(status, "RegistryPropertyDelete bad NVRAM entry");
					/*
					 * Since we're returning an error anyway, we ignore the
					 * status code.
					 */
					status = paramErr;
				}
			}
		}
		return (status);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Get the NVRAM property. Return an error if it does not exist, is the wrong size, or
 * is not marked "store in NVRAM." This may be called from a non-initialization context.
 * Errors:
 *	nrNotFoundErr		Not found in the registry
 *	nrDataTruncatedErr	Wrong size
 *	paramErr			Not marked "store in NVRAM"
 */
OSStatus
GetDriverNVRAMProperty(
		RegEntryIDPtr			regEntryIDPtr,			/* Driver's Name Registery ID	*/
		OSType					driverCreatorID,		/* Registered creator code		*/
		UInt8					driverNVRAMRecord[8]	/* Mandated size				*/
	)
{
		OSStatus				status;
		char					creatorNameString[sizeof (OSType) + 1];
		RegPropertyValueSize	size;
		RegPropertyModifiers	modifiers;

		CopyOSTypeToCString(&driverCreatorID, creatorNameString);
		status = RegistryPropertyGetSize(
			regEntryIDPtr,
			creatorNameString,
			&size
		);
		if (status == noErr && size != sizeof driverNVRAMRecord)
			status = nrDataTruncatedErr;
		if (status == noErr) {
			status = RegistryPropertyGetMod(
						regEntryIDPtr,
						creatorNameString,
						&modifiers
					);
		}
		if (status == noErr
		 && (modifiers & kRegPropertyValueIsSavedToNVRAM) == 0)
		 	status = paramErr;
		if (status == noErr) {
			status = RegistryPropertyGet(
						regEntryIDPtr,
						creatorNameString,
						driverNVRAMRecord,
						&size
					);
		}
		return (status);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Update the NVRAM property. Return an error if it was not created. This may be called
 * from PBStatus (or other non-initialization context).
 */
OSStatus
UpdateDriverNVRAMProperty(
		RegEntryIDPtr			regEntryIDPtr,			/* Driver's Name Registery ID	*/
		OSType					driverCreatorID,		/* Registered creator code		*/
		UInt8					driverNVRAMRecord[8]	/* Mandated size				*/
	)
{
		OSStatus				status;
		char					creatorNameString[sizeof (OSType) + 1];

		Trace(UpdateDriverNVRAMProperty);
		CopyOSTypeToCString(&driverCreatorID, creatorNameString);
		/*
		 * Replace the current value of the property with its new value. In this
		 * example, we are replacing the entire value and, potentially, changing
		 * its size. In production software, you may need to read an existing
		 * property and modify its contents. This shouldn't be too hard to do.
		 */
		status = RegistryPropertySet(		/* Update existing value				*/
					regEntryIDPtr,
					creatorNameString,
					driverNVRAMRecord,
					sizeof driverNVRAMRecord
				);
		CheckStatus(status, "StoreDriverNVRAMProperty");
		return (status);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Create the NVRAM property. This must be called from the driver initialization function.
 */
OSStatus
CreateDriverNVRAMProperty(
		RegEntryIDPtr			regEntryIDPtr,			/* Driver's Name Registery ID	*/
		OSType					driverCreatorID,		/* Registered creator code		*/
		UInt8					driverNVRAMRecord[8]	/* Mandated size				*/
	)
{
		OSStatus				status;
		char					creatorNameString[sizeof (OSType) + 1];
		RegPropertyValueSize	size;
		RegPropertyModifiers	modifiers;

		Trace(CreateDriverNVRAMProperty);
		CopyOSTypeToCString(&driverCreatorID, creatorNameString);
		/*
		 * Does the property currently exist (with the correct size)?
		 */
		status = RegistryPropertyGetSize(		/* returns noErr if the property exists	*/
					regEntryIDPtr,
					creatorNameString,
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
						regEntryIDPtr,
						creatorNameString,
						driverNVRAMRecord,
						sizeof driverNVRAMRecord
					);
		}
		else {
			status = RegistryPropertyCreate(	/* Make a new property					*/
						regEntryIDPtr,
						creatorNameString,
						driverNVRAMRecord,
						sizeof driverNVRAMRecord
					);
		}
		/*
		 * If status equals noErr, the property has been stored; set its "save to
		 * non-volatile ram" bit.
		 */
		if (status == noErr) {
			status = RegistryPropertyGetMod(
						regEntryIDPtr,
						creatorNameString,
						&modifiers
					);
		}
		if (status == noErr) {
			/*
			 * Set the NVRAM bit and update the modifiers.
			 */
			modifiers |= kRegPropertyValueIsSavedToNVRAM;
			status = RegistryPropertySetMod(
						regEntryIDPtr,
						creatorNameString,
						modifiers
					);
		}			
		CheckStatus(status, "CreateDriverNVRAMProperty");
		return (status);
}



