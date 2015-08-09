/* 
 *   Creation Date: <2002/01/09 22:42:56 samuel>
 *   Time-stamp: <2004/03/13 13:17:37 samuel>
 *   
 *	<MolExt.h>
 *	
 *	
 *   
 *   Copyright (C) 2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_MOLEXT
#define _H_MOLEXT

#include <IOKit/IOService.h>


class MolExt : public IOService {

	OSDeclareDefaultStructors(MolExt)

public:
	virtual bool		start( IOService *provider );
	virtual void		stop( IOService *provider );
};


#endif   /* _H_MOLEXT */
