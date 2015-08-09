/**************************************************************
*   
*   Creation Date: <1999-02-01 05:41:03 samuel>
*   Time-stamp: <2003/08/09 23:30:06 samuel>
*   
*	<thread.h>
*	
*	Thread manager
*   
*   Copyright (C) 1999, 2000, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
*   
*   This program is free software; you can redistribute it and/or
*   modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation
*   
**************************************************************/

#ifndef _H_THREAD
#define _H_THREAD

#include <pthread.h>

#if 1
/* Not defined in <ptherad.h> any more? */
extern int		pthread_mutexattr_setkind_np( pthread_mutexattr_t *attr, int kind );
#endif

extern void		threadpool_init( void );
extern void		threadpool_cleanup( void );

extern void		create_thread( void (*entry)(void*), void *data, const char *thread_name  );

extern const char 	*get_thread_name( void );

extern pthread_t		__main_th;
#define is_main_thread()	( pthread_self() == __main_th )
#define get_main_th()		( __main_th )

#endif   /* _H_THREAD */
