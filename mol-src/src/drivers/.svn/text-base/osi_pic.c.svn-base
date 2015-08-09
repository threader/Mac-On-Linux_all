/* 
 *   Creation Date: <2002/05/07 00:03:48 samuel>
 *   Time-stamp: <2003/12/15 22:18:27 samuel>
 *   
 *	<osi_pic.c>
 *	
 *	OSI Interrupt Controller (for Linux booting)
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <pthread.h>
#include "promif.h"
#include "pic.h"
#include "mac_registers.h"
#include "molcpu.h"
#include "driver_mgr.h"
#include "os_interface.h"
#include "hostirq.h"

static struct {
	pthread_mutex_t lock;

	int		level;
	int		mask;

	int		cpu_irq;
} ps;

#define LOCK		pthread_mutex_lock( &ps.lock )
#define UNLOCK		pthread_mutex_unlock( &ps.lock )

/************************************************************************/
/*	F U N C T I O N S						*/
/************************************************************************/

static void
source_hi( int irq )
{
	int b = (1<<irq);

	LOCK;
	ps.level |= b;
	
	if( (ps.level & ps.mask) )
		set_cpu_irq( 1, &ps.cpu_irq );
	UNLOCK;
}

static int
source_low( int irq )
{
	int old, b = (1<<irq);
	
	LOCK;
	old = ps.level & b;
	ps.level &= ~b;
	if( !(ps.level & ps.mask) )
		set_cpu_irq( 0, &ps.cpu_irq );
	UNLOCK;

	return old;
}

/************************************************************************/
/*	OSI interface							*/
/************************************************************************/

static int
osip_pic_unmask_irq( int sel, int *params )
{
	ulong b = (1UL << params[0]);
	//printm("osip_pic_unmask_irq %d\n", params[0] );

	LOCK;
	ps.mask |= b;

	if( (ps.level & ps.mask) )
		set_cpu_irq_mt( 1, &ps.cpu_irq );
	UNLOCK;
	return 0;
}

static int
osip_pic_mask_irq( int sel, int *params )
{
	ulong b = (1UL << params[0]);	
	//printm("osip_pic_mask_irq %d\n", params[0] );

	LOCK;
	ps.mask &= ~b;

	if( !(ps.level & ps.mask) )
		set_cpu_irq_mt( 0, &ps.cpu_irq );
	UNLOCK;
	return 0;
}

static int
osip_pic_get_active_irq( int sel, int *params )
{
	int active;

	hostirq_check_irqs();

	LOCK;
	active = ps.level & ps.mask;
	UNLOCK;
	return active;
}

static const osi_iface_t osi_iface[] = {
	{ OSI_PIC_MASK_IRQ, 		osip_pic_mask_irq	},
	{ OSI_PIC_UNMASK_IRQ,		osip_pic_unmask_irq	},
	{ OSI_PIC_GET_ACTIVE_IRQ,	osip_pic_get_active_irq	},
};


/************************************************************************/
/*	Initalize / Cleanup						*/
/************************************************************************/

static irq_controller_t osi_pic = {
	.irq_hi		= source_hi,
	.irq_low	= source_low,
	.register_irq	= NULL
};

static int
osi_pic_init( void )
{
	if( !prom_find_devices("osi-pic") )
		return 0;

	pthread_mutex_init( &ps.lock, NULL );

	g_irq_controller = osi_pic;

	register_osi_iface( osi_iface, sizeof(osi_iface) );
	return 1;
}

static void
osi_pic_cleanup( void )
{
	pthread_mutex_destroy( &ps.lock );
}

driver_interface_t osi_pic_driver = {
	"osi_pic", osi_pic_init, osi_pic_cleanup
};

