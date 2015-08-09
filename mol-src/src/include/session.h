/* 
 *   Creation Date: <2000/08/26 16:23:33 samuel>
 *   Time-stamp: <2001/03/10 23:44:17 samuel>
 *   
 *	<session.h>
 *	
 *	Support of fast save/restore session
 *   
 *   Copyright (C) 2000 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_SESSION
#define _H_SESSION

enum { kStaticChunk=0, kDynamicChunk };		/* for add_session_save_proc */

typedef int (*session_save_fp)( void );
typedef int (*session_prepare_fp)( void );

extern void	session_init( void );
extern void	session_cleanup( void );

extern int	save_session_now( void );
extern void	schedule_save_session( int usecs );

extern void	session_save_proc( session_save_fp, session_prepare_fp, int dynamic_data );

extern int	read_session_data( char *type, int id, char *ptr, size_t size );
extern void	align_session_data( int boundary );
extern int	write_session_data( char *type, int id, char *ptr, ssize_t size );
extern int	get_session_data_size( char *type, int id );
extern int	get_session_data_fd( char *type, int id, int *fd, off_t *offs, ssize_t *size );

extern ulong	calc_str_checksum( char *str );
extern ulong	calc_checksum( char *ptr, int size );

extern int	loading_session( void );
extern void	session_is_loaded( void );

extern void	session_failure( char *err ) __attribute__ ((__noreturn__));

#endif   /* _H_SESSION */


