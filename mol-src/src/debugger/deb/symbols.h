/* 
 *   Creation Date: <97/06/30 15:20:13 samuel>
 *   Time-stamp: <2003/03/28 22:58:34 samuel>
 *   
 *	<symbols.h>
 *	
 *	Symbol <-> address translation interface
 *   
 *   Copyright (C) 1997-2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _SYMBOLS_H
#define _SYMBOLS_H

extern void	symbols_init( void );
extern void	symbols_cleanup( void );

extern char 	*symbol_from_addr( ulong addr );
extern ulong	addr_from_symbol( char *symstr );

#endif
