#ifndef __VideoDriverPrivate_H__
#define __VideoDriverPrivate_H__

#pragma internal off

#include <VideoServices.h>
#include <Video.h>
#include <Displays.h>
#include <DriverGestalt.h>
#include <DriverServices.h>
#include <PCI.h>

#include "IRQUtils.h"
#include "video_sh.h"

#pragma internal on

#ifndef FALSE
#define TRUE	1
#define FALSE	0
#endif

#define MOL_PCI_VIDEO_VENDOR_ID		0x6666
#define MOL_PCI_VIDEO_DEVICE_ID		0x6666
#define MOL_PCI_VIDEO_NAME		"\pMacOnLinuxVideo"

#define MOL_PCI_VIDEO_BASE_REG		0x10

#define kDriverGlobalsPropertyName	"GLOBALS"
#define kDriverFailTextPropertyName	"FAILURE"
#define kDriverFailCodePropertyName	"FAIL-CODE"

/*
 * Our global storage is defined by this structure. This is not a requirement of the
 * driver environment, but it collects globals into a coherent structure for debugging.
 */
struct DriverGlobal {
	DriverRefNum		refNum;			/* Driver refNum for PB... */
	RegEntryID		deviceEntry;		/* Name Registry Entry ID */
	LogicalAddress		boardFBAddress;
	ByteCount		boardFBMappedSize;

	Boolean			irqInstalled;
	volatile Boolean	inInterrupt;

	/* Interrupt Set globals. */
	IRQInfo			irqInfo;

	/* Common globals */
	UInt32			openCount;
	
	/* Frame buffer configuration */
	Boolean			qdInterruptsEnable;	/* Enable VBLs for qd */
	Boolean			qdDeskServiceCreated;
	Boolean			qdLuminanceMapping;

	InterruptServiceIDType	qdVBLInterrupt;

	Boolean			isOpen;
	osi_get_vmode_info_t	vmode;

	/* "Hardware" cursor */
	int			useHWCursor;
	int			csCursorX, csCursorY;
	int			csCursorVisible;
};
typedef struct DriverGlobal DriverGlobal, *DriverGlobalPtr;

/*
 * Globals and functions
 */
extern DriverGlobal		gDriverGlobal;		/* All interesting globals */
#define GLOBAL			(gDriverGlobal)		/* GLOBAL.field for references */
extern DriverDescription	TheDriverDescription;	/* Exported to the universe */

#define VMODE			(gDriverGlobal.vmode)	/* for references */

#define FB_START(vmode)		((char*)GLOBAL.boardFBAddress + (vmode)->offset)
#define CHECK_OPEN( error )	if( !GLOBAL.isOpen ) return (error)

#if 0
extern UInt32			gPageSize;		/* GetLogicalPageSize() */
extern UInt32			gPageMask;		/* gPageSize - 1 */
#endif

#endif
