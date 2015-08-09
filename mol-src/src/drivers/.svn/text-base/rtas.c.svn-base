/* 
 *   Creation Date: <1999/11/03 01:50:43 samuel>
 *   Time-stamp: <2004/02/07 18:32:18 samuel>
 *   
 *	<rtas.c>
 *	
 *	Run Time Abstraction Services (RTAS)
 *   
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "promif.h"
#include "debugger.h"
#include "memory.h"
#include "os_interface.h"
#include "nvram.h"
#include "byteorder.h"
#include "booter.h"
#include "driver_mgr.h"
#include "../drivers/include/pci.h"

typedef struct rtas_args {
	ulong	token;
        int 	nargs;
        int 	nret;
        ulong 	args[10];	/* MAX NUM ARGS! */
} rtas_args_t;

static ulong	s_rtas_token;
static int	s_rtas_nargs, s_rtas_nret;


/************************************************************************/
/*	RTAS cmds							*/
/************************************************************************/

static int 
rc_nop( ulong args[], ulong ret[] ) 
{
	int i;
	printm(">>> Unimplemented RTAS '%08lx' (%d/%d)\n", s_rtas_token, s_rtas_nargs, s_rtas_nret );
	for( i=0; i<s_rtas_nargs; i++ )
		printm("  %08lX ", args[i] );
	printm("\n");
	return 0;	/* -1 might be better... */
}

/* -- status */
static int 
rc_restart_rtas( ulong args[], ulong ret[] ) 
{
	printm("******** restart-rtas ********\n");
	return 0;
}

/* offs buf length -- status actlen */
static int 
rc_nvram_fetch( ulong args[], ulong ret[] ) 
{
	/* printm("RTAS nvram-fetch %04lx %08lX %ld\n", args[0], args[1], args[2] );  */

	ret[1] = nvram_read( args[0], (char*)args[1], args[2] );
	ret[0] = 0;
	if( ret[1] != args[2] ){
		printm("---> nvram_read parameter error\n");
		ret[0] = -3;	/* parameter error */
	}
	/* printm("RTAS nvram-fetch %08lX [len=%d] %02X\n", args[0], args[2], 
		 *(char*)args[1] ); */
	return 0;
}

/* offs buf length -- status actlen */
static int 
rc_nvram_store( ulong args[], ulong ret[] ) 
{
	/* printm("nvram-store %04lx %08lX %ld\n", args[0], args[1], args[2] ); */

	ret[1] = nvram_write( args[0], (char*)args[1], args[2] );
	ret[0] = 0;
	if( ret[1] != args[2] ){
		printm("---> nvram_store parameter error\n");
		ret[0] = -3;	/* parameter error */
	}
	return 0;
}

/* config_addr size -- status value */
static int 
rc_read_pcicfg( ulong args[], ulong ret[] ) 
{
	pci_addr_t addr;
	ulong v;

	/* XXX: don't know how to handle different PCI domains... */

	/* 0,bus,devfn,reg */
	addr = PCIADDR_FROM_BUS_DEVFN( 0, (args[0]>>16)&0xff, (args[0]>>8)&0xff );
	v = read_pci_config( addr, args[0]&0xff, args[1] );
	if( args[1] == 2 )
		v = ld_le32(&v) >> 16;
	if( args[1] == 4 )
		v = ld_le32(&v);
	ret[1] = v;
	
	/* printm("RTAS: read_pci_config (%ld:%02lX) + 0x%02lx [%ld]  :  %08lX\n", 
		   (args[0]>>16)&0xff, (args[0]>>8)&0xff, args[0]&0xff, args[1], ret[1]  );*/
	ret[0] = 0;
	return 0;
}

/* config_addr size value -- status */
static int
rc_write_pcicfg( ulong args[], ulong ret[] ) 
{
	pci_addr_t addr;

	/* printm("RTAS: write_pci_config (%ld:%02lX) + 0x%02lx %08lX [%ld]\n", 
		   (args[0]>>16)&0xff, (args[0]>>8)&0xff, args[0]&0xff, args[2], args[1] ); */
	if( args[1] == 2 )
		args[2] = ld_le32( &args[2] ) >> 16;
	if( args[1] == 4 )
		args[2] = ld_le32( &args[2] );

	/* XXX: don't know how to handle different PCI domains... */

	addr = PCIADDR_FROM_BUS_DEVFN( 0, (args[0]>>16)&0xff, (args[0]>>8)&0xff );
	write_pci_config( addr, args[0]&0xff, args[2], args[1] );
	ret[0] = 0;
	return 0;
}


/************************************************************************/
/*	RTAS interface							*/
/************************************************************************/

#define A0	0x1
#define A1	0x2
#define A2	0x4
#define A3	0x8

struct {
	ulong	token;
	int	(*proc)( ulong args[], ulong ret[] );
	int	nargs, nret, translate_args;
} rtab[] = {
 /* --------------------------------------------------------------------------- */
 /* restart-rtas */		{0xABCDEF01,	rc_restart_rtas,0,1, 0  },	/* -- status */
 /* nvram-fetch */		{0xABCDEF02,	rc_nvram_fetch, 3,2, A1 },	/* offs buf length -- status actlen */
 /* nvram-store */		{0xABCDEF03,	rc_nvram_store, 3,2, A1 },	/* offs buf length -- status actlen */
 /* nvram-fetch (???)		{0xABCDEF02,	rc_nvram_fetch, 7,3, ?  },	*/
 /* nvram-store (???)		{0xABCDEF03,	rc_nvram_store, 7,3, ?  },	*/
 /* get-time-of-day */		{0xABCDEF04,	rc_nop,		0,8, 0  },	/* -- status y m d hour min sec ns */
 /* set-time-of-day */		{0xABCDEF05,	rc_nop,		7,1, 0  },	/* y m d hour min sec ns -- status */
 /* set-time-for-power-on */	{0xABCDEF06,	rc_nop,		7,1, 0  },	/* y m d hour min sec ns -- status */
 /* event-scan */		{0xABCDEF07,	rc_nop,		4,1, 0  },	/* eventmask critical buffer len -- status */
 /* check-exception */		{0xABCDEF08,	rc_nop,		6,1, 0  },	/* vector_offs info evmask critical buf len -- status */
 /* read-pci-config */		{0xABCDEF09,	rc_read_pcicfg,	2,2, 0  },	/* config_addr size -- status value */
 /* write-pci-config */		{0xABCDEF0A,	rc_write_pcicfg,3,1, 0  },	/* config_addr size value -- status */
 /* display-character		{0xABCDEF0B, 	rc_xxx,		1,1, 0  },	   char -- status */
 /* set-indicator */		{0xABCDEF0C,	rc_nop,		3,1, 0  },	/* token index state -- status */
 /* get-sensor-state		{0xABCDEF0D,	rc_xxx,		2,2, 0  },	   token index -- status state */
 /* set-power-level		{0xABCDEF0E,	rc_xxx,		2,2, 0  },	*/
 /* get-power-level		{0xABCDEF0F,	rc_xxx,		1,2, 0  },	*/
 /* assume-power-management	{0xABCDEF10,	rc_xxx,		0,1, 0  },	   -- success */
 /* relinquish-power-management {0xABCDEF11,	rc_xxx,		0,1, 0  },	   -- success */
 /* power-off */		{0xABCDEF12,	rc_nop,		2,1, 0  },	/* power_on_mask_hi power_on_mask_lo -- status */
 /* suspend			{0xABCDEF13,	rc_xxx,		3,2, 0  },	*/
 /* hibernate		 	{0xABCDEF14,	rc_xxx,		3,1, 0  },	*/
 /* system-reboot */		{0xABCDEF15,	rc_nop,		0,1, 0  },	/* -- status */
 /* -------- SMP -------------------------------------------------------------- */
 /* cache-control 	 	{0xABCDEF??,	rc_xxx,		2,1, 0  },	*/
 /* freeze-time-base 		{0xABCDEF??,	rc_xxx,		?,?, 0  },	*/
 /* thaw-time-base		{0xABCDEF??,	rc_xxx,		?,?, 0  },	*/
 /* stop-self			{0xABCDEF??,	rc_xxx,		?,?, 0  },	*/
 /* start-cpu			{0xABCDEF??,	rc_xxx,		?,?, 0  },	*/
 /* ?				{0xABCDEF??,	rc_xxx,		?,?, 0  },	*/
 /* --------------------------------------------------------------------------- */
 /* get-time-for-power-on */	{0xABCDEF1E,	rc_nop,		1,1, 0  },
				{0,		0,		0,0, 0  }
};

/* RTAS argument buffer: function-token, num_iargs, num_oargs, first iarg ... first oarg */
static int
osip_of_rtas( int sel, int *args )
{
	ulong largs[10];
	rtas_args_t *pb;
	int i,j;

	if( !(pb=transl_mphys( args[0] )) ) {
		printm("RTAS parameter block not in RAM!\n");
		return 0;
	}
	/* printm("RTAS call, token: %08lX\n", pb->token ); */

	for( i=0; rtab[i].token; i++ ) {
		if( rtab[i].token != pb->token || rtab[i].nargs != pb->nargs || rtab[i].nret != pb->nret )
			continue;
		break;
	}
	if( !rtab[i].token ) {
		printm("+ Missing RTAS function %08lx (%d/%d)\n", 
		       pb->token, pb->nargs, pb->nret );
		return -1;
	}
	for( j=0; j<rtab[i].nargs; j++ ) {
		largs[j] = pb->args[j];
		if( rtab[i].translate_args & (1<<j) && pb->args[j] ) {
			if( !(largs[j]=(ulong)transl_mphys( largs[j] )) ) {
				printm("RTAS pointer not in RAM\n");
				return -1;
			}
		}
	}
	s_rtas_token = pb->token;
	s_rtas_nargs = pb->nargs;
	s_rtas_nret = pb->nret;
	return (*rtab[i].proc)( largs, &pb->args[pb->nargs] );
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static int
rtas_init( void )
{
	if( !prom_find_devices("rtas") )
		return 0;
		
	os_interface_add_proc( OSI_OF_RTAS, osip_of_rtas );
	return 1;
}

driver_interface_t rtas_driver = {
	"rtas", rtas_init, NULL
};
