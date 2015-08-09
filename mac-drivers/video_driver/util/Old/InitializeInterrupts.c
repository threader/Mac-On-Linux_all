/*	.___________________________________________________________________________________.
  	| This function retrieves the interrupt-service property from the Name Registry		|
  	| It then installs a primary interrupt service routine and enables interrupts.		|
  	| It is independent of the specifics of the framework sample driver.				|
	.___________________________________________________________________________________.
*/


#ifndef __VideoDriverPrivate_H__
#include "VideoDriverPrivate.h"
#endif

#ifndef __VideoDriverPrototypes_H__
#include "VideoDriverPrototypes.h"
#endif

#include "logger.h"

/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 * GetDeviceInterruptPropertyInfo retrieves the interrupt-service property information
 * from the System Registry. If successful, it installs our primary interrupt service
 * routine (PCIInterruptServiceRoutine)
 */
OSStatus InstallISR(InterruptHandler handler)
{
	OSStatus				osStatus;
	RegPropertyValueSize	propertySize;

	propertySize = sizeof(ISTProperty);

	osStatus = RegistryPropertyGet(&GLOBAL.deviceEntry,
									kISTPropertyName,
									&GLOBAL.theISTProperty,
									&propertySize);
								
	if (osStatus) {
#if 1
		lprintf("RegistryPropertyGet failed\n");
#endif	
		return osStatus;
	}

	GLOBAL.interruptSetMember.setID		= GLOBAL.theISTProperty[kISTChipInterruptSource].setID;
	GLOBAL.interruptSetMember.member	= GLOBAL.theISTProperty[kISTChipInterruptSource].member;

	osStatus = GetInterruptFunctions(GLOBAL.theISTProperty[kISTChipInterruptSource].setID,
							GLOBAL.theISTProperty[kISTChipInterruptSource].member,
							&GLOBAL.oldInterruptSetRefcon,
							&GLOBAL.oldInterruptServiceFunction,
							&GLOBAL.oldInterruptEnableFunction,
							&GLOBAL.oldInterruptDisableFunction);
		
	if (osStatus == noErr)
	{
		osStatus = InstallInterruptFunctions(GLOBAL.theISTProperty[kISTChipInterruptSource].setID, 
							GLOBAL.theISTProperty[kISTChipInterruptSource].member, 
							NULL, 
							handler, 
							NULL, 
							NULL);
		if (GLOBAL.oldInterruptDisableFunction)
			GLOBAL.oldInterruptDisableFunction(GLOBAL.theISTProperty[kISTChipInterruptSource], GLOBAL.oldInterruptSetRefcon);
	}
	
	return osStatus;
}
	
//-----------------------------------------------------------------------------------------
//	Description:
//		This routine uninstalls the interrupt service routine for our card.
//
//	Input:
//		NONE
//
//	Output:
//		returns error if problem occurs during uninstall of isr
//
//-----------------------------------------------------------------------------------------
OSStatus UninstallISR(void)
{
	OSStatus				osStatus;

	if (GLOBAL.oldInterruptDisableFunction)
		GLOBAL.oldInterruptDisableFunction(GLOBAL.interruptSetMember, GLOBAL.oldInterruptSetRefcon);
	
	osStatus = InstallInterruptFunctions(
						GLOBAL.interruptSetMember.setID, 
						GLOBAL.interruptSetMember.member, 
						GLOBAL.oldInterruptSetRefcon, 
						GLOBAL.oldInterruptServiceFunction, 
						NULL, 
						NULL);

	return osStatus;
}



