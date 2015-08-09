/* 
 *   Creation Date: <2002/01/20 02:07:25 samuel>
 *   Time-stamp: <2002/01/20 21:37:37 samuel>
 *   
 *	<wrapper.c>
 *	
 *	
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <mach/mach.h>
#include <IOKit/IOKitLib.h>

#include "wrapper.h"

static io_connect_t	mcon;

char *
_map_mregs( void );


void 
wrapper_init( void )
{
	mach_port_t	masterPort;
	io_iterator_t	iter;
	io_service_t	service;

	
	if( IOMasterPort( MACH_PORT_NULL, &masterPort ) ) {
		printm("IOMasterPort error\n");
		exit(1);
	}
	if( IOServiceGetMatchingServices( masterPort, IOServiceMatching("MolExt"), &iter ) ){
		printm("IOServiceGetMatchingServices failed\n");
		exit(1);
	}
	if( !(service = IOIteratorNext( iter )) ){
		printm("The MOL kernel extension was not found\n");
		exit(1);
	}
	IOObjectRelease( iter );
	if( IOServiceOpen( service, mach_task_self(), 1 /*type*/, &mcon ) ){
		printm("Could not connect to the MOL kernel extension\n");
		exit(1);
	}
	printm("Connected to the MOL kernel extension!\n");

#if 0
//	IOConnectMethodScalarIScalarO( mcon, /*index*/0, 0,0 );
//	IOConnectMethodScalarIScalarO( mcon, /*index*/1, 2,0, 2008,-20 );
//	IOConnectMethodScalarIScalarO( mcon, /*index*/2, 1,1, 20,&v );

	v2[0] = 10;
	v2[1] = 20;
	v2[2] = 30;
	v2[3] = 40;
	IOConnectMethodScalarIStructureI( mcon, /*index*/ 3, 2 /*scalar#*/,
					  sizeof(v2),
					  /*scalars*/ 1,2,
					  &v2 );
	size = sizeof(v);
	v = 10;
	IOConnectMethodScalarIStructureO( mcon, /*index*/ 5, 2 /*scalar#*/,
					  &size,
				         /*scalars*/ 1,2, 
					  &v );
	printm("v=%d, size=%d\n", v, size );

	v = 10;
	IOConnectMethodScalarIStructureO( mcon, /*index*/ 6, 2 /*scalar#*/,
					  &size,
				         /*scalars*/ 1,2, 
					  &v );
	printm("v=%d, size=%d\n", v, size );
#endif
	printm(" get_mol_mod_version: %08lX\n", _get_mol_mod_version() );
	g_session_id = 0;
	_kernel_init(0x1234567);
	printm("Before\n" );
	{
		mac_regs_t *mr = _map_mregs();
		printm("Mregs = %08lX\n", mr );
		printm("GPR[0] = %08lX\n", mr->gpr[0] );
		_kernel_cleanup();
		exit(1);
	}
}

void
wrapper_cleanup( void )
{
	IOObjectRelease( mcon );
}


/************************************************************************/
/*	Wrapper functions						*/
/************************************************************************/

ulong
_get_mol_mod_version( void )
{
	ulong ver;
	return IOConnectMethodScalarIScalarO( mcon, /*index*/ 0, 0, 1, &ver ) ? 0 : ver;
}

int
_kernel_init( ulong session_magic ) 
{
	return IOConnectMethodScalarIScalarO( mcon, /*index*/ 1, 2, 0, g_session_id, session_magic );
	
}

void
_kernel_cleanup( void ) 
{
	IOConnectMethodScalarIScalarO( mcon, /*index*/ 2, 0, 0 );
}


char *
_map_mregs( void )
{
	char *p;
	return IOConnectMethodScalarIScalarO( mcon, /*index*/ 3, 0,1, &p ) ? NULL : p;
}
