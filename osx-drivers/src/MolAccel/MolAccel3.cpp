/* 
 *   Creation Date: <2002/09/04 23:03:30 samuel>
 *   Time-stamp: <2004/03/06 16:55:34 samuel>
 *   
 *	<MolAccel3.cpp>
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

#define MOL_ACCEL_CLASS_NAME	MolAccel3
#include "MolAccel.h"

#include "mol_config.h"
#include "osi_calls.h"

extern "C" { 
	// 10.1
	extern char bcopy_phys[], sync_cache[], phys_copy[];
	extern char pmap_zero_page[], pmap_copy_page[];
	/* hw_cpv undefined */

	// 10.2 only
	extern char bcopy_physvir[];

	// 10.3 only
	extern char ml_set_physical[], ml_restore[];
}


#define super MolAccel
OSDefineMetaClassAndStructors( MolAccel3, MolAccel );


bool
MolAccel3::start( IOService *provider )
{
	if( !super::start( provider ) )
		return false;
	
	printm("MOL acceleration for 10.3\n");

	replaceFunction( (UInt32*)bcopy_phys, "bcopy_phys_103" );
	replaceFunction( (UInt32*)bcopy_physvir, "bcopy_physvir_103" );
	replaceFunction( (UInt32*)phys_copy, "phys_copy_103" );
	replaceFunction( (UInt32*)sync_cache, "sync_cache" );

	//replaceFunction( (UInt32*)hw_cpv, "hw_cpv" );
	replaceFunction( (UInt32*)pmap_zero_page, "pmap_zero_page_103" );
	replaceFunction( (UInt32*)pmap_copy_page, "pmap_copy_page_103" );

	/* 10.3 only */
	replaceFunction( (UInt32*)ml_set_physical, "ml_set_physical" );
	//replaceFunction( (UInt32*)ml_restore, "ml_restore" );

	return true;
}
