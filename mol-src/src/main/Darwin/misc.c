/* 
 *   Creation Date: <2004/01/23 19:22:49 samuel>
 *   Time-stamp: <2004/02/04 21:35:30 samuel>
 *   
 *	<osx.c>
 *	
 *	
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
#include <signal.h>
#include <ucontext.h>
#include "res_manager.h"
#include "thread.h"
#include "mac_registers.h"
#include "rvec.h"
#include "misc.h"

static void __dbg
backtrace( void )
{
	const char *s = get_filename_res("molsyms");
	ulong oldsp, sp, base;
	
	asm volatile("mr %0,r1" : "=r" (sp) : );
	base = sp;
	printm("***** Backtrace *****\n");
	do {
		printm( "   %08lx: ", sp );
		print_btrace_sym( *((ulong*)sp+2), s );
		oldsp = sp;
		sp = *(ulong*)sp;
	} while( sp > oldsp && sp < base + 0x10000 );
}

void 
signal_handler( int sig_num )
{
	if( common_signal_handler(sig_num) )
		return;	

	switch( sig_num ) {
	case SIGTRAP: 
	case SIGSEGV:
	case SIGILL:
	case SIGBUS:
		backtrace();
		if( sig_num == SIGTRAP )
			return;
		break;
	default:
		break;
	}
	exit(1);
}

void
load_mods( void )
{
	/* nothing yet */
}
