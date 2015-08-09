/*	.___________________________________________________________________________________.
  	| These functions manage low-level access to the Name Registry. Because they		|
  	| allocate memory, they must be called from the driver initialization functions.	|
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
 * This is a generic function that retrieves a property from the Name Registry,
 * allocating memory for it in the system pool. It looks in the Name Registry entry
 * for this driver -- the DriverInitializeCmd passed this as one of its parameters.
 * This sample is specific to device drivers: it allocates the property in the resident
 * memory pool.
 */
OSStatus
GetThisProperty(
		RegEntryIDPtr			regEntryIDPtr,			/* Driver's Name Registery ID	*/
		RegPropertyNamePtr		regPropertyName,
		RegPropertyValue		**regPropertyValuePtr,
		RegPropertyValueSize	*regPropertyValueSizePtr
	)
{
		OSStatus				status;

		Trace(GetThisProperty);
		/*
		 * In addition to getting the size of a property, this function will fail if
		 * the property is not present in the registry. We NULL the result before
		 * starting so we can dispose of the property even if this function failed.
		 */
		*regPropertyValuePtr = NULL;
		status = RegistryPropertyGetSize(
					regEntryIDPtr,
					regPropertyName,
					regPropertyValueSizePtr
				);
		//** CheckStatus(status, "RegistryPropertyGetSize failed");
		if (status == noErr) {
			*regPropertyValuePtr = (RegPropertyValue *)
						PoolAllocateResident(*regPropertyValueSizePtr, FALSE);
			status = RegistryPropertyGet(
						regEntryIDPtr,
						regPropertyName,
						*regPropertyValuePtr,
						regPropertyValueSizePtr
					);
			CheckStatus(status, "RegistryPropertyGet failed");
		}
		return (status);
}

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * Dispose of a property that was obtained by calling GetThisProperty(). This sample is
 * specific to device drivers: the property was allocated in the resident memory pool.
 */
void
DisposeThisProperty(
		RegPropertyValue		*regPropertyValuePtr
	)
{
		Trace(DisposeThisProperty);
		if (*regPropertyValuePtr != NULL) {
			PoolDeallocate(*regPropertyValuePtr);
			*regPropertyValuePtr = NULL;
		}
}


