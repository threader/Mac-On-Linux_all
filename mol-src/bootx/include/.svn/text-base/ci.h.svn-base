/* 
 *   Creation Date: <2002/07/19 15:17:55 samuel>
 *   Time-stamp: <2003/07/30 14:48:17 samuel>
 *   
 *	<ci.h>
 *	
 *	Client Interface
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *
 */

#ifndef _H_CI
#define _H_CI

#include "libc.h"

typedef enum { kGetRootPhandle=0 } mol_phandle_t;	/* must promote to int */

extern int		CloseProm( void );

extern mol_phandle_t	Peer( mol_phandle_t phandle );
extern mol_phandle_t	Child( mol_phandle_t phandle );
extern mol_phandle_t	Parent( mol_phandle_t phandle );
extern int		PackageToPath( mol_phandle_t phandle, char *buf, long buflen );
extern int		GetPropLen( mol_phandle_t phandle, char *name );
extern int		GetProp( mol_phandle_t phandle, char *name, char *buf, long buflen );
extern int		NextProp( mol_phandle_t phandle, char *prev, char *buf );
extern int		SetProp( mol_phandle_t phandle, char *name, char *buf, long buflen );

extern mol_phandle_t	CreateNode( char *path );
extern mol_phandle_t	FindDevice( char *path );

/* File Interface */
extern int		Open( char *path );
extern void		Close( int ihandle );
extern void		Read( int ihandle, char * addr, long length );
extern void		Seek( int ihandle, long long position );

extern int		IsCD( int ihandle );

/* Pseudo FS */
struct pseudo_fd;
typedef struct pseudo_fd pseudo_fd_t;

extern pseudo_fd_t 	*pseudo_open( const char *path );
extern void		pseudo_close( pseudo_fd_t *p );
extern int		pseudo_read( pseudo_fd_t *p, char *buf, int count );
extern int		pseudo_lseek( pseudo_fd_t *p, off_t offs, int whence );
extern int		pseudo_load_file( const char *path, char *buf, int maxsize );

#define SEEK_SET	0
#define SEEK_CUR	1
#define SEEK_END	2

/* BootX specific stuff */

typedef long		CICell;
#define kCINoError	0
#define kCIError	-1

/* exports from driver.c */
extern long AddDriverMKext( void );

#define DBG		printm

#endif   /* _H_CI */
