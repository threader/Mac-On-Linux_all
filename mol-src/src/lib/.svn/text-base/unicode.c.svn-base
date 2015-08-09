/* 
 *   Creation Date: <2002/10/22 23:11:18 samuel>
 *   Time-stamp: <2004/01/18 01:15:52 samuel>
 *   
 *	<unicode.c>
 *	
 *	Unicode support
 *   
 *   Copyright (C) 2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *
 *   Based upon code written by
 *
 *   Copyright (C) 1999-2000  Brad Boyer (flar@pants.nu)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

/* ISO-8859-1 -> unicode */
int
asc2uni( unsigned char *ustr, const char *astr, int maxlen )
{
	int len;
	
	if( maxlen <= 0 )
		return 0;

	for( len=0; *astr && len < maxlen-1 ; astr++, len++ ) {
		*ustr++ = 0;
		*ustr++ = *astr;
	}
	ustr[0] = ustr[1] = 0;
	return len;
}

/* unicode -> ISO-8859-1 */
int
uni2asc( char *astr, const unsigned char *ustr, int ustrlen, int maxlen )
{
	int len;
	
	if( maxlen <= 0 )
		return 0;
	
	for( len=0; ustrlen-- > 0 && len < maxlen-1 ; ustr += 2 ) {
		/* might be unrepresentable (or too complicated to represent) */
		if( ustr[0] || !ustr[1] )
			continue;
		*astr++ = ustr[1];
		len++;
	}
	*astr = 0;
	return len;
}
