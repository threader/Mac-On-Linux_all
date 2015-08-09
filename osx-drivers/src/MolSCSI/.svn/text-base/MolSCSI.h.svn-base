/* 
 *   Creation Date: <2003/07/08 22:34:39 samuel>
 *   Time-stamp: <2003/07/12 20:14:49 samuel>
 *   
 *	<MolSCSI.h>
 *	
 *	
 *   
 *   Copyright (C) 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#include <IOKit/IOMemoryCursor.h>
#include <IOKit/pci/IOPCIDevice.h>
#include <libkern/OSByteOrder.h>
#include "scsi_sh.h"

#include <IOKit/scsi/IOSCSIParallelInterface.h>

typedef struct {
	int	dummy;
} MolCmdData;

struct priv_req;

class MolSCSI : public IOSCSIParallelController
{
	OSDeclareDefaultStructors( MolSCSI )

 private:
	IOService		*provider;
	IOInterruptEventSource 	*_irq;
	IOMemoryDescriptor	*memoryDesc;

	// overflow lists
	int			_olists_in_use;
	sglist_t		*_olist;
	int			_is_disabled;

	void			releaseOLists( scsi_req_t *r );
	

	void			interrupt( IOInterruptEventSource *sender, int count );
	int			prepareDataXfer( IOSCSIParallelCommand *cmd, scsi_req_t *r );

 public:
	bool			configure( IOService *provider, SCSIControllerInfo *controllerInfo );
	void			executeCommand( IOSCSIParallelCommand *cmd );
	void			cancelCommand( IOSCSIParallelCommand *cmd );
	void			resetCommand( IOSCSIParallelCommand *cmd );

};
