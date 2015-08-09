#ifndef __VideoDriverPrototypes_H__
#define __VideoDriverPrototypes_H__

#include <PCI.h>

/*
 * The Driver Manager calls DoDriverIO to perform I/O.
 */
#pragma internal off

OSStatus
DoDriverIO(	AddressSpaceID			addressSpaceID,
		IOCommandID			ioCommandID,
		IOCommandContents		ioCommandContents,
		IOCommandCode			ioCommandCode,
		IOCommandKind			ioCommandKind);

#pragma internal on

/*
 * Prototypes for the specific driver handlers. These do real work.
 */
OSStatus
DriverInitializeCmd(	AddressSpaceID			addressSpaceID,
			DriverInitInfoPtr		driverInitInfoPtr);

OSStatus
DriverFinalizeCmd(	DriverFinalInfoPtr		driverFinalInfoPtr);

OSStatus
DriverSupersededCmd(	DriverSupersededInfoPtr		driverSupersededInfoPtr,
			Boolean				calledFromFinalize);
			
OSStatus
DriverReplaceCmd(	AddressSpaceID			addressSpaceID,
			DriverReplaceInfoPtr		driverReplaceInfoPtr);
			
OSStatus
DriverOpenCmd(		AddressSpaceID			addressSpaceID,
			ParmBlkPtr			pb);
			
OSStatus
DriverCloseCmd(		ParmBlkPtr			pb);

OSStatus
DriverControlCmd(	AddressSpaceID			addressSpaceID,
			IOCommandID			ioCommandID,
			IOCommandKind			ioCommandKind,
			CntrlParam			*pb);
			
OSStatus
DriverStatusCmd(	IOCommandID			ioCommandID,
			IOCommandKind			ioCommandKind,
			CntrlParam			*pb);
			
OSStatus
DriverKillIOCmd(	ParmBlkPtr			pb);

OSStatus
DriverReadCmd(
			AddressSpaceID			addressSpaceID,
			IOCommandID			ioCommandID,
			IOCommandKind			ioCommandKind,
			ParmBlkPtr			pb);
			
OSStatus
DriverWriteCmd(		AddressSpaceID			addressSpaceID,
			IOCommandID			ioCommandID,
			IOCommandKind			ioCommandKind,
			ParmBlkPtr			pb);

/*	.___________________________________________________________________________________.
  	| Driver Gestalt handler -- called from the PBStatus handler.						|
	.___________________________________________________________________________________.
 */
OSStatus
DriverGestaltHandler(	CntrlParam*			pb);

/*	.___________________________________________________________________________________.
  	| Primary and Secondary Interrupt Service Routines									|
	.___________________________________________________________________________________.
 */
 
#pragma internal off

InterruptMemberNumber
PCIInterruptServiceRoutine(	InterruptSetMember		member,
				void				*refCon,
				UInt32				theIntCount);


#pragma internal on

/*	.___________________________________________________________________________________.
  	| Driver initialization functions. These call the Name Registry utilities			|
	| Some of the NVRAM functions may be called from a Status or Control call handler.	|
	.___________________________________________________________________________________.
 */

OSStatus
InstallISR(InterruptHandler handler);

OSStatus
UninstallISR(void);

void
PublishDriverDebugInfo(void);


/* text into the name registry	*/
void
PublishInitFailureMsg(		OSStatus			errorStatus,
				ConstStr255Param		messageText);

/* From initialization only	*/			
OSStatus
RetrieveDriverNVRAMProperty( 	RegEntryIDPtr			regEntryIDPtr,		/* Driver's Name Registery ID	*/
				OSType				driverCreatorID,	/* Registered creator code		*/
				UInt8				driverNVRAMRecord[8]);	/* Result returned here			*/

/* Called at any time			*/
OSStatus
GetDriverNVRAMProperty(		
				RegEntryIDPtr			regEntryIDPtr,		/* Driver's Name Registery ID	*/
				OSType				driverCreatorID,	/* Registered creator code		*/
				UInt8				driverNVRAMRecord[8]);	/* Result returned here			*/

/* Called at any time			*/
OSStatus
UpdateDriverNVRAMProperty(	RegEntryIDPtr			regEntryIDPtr,			/* Driver's Name Registery ID	*/
				OSType				driverCreatorID,		/* Registered creator code		*/
				UInt8				driverNVRAMRecord[8]);
				
/* Called from initialization	*/
OSStatus
CreateDriverNVRAMProperty(	RegEntryIDPtr			regEntryIDPtr,		/* Driver's Name Registery ID	*/
				OSType				driverCreatorID,	/* Registered creator code		*/
				UInt8				driverNVRAMRecord[8]);	/* Data to store in registry	*/

/*	.___________________________________________________________________________________.
  	| These functions can only be called from driver initialization.					|
	.___________________________________________________________________________________.
 */
OSStatus
GetDeviceLogicalAddress(	RegEntryIDPtr			regEntryIDPtr,		/* Driver's Name Registery ID	*/
				PCIRegisterNumber		deviceRegister,		/* Register in address property	*/
				LogicalAddress			*deviceLogicalAddress,	/* Gets the logical address		*/
				ByteCount			*deviceAreaLength);	/* Gets the area size			*/

OSStatus
GetThisProperty(	RegEntryIDPtr				regEntryIDPtr,			/* Driver's Name Registery ID	*/
			RegPropertyNamePtr			regPropertyName,
			RegPropertyValue			**regPropertyValuePtr,
			RegPropertyValueSize			*regPropertyValueSizePtr);
			
void
DisposeThisProperty(	RegPropertyValue			*regPropertyValuePtr);

OSStatus
InitPCIMemorySpace(	RegEntryIDPtr				regEntryIDPtr);



/*	.___________________________________________________________________________________.
  	| Utitlity function to clear a block of memory.										|
	.___________________________________________________________________________________.
 */
#ifndef CLEAR
#define CLEAR(what)	BlockZero((char*)&what, sizeof what)
#endif

/*
 * This uses the ANSI-C string concatenate and "stringize" operations.
 */
#define Trace(what)

#if 0
static void
CheckStatus(	OSStatus	value,
		char*		message)
{}
#endif

#endif