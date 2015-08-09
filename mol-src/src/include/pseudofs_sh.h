/* 
 *   Creation Date: <2002/10/26 19:49:17 samuel>
 *   Time-stamp: <2002/10/29 01:01:46 samuel>
 *   
 *	<pseudofs_sh.h>
 *	
 *	Pseudo filesystem (used to publish certain linux files)
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_PSEUDOFS_SH
#define _H_PSEUDOFS_SH


static inline _osi_call2( int, OSI_PseudoFS1, OSI_PSEUDO_FS, int, selector, int, p1 );
static inline _osi_call4( int, OSI_PseudoFS3, OSI_PSEUDO_FS, int, selector, int, p1, int, p2, int, p3 );
static inline _osi_call5( int, OSI_PseudoFS4, OSI_PSEUDO_FS, int, selector, int, p1, int, p2, int, p3, int, p4 );

static inline int PseudoFSOpen( const char *s ) {
	return OSI_PseudoFS1( kPseudoFSOpen, (int)s );
}
static inline int PseudoFSClose( int fd ) {
	return OSI_PseudoFS1( kPseudoFSClose, fd );
}
static inline int PseudoFSGetSize( int fd ) {
	return OSI_PseudoFS1( kPseudoFSGetSize, fd );
}
static inline int PseudoFSRead( int fd, int offs, char *buf, int count ) {
	return OSI_PseudoFS4( kPseudoFSRead, fd, offs, (int)buf, count );
}
static inline int PseudoFSIndex2Name( int index, char *buf, int size ) {
	return OSI_PseudoFS3( kPseudoFSIndex2Name, index, (int)buf, size );
}

#endif   /* _H_PSEUDOFS_SH */
