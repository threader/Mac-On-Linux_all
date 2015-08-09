/* 
 *   Creation Date: <1999/10/31 18:16:04 samuel>
 *   Time-stamp: <2004/02/28 15:06:56 samuel>
 *   
 *	<booter.h>
 *	
 *	World interface to NewWorld booting replacement
 *   
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_BOOTER
#define _H_BOOTER

extern void 	booter_init( void );
extern void 	booter_cleanup( void );
extern void 	booter_startup( void );

extern void 	oldworld_startup( void );
extern void 	elf_startup( void );
extern void 	of_startup( void );
extern void 	osx_startup( void );

typedef struct {
	/* booter */
	void	(*booter_startup)( void );
	void	(*booter_cleanup)( void );
	
	/* mouse / keyboard interface */
	int	(*adb_key_event)( unsigned char key, int raw );
	void	(*mouse_event)( int dx, int dy, int buttons );

	/* config */
	char	*nvram_image_name;
	char	*oftree_filename;
} platform_expert_ops_t;

extern platform_expert_ops_t gPE;

static inline int PE_adb_key_event( unsigned char key, int raw )
	{ return gPE.adb_key_event ? gPE.adb_key_event(key, raw) : -1; }
static inline void PE_mouse_event( int dx, int dy, int buttons ) 
	{ if( gPE.mouse_event ) gPE.mouse_event(dx,dy,buttons); }

/* boot type */
#define kBootNewworld		1
#define kBootOldworld		2
#define kBootElf		4
#define kBootLinux		8		// kBotElf also set
#define kBootOSX		16

/* boot method queries */
extern void			determine_boot_method( void );
extern int			_boot_flags;

#define is_newworld_boot()	( _boot_flags & kBootNewworld )
#ifdef OLDWORLD_SUPPORT
#define is_oldworld_boot()	( _boot_flags & kBootOldworld )
#else
#define is_oldworld_boot()	0
#endif
#define is_classic_boot()	( _boot_flags & (kBootNewworld | kBootOldworld) )
#define is_macos_boot()		( _boot_flags & (kBootOldworld | kBootNewworld | kBootOSX) )

#define is_osx_boot()		( _boot_flags & kBootOSX )
#define is_elf_boot()		( _boot_flags & kBootElf )
#define is_linux_boot()		( _boot_flags & kBootLinux )


#endif   /* _H_BOOTER */
