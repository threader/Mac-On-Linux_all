/* 
 *   Creation Date: <2001/06/11 22:41:04 samuel>
 *   Time-stamp: <2003/08/20 11:41:22 samuel>
 *   
 *	<selftest.c>
 *	
 *	This piece of code is run inside MOL. That is, it is
 *	not linked against glibc.
 *   
 *   Copyright (C) 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <stdarg.h>
#include "osi_calls.h"
#include "processor.h"

extern void	performance( void );
extern int	entry( void );
extern void	perf_print( char *str, ulong count, int attempt_num );
extern int	test_opcode( int opcode );
extern int	set_usermode( int usermode );

/************************************************************************/
/*	F U N C T I O N S						*/
/************************************************************************/

static int	mticks;

static void
print_ranges(char res[0x400] ) 
{
	int i, st, j;

	st = 0xfff;
	printm("\n    ");
	for( j=0, i=0; i < 0x400; i++ ) {
		if( st < i ) {
			if( !res[i] )
				continue;
			if( st == i-1 )
				printm("%d", st );
			else
				printm("%d-%d", st, i-1 );

			if( !(++j%5) )
				printm(",\n    ");
			else
				printm(", ");
			st = 0xfff;
		}  else {
			st = !res[i] ? i : 0xfff;
			if( i == 0x3ff && !res[i] )
				printm("%d", i );
		}
	}
	if( st < i )
		printm("%d-%d", st, i );
	printm("\n");
}

static void
opcode_testing( void )
{
	char res[0x400];
	int i, j, opcode, srr1;

	for( j=0; j<4; j++ ) {
		set_usermode(!!j);
		for( i=0; i < 0x400; i++ ){
			opcode = (31 << (32-6)) | (339 << 1);
			opcode |= SPRNUM_FLIP(i) << 11;
			srr1 = test_opcode(opcode);
			if( j< 2 )
				res[i] = !!srr1;
			else if( j==2 ) {
				res[i] = !(srr1 & 0x00080000);
			} else
				res[i] = !(srr1 & 0x00040000);
		}
		if( !j )
			printm("-- SUPERVISOR SPRs -------------------------" );
		else if( j==1 )
			printm("-- USER SPRs -------------------------------" );
		else if( j==2 )
			printm("-- ILLEGAL SPRs ----------------------------" );
		else
			printm("-- PRIVILEGED SPRs -------------------------" );
		print_ranges( res );
		printm("\n");
	}
	set_usermode(0);
}

int
entry( void )
{
	printm(" *******************************************\n"
	       " *             Testing performance         *\n"
	       " *******************************************\n\n");
	mticks = OSI_UsecsToMticks(1000000);

	opcode_testing();
	performance();

	printm("\n *******************************************\n"
	         " *          Self-test successful           *\n"
	         " *******************************************\n\n");
	return 0;
}

void
perf_print( char *str, ulong elapsed, int attempt )
{
	static uint old_attempt, result, calibration;
	ulong v, vv;
	char *p;

	if( elapsed < result || old_attempt < attempt )
		result = elapsed;
	old_attempt = attempt;
	
	if( attempt )
		return;
	
	if( !calibration )
		calibration = result;

	v = (ulong)(327.68/20 * (double)mticks / (double)(result) + 0.5);
	vv = 10000.0 * (double)calibration / (double)result + 0.5;
	for( p=str; *p; p++ )
		if( *p == '\t' )
			*p = ' ';

	printm(" %-19s %4ld.%02ld MHz,  %3ld.%02ld %%\n", str, 
	       v/1000, (v/10) % 100,
	       vv/100, vv%100 );
}
