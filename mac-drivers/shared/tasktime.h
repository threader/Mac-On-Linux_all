/* 
 *   Creation Date: <2002/12/28 15:29:10 samuel>
 *   Time-stamp: <2002/12/28 16:36:53 samuel>
 *   
 *	<tasktime.h>
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

#ifndef _H_TASKTIME
#define _H_TASKTIME

typedef void	(*DNeedTimeFunc)( int param1, int param2 );

extern void	DNeedTime( DNeedTimeFunc func, int param1, int param2 );
extern void	DNeedTime_Init( void );


#endif   /* _H_TASKTIME */
