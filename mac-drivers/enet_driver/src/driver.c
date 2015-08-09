/* 
 *   Creation Date: <2002/11/23 17:53:53 samuel>
 *   Time-stamp: <2002/12/07 17:12:01 samuel>
 *   
 *	<enet.c>
 *	
 *	MOL ethernet driver, based upon 
 *
 *	   Simple loopback driver utilizing Mentat DLPI Template Code (dlpiether.c)
 *	   This file, combined with dlpiether.c (or linked against the Shared LIbrary
 *	   containing dlpiether.c) is a functioning loopback driver for Open Transport.
 *	   This file also serves as a template for writing the hardware-specific
 *	   code for a driver which will use dlpiether.c.  Comments in the code and
 *	   the accompanying documentation, Mentat DLPI Driver Template, provide
 *	   necessary information.
 *	   loopback.c 1.2 Last Changed 29 Mar 1996
 *
 *	   Copyright: (C) 1996 by Mentat Inc.
 *
 *	MOL adaption by BenH:
 *
 *	Copyright (C) 1999 Benjamin Herrenschmidt (bh40@calva.net)
 *
 *	Copyright (C) 2002 by Samuel Rydh
 */

#include "enet.h"
#include "logger.h"
#include "LinuxOSI.h"
#include "misc.h"

/* entrypoints */
extern Boolean		InitStreamModule( void *systemDependent );
extern void		TerminateStreamModule(void);
extern OTResult		ValidateHardware( RegEntryID *our_id );
extern install_info	*GetOTInstallInfo(void);

board_t			globals;

/************************************************************************/
/*	DriverDescription						*/
/************************************************************************/

DriverDescription TheDriverDescription = {
	kTheDescriptionSignature,				/* Signature field of this structure */
	kVersionOneDriverDescriptor,				/* Version of this data structure */
	{
		"\p"kMOL_BoardName,				/* Driver Name/Info String */
		kVMajor,					/* 1st part of version number in BCD */
		kVMinor,					/* 2nd & 3rd part share a byte */
		kVStage,					/* stage: dev, alpha, beta, or final */
		kVStage						/* non-released revision level */
	},
	{							/* OS Runtime Requirements of Driver */
		kDriverIsUnderExpertControl,			/* Options for OS Runtime */
		"\p"kEnetName					/* Driver's name to the OS */
	},
	{							/* Apple Service API Membership */
		1,						/* Number of Services Supported */
		{						/* The List of Services */
			kServiceCategoryOpenTransport,		/* Service Category Name */
			OTPCIServiceType(
				kOTEthernetDevice,		/* Ethernet Device */
				kOTFramingEthernet		/* Ethernet Packets */
				  | kOTFramingEthernetIPX	/* IPX Packets */
				  | kOTFraming8022,		/* 802.2 Packets, */
				false,				/* it is not TPI */
				true ),				/* it is DLPI */
			kVMajor,				/* 1st part of version number in BCD */
			kVMinor,				/* 2nd & 3rd part share a byte */
			kVStage,				/* stage: dev, alpha, beta,  or final */
			kVRelease				/* non-released revision level */
		}
	}
};


/************************************************************************/
/*	module and STREAMS information					*/
/************************************************************************/

/* the_module_info provides name and operation parameters.
 * See mistream.h for more info.
 */
static struct module_info the_module_info = {
	kEnetModuleID,
	kMOL_BoardName,
	MIN_PACKET_SIZE,		/* min pkt size, for developer use */
	MAX_PACKET_SIZE,		/* max pkt size, for developer use */
	LOOP_HIWAT,			/* hi-water mark, for flow control */
	LOOP_LOWAT			/* lo-water mark, for flow control */
};

/* read_qinit provides read queue information for the stream.
 * See mistream.h for more info.
 */
static struct qinit read_qinit = {
	NULL,				/* put procedure */
	mol_enet_rsrv,			/* service procedure */
	mol_enet_open,			/* called on each open or a push */
	mol_enet_close,			/* called on last close or a pop */
	NULL,				/* reserved for future use */
	&the_module_info,		/* information structure */
	NULL				/* statistics structure - optional */
};

/* write_qinit provides write queue information for the stream.
 * See mistream.h for more info.
 */
static struct qinit write_qinit = {
	mol_enet_wput,			/* put procedure */
	NULL,				/* service procedure */
	mol_enet_open,			/* open specified in read queue */
	mol_enet_close,			/* close specified in read queue */
	NULL,				/* reserved for future use */
	&the_module_info,		/* information structure */
	NULL				/* statistics structure - optional */
};

/* the_streamtab provides stream information to Open Transport.
 * See mistream.h for more info.
 */
static struct streamtab the_streamtab = {
	&read_qinit,			/* defines read QUEUE */
	&write_qinit,			/* defines write QUEUE */
	NULL,				/* for multiplexing drivers only */
	NULL				/* for multiplexing drivers only */
};

/* the_install_info provides driver information to Open Transport.
 * See OpenTptModule.h for more info.
 * Also see OpenTptPCISupport.h for more info.
 */
static install_info the_install_info = {
	&the_streamtab,			/* Streamtab pointer. */
	kOTModIsDriver |		/* yes, this is a driver :) */
		kOTModUpperIsDLPI |	/* this driver provides DLPI */
		kOTModUsesInterrupts,	/* interrupts are used */
	SQLVL_MODULE,			/* Synchronization level. */
	NULL,				/* Shared writer list buddy */
	0,				/* ref_load, Set to 0 */
	0				/* ref_count, Set to 0 */
};

/* GetOTInstallInfo() is an entry point function that returns a pointer to
 * the drivers install_info structure. See OpenTptModule.h for more info.
 */
install_info *
GetOTInstallInfo( void )
{
	return &the_install_info;
}



/************************************************************************/
/*	entrypoints 							*/
/************************************************************************/

/* InitStreamModule() is an entry point used by Open Transport.
 * It will be called whenever this module is about to be loaded
 * into a stream for the first time, or if it is about to be
 * reloaded after having been unloaded.
 * Returns false if this module should NOT be loaded.
 * The void* parameter can be interpreted as a RegEntryIDPtr.
 * See OpenTptModule.h for more info.
 */
Boolean
InitStreamModule( void *systemDependent )
{
	RegEntryIDPtr theID = (RegEntryIDPtr)systemDependent;
	UInt8 *phys_address = NULL;
	dle_t *dle;
	
	/* These are the hardware start/stop/reset functions */
	bzero( (char *)&globals, sizeof(globals) );
	dle = &globals.board_dle;
	dle->dle_hw.dlehw_start = mol_enet_hw_start;
	dle->dle_hw.dlehw_stop = mol_enet_hw_stop;
	dle->dle_hw.dlehw_address_filter_reset = mol_enet_hw_address_filter_reset;
	dle->dle_hw.dlehw_send_error = NULL;
	dle->dle_hw.dlehw_recv_error_flags = 0;

	/*
	 * Suggestions:
	 * - Install interrupt vectors in case any memory allocations are
	 *	required.  Interrupts should not be enabled until the
	 *	board start routine is called.
	 * - Reset the hardware to a known state.  If the hardware does
	 *	not respond, then return false.
	 * - Read the Ethernet address from the board's ROM area.  This
	 *	address should be copied into both the dle_factory_addr
	 *	field and the dle_current_addr field.
	 */

	globals.xmt_buf = MemAllocatePhysicallyContiguous( MAX_PACKET_SIZE, true );
	if( !globals.xmt_buf ) {
		lprintf("mol_enet:MemAllocatePhysicallyContiguous failed\n");
		goto fail;
	}
	globals.rx_buf = MemAllocatePhysicallyContiguous( MAX_PACKET_SIZE, true );
	if( !globals.rx_buf ) {
		lprintf("mol_enet:MemAllocatePhysicallyContiguous failed\n");
		goto fail;
	}

	globals.multi_addrs = MemAllocatePhysicallyContiguous( 
					MAX_MULTICAST * sizeof(multi_addr_t), true );
	if( !globals.multi_addrs ) {
		lprintf("mol_enet:MemAllocatePhysicallyContiguous failed\n");
		goto fail;
	}
	globals.multi_count = 0;

	/* Copy the registry entry */
	(void)RegistryEntryIDInit( &globals.name_reg_entry );
	(void)RegistryEntryIDCopy( theID, &globals.name_reg_entry );

	if( InstallIRQ( &globals.name_reg_entry, mol_enet_irq, &globals.irqInfo ) != noErr ) {
		lprintf("mol_enet: Can't install interrupt\n");
		goto fail;
	}
	
	globals.osi_id = OSI_EnetOpen( GetOSIID(&globals.irqInfo) );
	if( globals.osi_id < 0) {
		lprintf("mol_enet:OSI_EnetOpen failed\n");
		goto fail;
	}
	OSI_EnetControl1( globals.osi_id, kEnetRouteIRQ, globals.irqInfo.aapl_int );

	phys_address = MemAllocatePhysicallyContiguous(6, true);
	if( !phys_address ) {
		lprintf("mol_enet:MemAllocatePhysicallyContiguous failed\n");
		goto fail;
	}
	
	if( OSI_EnetGetEthAddress(globals.osi_id, GetPhysicalAddr(phys_address)) ) {
		lprintf("mol_enet:OSI_EnetGetEthAddress failed\n");
		goto fail;
	}

	bcopy( phys_address, dle->dle_factory_addr, 6 );
	bcopy( phys_address, dle->dle_current_addr, 6 );

	MemDeallocatePhysicallyContiguous( phys_address );
	phys_address = NULL;

	/*
	 * Allow the DLPI common code to initialize the dle fields. Once
	 * dle_init is called, dle_terminate must be called before freeing
	 * the dle structure.  There is private memory allocated for each
	 * dle that needs to be freed.
	 */
	dle_init( dle, 0 );
	return true;

fail:
	if( globals.xmt_buf )
		MemDeallocatePhysicallyContiguous( globals.xmt_buf );
	if( globals.rx_buf )
		MemDeallocatePhysicallyContiguous( globals.rx_buf );
	if( globals.multi_addrs )
		MemDeallocatePhysicallyContiguous( globals.multi_addrs );
	if( phys_address )
		MemDeallocatePhysicallyContiguous( phys_address );
		
	return false;
}

void
TerminateStreamModule( void )
{
	/*
	 * Suggestions:
	 * - Remove interrupt vectors.
	 * - Reset the hardware to a known state.
	 */

	mol_enet_hw_stop( NULL );
	
	if( globals.osi_id ) {
		OSI_EnetClose( globals.osi_id );
		globals.osi_id = 0;
	}	

	if( globals.xmt_buf )
		MemDeallocatePhysicallyContiguous( globals.xmt_buf );
	globals.xmt_buf = NULL;
	if( globals.rx_buf )
		MemDeallocatePhysicallyContiguous( globals.rx_buf );
	globals.rx_buf = NULL;
	if( globals.multi_addrs )
		MemDeallocatePhysicallyContiguous( globals.multi_addrs );
	globals.multi_addrs = NULL;

	mol_enet_hw_stop( NULL );		/* extra call to make sure the IRQ is disabled */
	UninstallIRQ( &globals.irqInfo );

	dle_terminate( &globals.board_dle );
	/* XXX: clear globals */
}

OTResult
ValidateHardware( RegEntryID *our_id )
{
	return kOTPCINoErrorStayLoaded;		/* kOTNoError */
}
