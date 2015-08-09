/* 
 *   Creation Date: <2002/12/08 00:06:15 samuel>
 *   Time-stamp: <2003/04/29 23:13:35 samuel>
 *   
 *	<irq_test.c>
 *	
 *	IRQ testing/debug
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "driver_mgr.h"
#include "pci.h"
#include "osi_driver.h"
#include "pic.h"
#include "os_interface.h"
#include "mac_registers.h"


enum { kIRQTestGetID=0, kIRQTestAck0, kIRQTestAck1, kIRQTestRouteIRQ0, kIRQTestRouteIRQ1,
       kIRQTestRaise0, kIRQTestRaise1 };

static int irq0=-1, irq1=-1;
static osi_driver_t *driver0;

static int
osip_irqtest( int sel, int *params )
{
	int ret;
	
	switch( params[0] ) {
	case kIRQTestGetID:
		return get_osi_driver_from_id(params[1]) != driver0;
	case kIRQTestAck0:
		ret = irq_line_low( irq0 );
		printm("ack irq 0 - %x\n", ret);
		return ret;
	case kIRQTestAck1:
		ret = irq_line_low( irq1 );
		printm("ack irq 1 - %x\n", ret );
		return ret;
	case kIRQTestRouteIRQ0:
		oldworld_route_irq( params[1], &irq0, "irq-test" );
		break;
	case kIRQTestRouteIRQ1:
		oldworld_route_irq( params[1], &irq1, "irq-test" );
		break;
	case kIRQTestRaise0:
		printm("raising irq 0\n");
		irq_line_hi( irq0 );
		break;
	case kIRQTestRaise1:
		//printm("raising irq 1\n" );
		irq_line_hi( irq1 );
		printm("MSR_EE = %d, irq = %d\n", (mregs->msr & MSR_EE)?1:0, mregs->irq );
		break;
	}
	return 0;
}


static int
cmd_irqtest0( int argc, char **argv )
{
	irq_line_hi( irq0 );
	return 0;
}
static int
cmd_irqtest1( int argc, char **argv )
{
	irq_line_hi( irq1 );
	return 0;
}

static int
irq_test_init( void )
{
	static pci_dev_info_t pci_config = { 0x1000, 0x0003, 0x02, 0x0, 0x0100 };

	return 0;
	
	os_interface_add_proc( OSI_IRQTEST, osip_irqtest );

	if( !(driver0=register_osi_driver( "irqtest", "irq-test-0", &pci_config, &irq0 )) ) {
		printm("Failed to register irq-test-0\n");
	}

	if( !register_osi_driver( "irqtest", "irq-test-1", &pci_config, &irq1 ) ) {
		printm("Failed to register irq-test-1\n");
	}
	/* irq_line_hi( irq1 ); */
	/* irq_line_hi( irq0 ); */

	add_cmd("irqtest0", "irqtest\n", -1, cmd_irqtest0 );
	add_cmd("irqtest1", "irqtest\n", -1, cmd_irqtest1 );

	return 1;
}

static void
irq_test_cleanup( void )
{
}

driver_interface_t irq_test_driver = {
    "irq_test", irq_test_init, irq_test_cleanup
};
