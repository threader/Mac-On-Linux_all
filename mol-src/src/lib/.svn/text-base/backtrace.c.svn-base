/* 
 *   Creation Date: <2004/01/23 19:33:34 samuel>
 *   Time-stamp: <2004/01/24 20:43:29 samuel>
 *   
 *	<backtrace.c>
 *	
 *	Backtrace support
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#include "mol_config.h"
#include "extralib.h"


void
print_btrace_sym( ulong addr, const char *sym_filename )
{
	char ch, buf[80];
	ulong v, v2=0;
	int found=0;
	FILE *f=NULL;

	if( sym_filename )
		f = fopen(sym_filename, "ro");

	buf[0]=0;
	while( f && fscanf(f, "%lx %c ", &v, &ch) == 2 ) {
		if( v <= addr ) {
			v2 = v;
			if(fscanf( f, "%79s\n", buf ) != 1) {
				printm("Error reading from backtrace symbol\n");
				fclose(f);
				return;
			}
			continue;
		}
		if( buf[0] ) {
			printm("%s + 0x%lx\n", buf, addr - v2 );
			found=1;
		}
		break;
	}
	if( !found )
		printm("0x%08lx\n", addr );
	if( f )
		fclose(f);
}
