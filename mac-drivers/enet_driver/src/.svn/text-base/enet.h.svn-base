/* 
 *   Creation Date: <2002/11/23 18:57:07 samuel>
 *   Time-stamp: <2002/11/23 19:37:35 samuel>
 *   
 *	<enet.h>
 *	
 *	MOL enet globals
 *
 *	Based upon EnetSample.h, Sample ENET Driver v1.0.0
 *	(C) 1998 by Apple Computer, Inc., all rights reserved.
 *
 *   Adapted for MOL by BenH:
 *
 *   Copyright (C) 1999 Benjamin Herrenschmidt (bh40@calva.net)
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 */

#ifndef _H_ENET
#define _H_ENET

#include <OpenTptModule.h>
#include <dlpi.h>
#include <NameRegistry.h>
#include <OpenTptLinks.h>
#include <OpenTptPCISupport.h>
#include <Devices.h>
#include <DriverServices.h>
#include <interrupts.h>

typedef UInt32	ulong;
typedef UInt32	Count;

#include "IRQUtils.h"

#include <dlpi.h>
#include "dlpiether.h"
#include "dlpiuser.h"

#include "osi_enet.h"

#define	MAX_PACKET_SIZE	1514		/* We're ethernet, remember :-) */
#define	MIN_PACKET_SIZE	0		/* We'll pad short packets Ethernet style */
#define LOOP_HIWAT	2048		/* Flow control high water mark */
#define LOOP_LOWAT	64		/* Flow control low water mark */

#define kVString	MASTER_ENET_VERSION
#define kVMajor		2
#define kVMinor		1
#define kVStage		8
#define kVRelease	0

#define MAX_MULTICAST	16


typedef struct multi_addr_s {
	UInt8		address[6];
} multi_addr_t;

/* driver information 'per board', e.g., internal control structure. */
typedef struct board_s {
	/* must be first in structure */
	dle_t		board_dle;
	
	/* various infos */
	RegEntryID	name_reg_entry;
	UInt32		osi_id;
	unsigned char	*xmt_buf;
	unsigned char	*rx_buf;
	multi_addr_t	*multi_addrs;
	UInt32		multi_count;
	
	IRQInfo		irqInfo;
} board_t;

extern board_t		globals;

/* STREAMS instance data (q_ptr) */
typedef struct mol_enet_s {
	/* Must be first in structure */
	dcl_t		mol_enet_dcl;		
	
	/* any 'per stream' fields needed by board go here */
} mol_enet_t;


/* pointer to board_t structure */
#define	mol_enet_dle	mol_enet_dcl.dcl_hw


extern InterruptMemberNumber mol_enet_irq( InterruptSetMember ISTmember, void *refCon, UInt32 theIntCount );
extern OTInt32	mol_enet_close( queue_t *q, OTInt32 flag, cred_t *credp );
extern void	mol_enet_hw_start( void *boardvp );
extern void	mol_enet_hw_stop( void *boardvp );
extern OTInt32	mol_enet_open( queue_t *q, dev_t *devp, OTInt32 flag, OTInt32 sflag, cred_t *credp );
extern long	mol_enet_rsrv( queue_t *q );
extern OTInt32	mol_enet_wput( queue_t *q, mblk_t *mp );
extern void	mol_enet_hw_address_filter_reset( void *boardvp, dle_addr_t *addr_list, ulong addr_cnt,
						  ulong promisc_cnt, ulong multi_promisc_cnt,
						  ulong accept_broadcast, ulong accept_error );

#endif   /* _H_ENET */
