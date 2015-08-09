/* 
 *   Creation Date: <2002/09/04 23:03:41 samuel>
 *   Time-stamp: <2003/08/24 16:23:25 samuel>
 *   
 *	<MolAccel2.h>
 *	
 *	
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_MOLACCEL2
#define _H_MOLACCEL2

#include "MolAccel.h"



class MolAccel2 : public MolAccel {
	OSDeclareDefaultStructors( MolAccel2 );
	
 public:
	virtual bool		start( IOService *provider );
};

#endif   /* _H_MOLACCEL2 */
