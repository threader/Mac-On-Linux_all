/* 
 *   Creation Date: <2000/12/28 21:23:03 samuel>
 *   Time-stamp: <2004/02/28 15:07:25 samuel>
 *   
 *	<booter.c>
 *	
 *	Booter glue
 *   
 *   Copyright (C) 2000, 2001, 2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/param.h>
#include "booter.h"
#include "res_manager.h"
#include "debugger.h"
#include "os_interface.h"
#include "boothelper_sh.h"
#include "memory.h"
#include "loader.h"

/* globals */
platform_expert_ops_t 	gPE;
int			_boot_flags;

static struct {
	char		*key;
	int		flag;
	void		(*startup)( void );
} btable[] = {
	{ "newworld",	kBootNewworld,		of_startup },
	{ "osx",	kBootOSX,		osx_startup },
	{ "elf",	kBootElf,		elf_startup },
	{ "linux",	kBootElf | kBootLinux,	of_startup },
#ifdef OLDWORLD_SUPPORT
	{ "oldworld",	kBootOldworld,		oldworld_startup },
#endif
	{ NULL, 0, NULL }
};


/************************************************************************/
/*     	boot loader support functions					*/
/************************************************************************/

static int
osip_boot_helper( int sel, int *args )
{
	char *ustr, *s, *p, *buf;
	
	switch( args[0] ) {
	case kBootHAscii2Unicode: /* unicode_dest, src, maxlen -- uni_strlen */
		ustr = transl_mphys( args[1] );
		s = transl_mphys( args[2] );
		if( !ustr || !s )
			return -1;
		return asc2uni( (unsigned char *)ustr, s, args[3] );

	case kBootHUnicode2Ascii: /* dest, uni_str, uni_strlen, maxlen -- strlen */
		s = transl_mphys( args[1] );
		ustr = transl_mphys( args[2] );
		if( !ustr || !s )
			return -1;
		return uni2asc( s, (unsigned char *)ustr, args[3], args[4] );

	case kBootHGetStrResInd: /* resname, buf, len, index, argnum */
		p = transl_mphys( args[1] );
		buf = transl_mphys( args[2] );
		if( !p || !(s=get_str_res_ind(p,args[4],args[5])) )
			return 0;
		if( args[2] && buf )
			strncpy0( buf, s, args[3] );
		return args[2];

	case kBootHGetRAMSize:
		return ram.size;
	}
	return -1;
}


/************************************************************************/
/*	boot method detection						*/
/************************************************************************/

void
booter_startup( void )
{
	if( gPE.booter_startup )
		gPE.booter_startup();
}

void 
determine_boot_method( void )
{
	char *s = get_str_res("boot_method");
	int i=0;
	
	if( !s )
		missing_config_exit("boot_method");

	while( btable[i].key && strcmp(s, btable[i].key) )
		i++;
	if( !btable[i].key ) {
		printm("--->Bad 'boot_method' value\n");
		exit(1);
	}
	_boot_flags = btable[i].flag;
}

void 
booter_init( void )
{
	char *s = get_str_res("boot_method");
	int i;
	memset( &gPE, 0, sizeof(gPE) );

	for( i=0; strcmp(s, btable[i].key); i++ )
		;
	gPE.booter_startup = btable[i].startup;

	gPE.nvram_image_name = get_filename_res("nvram");
	gPE.oftree_filename = get_filename_res("oftree");

	os_interface_add_proc( OSI_BOOT_HELPER, osip_boot_helper );
}

void 
booter_cleanup( void )
{
	if( gPE.booter_cleanup )
		gPE.booter_cleanup();
}
