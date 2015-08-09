/* 
 *   Creation Date: <1999/03/29 13:32:19 samuel>
 *   Time-stamp: <2004/03/28 01:26:20 samuel>
 *   
 *	<res_manager.h>
 *	
 *	Resource manager (also handles cmdline options)
 *   
 *   Copyright (C) 1999, 2000, 2001, 2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_OPTIONS
#define _H_OPTIONS

typedef struct {
	const char 	*name;
	ulong		flag;
} opt_entry_t;

extern int		g_session_id;

/* res manager interface */
extern void 		res_manager_init( int is_mol, int argc, char **argv );
extern void		res_manager_cleanup( void );

extern void		default_res( const char *key, const char *value );
extern char		*add_res( const char *key, const char *value );
extern int		res_present( const char *key );

extern int 		get_bool_res( const char *key );
extern long		get_numeric_res( const char *key );
extern char		*get_lockfile( void );

extern ulong		parse_res_options( const char *key, int index, int first_arg, 
					   opt_entry_t *opts, const char *err_str );

extern char		*get_str_res( const char *key );
extern char		*get_str_res_ind( const char *key, int index, int argnum );
#define			get_filename_res	get_str_res
#define			get_filename_res_ind	get_str_res_ind

extern void		missing_config_exit( const char *s );
extern void		missing_config( const char *s );

/* directory/filename functions */
extern char		*fix_filename( char *raw_filename, char *buf, size_t bufsize );
extern char    		*get_libdir( void );
extern char		*get_datadir( void );
extern char    		*get_vardir( void );
extern char    		*get_bindir( void );
extern char		*get_molrc_name( void );

#endif   /* _H_OPTIONS */

