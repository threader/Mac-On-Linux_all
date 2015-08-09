/* 
 *   Creation Date: <2004/01/23 19:21:29 samuel>
 *   Time-stamp: <2004/06/12 21:48:07 samuel>
 *   
 *	<misc.c>
 *	
 *	Signal handling, backtrace
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


/************************************************************************/
/*	signal handlers							*/
/************************************************************************/

#ifdef __ppc__
static void __dbg
backtrace( int nip )
{
	const char *name = get_filename_res("molsyms");
	ulong oldsp, sp, base;
	
	asm volatile("mr %0,1" : "=r" (sp) : );

	if( nip ) {
		aprint("   NIP ");
		print_btrace_sym( nip, name );
	}
	base = sp;
	printm("***** Backtrace *****\n");
	do {
		printm( "   %08lx: ", sp );
		print_btrace_sym( *((ulong*)sp+1), name );
		oldsp = sp;
		sp = *(ulong*)sp;
	} while( sp > oldsp && sp < base + 0x10000 );
}

#ifdef UCCONTEXT_HAS_GREGS
#define get_nip(puc)	(((struct pt_regs*)puc->uc_mcontext.gregs)->nip)
#else
#define get_nip(puc)	(puc->uc_mcontext.regs->nip)
#endif

#else
static void __dbg
backtrace( int nip )
{
	printf("*** backtrace unimplemented ***\n");
}
#define get_nip(puc)	0
#endif

void 
signal_handler( int sig_num, siginfo_t *sinfo, struct ucontext *puc, ulong rt_sf )
{
	/* handles SIGINT, SIGPIPE */
	if( common_signal_handler(sig_num) )
		return;
	
	switch( sig_num ) {
	case SIGHUP:
		aprint("HUP: _pid %d, _uid %d\n", sinfo->si_pid, sinfo->si_uid );
		// if( !is_main_thread() )
		//	return;
		break;
	case SIGTRAP: 
	case SIGSEGV:
	case SIGILL:
	case SIGBUS: {
		aprint("   si_signo = %d, si_errno %d, si_code %08X, si_addr %p\n", 
		       sinfo->si_signo, sinfo->si_errno, sinfo->si_code, sinfo->si_addr );
		aprint("   Last RVEC: 0x%lx (%ld), last OSI: %ld, mac_nip %08lX\n", 
		       mregs->dbg_last_rvec, (mregs->dbg_last_rvec & RVEC_MASK), mregs->dbg_last_osi,
		       mregs->nip );
		backtrace( get_nip(puc) );

		if( sig_num == SIGTRAP )
			return;
		break;
	}
	default:
		break;
	}
	exit(1);
}



/************************************************************************/
/*	kernel module loading						*/
/************************************************************************/

void
load_mods( void )
{
	char *s, buf[80], script[256];
	int i, sheep, tun, allow_mismatch;

	allow_mismatch = (get_bool_res("allow_kver_mismatch") == 1);

	sheep = tun = 0;
	for( i=0 ; get_str_res_ind("netdev",i,0) ; i++ ) {
		if( !(s=get_str_res_ind("netdev", i, 1)) )
			continue;
		sheep |= !!strstr( s, "sheep" );
		tun |= !!strstr( s, "tun" );
	}
	sprintf( buf, "%d %s %s", allow_mismatch,
		 sheep ? "sheep" : "", tun? "tun" : "" );
#ifndef __MPC107__
	strncpy0( script, get_libdir(), sizeof(script) );
	strncat0( script, "/bin/modload", sizeof(script) );
	if( script_exec(script, get_libdir(), buf) )
		exit(1);	
	if( get_bool_res("load_only") == 1 )
		exit(0);
#endif
}
