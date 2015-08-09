/* 
 *   Creation Date: <2002/10/25 21:23:02 samuel>
 *   Time-stamp: <2004/03/27 01:54:02 samuel>
 *   
 *	<boothelper_sh.h>
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

#ifndef _H_BOOTHELPER_SH
#define _H_BOOTHELPER_SH

typedef struct {
	int	offs;			/* offset to data */
	int	width;
	int	height;
	int	depth;
} bootimage_t;

#ifndef MOL_PROPER


/************************************************************************/
/*	Boot Helper							*/
/************************************************************************/

static inline _osi_call3( int, OSI_BootHelper, OSI_BOOT_HELPER, 
			  int, what, int, param1, int, param2 );
static inline _osi_call4( int, OSI_BootHelper3, OSI_BOOT_HELPER, 
			  int, what, int, param1, int, param2, int, param3 );
static inline _osi_call5( int, OSI_BootHelper4, OSI_BOOT_HELPER, 
			  int, what, int, param1, int, param2, int, param3, int, param4 );
static inline _osi_call6( int, OSI_BootHelper5, OSI_BOOT_HELPER, 
			  int, what, int, param1, int, param2, int, param3, int, param4, int, param5 );

static inline int BootHAscii2Unicode( ushort *ustr, const char *s, int maxlen ) {
	return OSI_BootHelper3( kBootHAscii2Unicode, (int)ustr, (int)s, maxlen );
}
static inline int BootHUnicode2Ascii( char *d, const ushort *ustr, int uni_strlen, int maxlen ) {
	return OSI_BootHelper4( kBootHUnicode2Ascii, (int)d, (int)ustr, uni_strlen, maxlen );
}
static inline int BootHResPresent( const char *s ) {
	return OSI_BootHelper5( kBootHGetStrResInd, (int)s, (int)1, 0, 0, 0 );
}
static inline char *BootHGetStrRes( const char *s, char *buf, size_t len ) {
	return (char*)OSI_BootHelper5( kBootHGetStrResInd, (int)s, (int)buf, len, 0, 0 );
}
static inline char *BootHGetStrResInd( const char *s, char *buf, size_t len, int index, int argnum ) {
	return (char*)OSI_BootHelper5( kBootHGetStrResInd, (int)s, (int)buf, len, index, argnum );
}
static inline int BootHGetRamSize( void ) {
	return OSI_BootHelper( kBootHGetRAMSize, 0, 0 );
}

#endif /* MOL_PROPER */

#endif   /* _H_BOOTHELPER_SH */






