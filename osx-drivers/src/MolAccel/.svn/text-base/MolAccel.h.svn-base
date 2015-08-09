/* 
 *   Creation Date: <2002/08/07 22:03:17 samuel>
 *   Time-stamp: <2003/08/24 16:23:59 samuel>
 *   
 *	<MolAccel.h>
 *	
 *	MOL acceleration
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_MOLACCEL
#define _H_MOLACCEL

#include <IOKit/IOService.h>

class MolAccel : public IOService {
	OSDeclareDefaultStructors( MolAccel );

 private:
	void 			(*write_hook)( UInt32 *fentry, UInt32 addr );
	UInt32			getFuncAddr( char *fname );
 protected:
	void			replaceFunction( UInt32 *fentry, char *fname );
 public:
	virtual bool		start( IOService *provider );
};



#ifdef MOL_ACCEL_CLASS_NAME

class MOL_ACCEL_CLASS_NAME : public MolAccel {
	OSDeclareDefaultStructors( MOL_ACCEL_CLASS_NAME );
 public:
	virtual bool		start( IOService *provider );	
};

#endif /* MOL_ACCEL_CLASS_NAME */



#endif   /* _H_MOLACCEL */
