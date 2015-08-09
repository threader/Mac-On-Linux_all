/* 
 *   Creation Date: <1999/03/20 07:37:20 samuel>
 *   Time-stamp: <1999/03/20 07:38:31 samuel>
 *   
 *	<pci_mol.h>
 *	
 *	Version & PCI definitions
 *   
 *   Copyright (C) 1999 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef __PCI_MOL__
#define __PCI_MOL__

/* Some fields in the DriverDescription table */
#define kVersionMajor		1
#define kVersionMinor		0
#define kVersionStage		final		/* Res only			*/
#define kVersionStageName	"fc"		/* "d", "a", "b", "" for final	*/
#define kVersionStageValue	finalStage	/* Released			*/
#define kVersionRevision	0

/* Driver name */
#define kDriverNamePString "\p.MolScsiSIM"
#define kDriverNameCString ".MolScsiSIM"

/*
 * PCI configuration definitions - I got these by installing the board and dumping
 * the property list. They differ slightly from the printed documentation. Only the
 * device name and kPCIRevisionID are used.
 */
#define kPCIVendorID			0x1000		/* 00 15- 0	Registered vendor ID	*/
#define kPCIClassCode			0x00010000	/* 08 31- 8	Registered class code	*/
#define kPCIRevisionID			0x02		/* 08  7- 0	Registered revision ID	*/
#define kPCIRegisterSize		0x100		/* Size of the I/O and/or Mem area	*/
/*
 * These are offsets within the Configuration Register area that Open Firmware uses
 * to determine the register mapping. The driver can use the I/O and/or memory base
 * registers. Expansion rom is not used, but could be used by a utility that stores
 * a driver into the NCR flash rom.
 */
#define kPCIIOBaseRegister		0x10	/* I/O base address in Config Reg's	*/
#define kPCIMemoryBaseRegister		0x14	/* Memory base address in Config Reg's	*/
#define kPCIExpansionROMRegister	0x30	/* Expansion rom base in Config Reg's	*/

#define kPCIDeviceNamePString		"\ppci1000,3"
#define kPCIDeviceNameCString		"pci1000,3"
#define kPCIDeviceID			0x0003
#define kPCIChipRevision		0

#endif
