/* 
 *   Creation Date: <2002/09/04 23:03:30 samuel>
 *   Time-stamp: <2004/03/04 20:31:30 samuel>
 *   
 *	<MolAccel2.cpp>
 *	
 *	
 *   
 *   Copyright (C) 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#define MOL_ACCEL_CLASS_NAME	MolAccel2
#include "MolAccel.h"

#include "mol_config.h"
#include "osi_calls.h"

extern "C" { 
	// 10.1
	extern char bcopy_phys[], sync_cache[], phys_copy[], hw_cpv[];
	extern char pmap_zero_page[], pmap_copy_page[];
	// 10.2 only
	extern char bcopy_physvir[];
}

#define super MolAccel
OSDefineMetaClassAndStructors( MolAccel2, MolAccel );


bool
MolAccel2::start( IOService *provider )
{
	if( !super::start( provider ) )
		return false;
	
	/* 10.1 and 10.2 */
	replaceFunction( (UInt32*)bcopy_phys, "bcopy_phys" );
	replaceFunction( (UInt32*)sync_cache, "sync_cache" );
	replaceFunction( (UInt32*)phys_copy, "phys_copy" );
	replaceFunction( (UInt32*)hw_cpv, "hw_cpv" );
	replaceFunction( (UInt32*)pmap_zero_page, "pmap_zero_page" );
	replaceFunction( (UInt32*)pmap_copy_page, "pmap_copy_page" );

	/* 10.2 only */
	replaceFunction( (UInt32*)bcopy_physvir, "bcopy_physvir" );

	printm("MOL acceleration for 10.2\n");
	return true;
}
