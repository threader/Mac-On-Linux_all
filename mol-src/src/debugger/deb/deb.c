/* 
 *   Creation Date: <1999/02/22 23:12:50 samuel>
 *   Time-stamp: <2004/02/07 20:02:22 samuel>
 *   
 *	<deb.c>
 *	
 *	Stand alone debugger, MOL interface
 *   
 *   Copyright (C) 1999, 2000, 2001, 2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include <sys/socket.h>
#include <sys/un.h>
#include "poll_compat.h"
#include <stddef.h>
#include <time.h>

#include "deb.h"
#include "version.h"
#include "debugger.h"
#include "symbols.h"
#include "monitor.h"
#include "cmdline.h"
#include "mmu_contexts.h"
#include "dgram.h"
#include "res_manager.h"

#include "mac_registers.h"


static void do_connect( void );
static void exit_hook( void );

mac_regs_t *mregs;

typedef struct {
	int 			fd;
	dgram_receiver_t	*dgram_receiver;
	mac_regs_t 		mregs;
	mol_dgram_t		*queue;
} deb_static_t;

static deb_static_t sv;

int in_security_mode = 0;


/************************************************************************/
/*	FUNCTIONS							*/
/************************************************************************/

int
main( int argc, char **argv )
{
	int ret;
	
	if( getuid() ) {
		printf("You must be root to run the MOL debugger\n");
		return 1;
	}
	mregs = &sv.mregs;

        res_manager_init(0, argc, argv );
        atexit( res_manager_cleanup );

	/* Initialize socket and connect to MOL */
	sv.fd = -1;
	do_connect();
	
	symbols_init();
	monitor_init();
	cmdline_init();
	install_monitor_cmds();

	printm("Mac-on-Linux debugger %s, ", MOL_RELEASE);
	printm("(C) 2001 Samuel Rydh <samuel@ibrium.se>\n");

	atexit(exit_hook);
	for( ;; ) {
		if( (ret = mon_debugger()) == kDbgExit )
			break;
		send_dgram1( sv.fd, kMDG_debug_action, ret );
	}
	send_dgram( sv.fd, kMDG_disconnect );
	return 0;
}

static void
exit_hook( void )
{
	cmdline_cleanup();
	monitor_cleanup();
	symbols_cleanup();
}

static void
do_connect( void )
{
	int warned=0;
	char *socket_name = molsocket_name(); 	/* malloced */
	struct sockaddr_un addr;
	struct timespec tv;
	tv.tv_sec = 0;
	tv.tv_nsec = 100000000;

	printm("Trying to connect to MOL...\n");
	
	addr.sun_family = AF_UNIX;
	strcpy( addr.sun_path, socket_name );
	free( socket_name );
	
	if( sv.fd > 0 ) {
		close( sv.fd );
		sv.fd = -1;
	}
	if( (sv.fd = socket(PF_UNIX, SOCK_STREAM, 0)) < 0 ) {
		perrorm("socket");
		exit(1);
	}
	if( sv.dgram_receiver )
		free_dgram_receiver( sv.dgram_receiver );
	
	while( connect(sv.fd, (struct sockaddr*)&addr, sizeof(addr) ) < 0 ) {
		if( warned++>3 )
			printm("Waiting for MOL to start.\n"
			       "Make sure the debugger nub is enabled ('debug: true')\n");
		nanosleep(&tv, NULL);
	}
	printm("...connected\n");
	sv.dgram_receiver = create_dgram_receiver( sv.fd );

	send_dgram( sv.fd, kMDG_connect );
}


/************************************************************************/
/*	Message reception						*/
/************************************************************************/

static void
recv_dgram( mol_dgram_t *dg )
{
	int ans;

	switch( dg->what ){
	case kMDG_mregs:
		memcpy( (char*)mregs, dg->data, dg->data_size );
		break;
	case kMDG_printm:
		printm( "%s", dg->data );
		break;
	case kMDG_yn_question:
		ans = yn_question( dg->data, dg->p0 );
		send_dgram1( sv.fd,  kMDG_yn_answer, ans );
		break;
	case kMDG_refresh_instwin:
		redraw_inst_win();
		break;
	case kMDG_refresh_dbgwin:
		refresh_debugger_window();
		break;
	case kMDG_refresh_debugger:
		clear_cache();
		refresh_debugger();
		break;
	case kMDG_dbg_cmd: /* help_offs, numargs */
		add_remote_cmd( dg->data, dg->data+dg->p0, dg->p1 );
		break;
	case kMDG_connect:
		send_dgram( sv.fd, kMDG_mregs );
		printm("Connection established\n");
		break;
	case kMDG_disconnect:
		printm("MOL exited\n");
		/* we use the special return code 2 to make sure startmol
		 * does not kill-HUP the mol process during its cleanup phase
		 */
		exit(2);
		break;
	}
}

static mol_dgram_t *
dgram_wait( int wait_what )
{
	mol_dgram_t *dg, **dgp;
	struct pollfd ufds;
	int timeout;
	
	ufds.fd = sv.fd;
	ufds.events = POLLIN;
	ufds.revents = 0;

	/* Search the queue... */
	for( dgp=&sv.queue; *dgp; dgp=&(**dgp).next ) {
		if( (**dgp).what == wait_what || !wait_what ) {
			dg = *dgp;
			*dgp = dg->next;

			recv_dgram( dg );
			return dg;
		}
	}

	for( ;; ) {
		int err;
		int n = poll( &ufds, 1, 10 /*ms*/ );
		if( !n && !wait_what )
			return NULL;
		if( !n ) {
			timeout++;
			continue;
		}
		
		if( n<0 ) {
			if( errno != EINTR )
				perrorm("poll");
			continue;
		}
		
		if( !(dg = receive_dgram( sv.dgram_receiver, &err )) )
			continue;
		if( wait_what == dg->what || !wait_what ) {
			recv_dgram( dg );
			break;
		}
		/* Enqueue */
		for( dgp=&sv.queue; *dgp; dgp=&(**dgp).next )
			;
		*dgp = dg;
	}
	return dg;
}

void
poll_socket( void )
{
	mol_dgram_t *dg;

	if( (dg=dgram_wait(0)) )
	    free( dg );
}



/************************************************************************/
/*	FUNCTIONS							*/
/************************************************************************/

static char c_pagebuf[ 0x2000 ];
static int  c_ea=-1, c_context, c_use_dtrans;
static char c_breakbuf[256];
static ulong  c_break_ea=1;

void 
clear_cache( void )
{
	c_ea = -1;
	c_break_ea = 1;
}

ulong 
readc_ea( ulong ea, int context, int use_dtrans )
{
	int reload=0;

	if( (ea & ~0xfff) != c_ea && ((ea+3) & ~0xfff) != c_ea + 0x1000 )
		reload = 1;
	if( context != c_context || use_dtrans != c_use_dtrans )
		reload = 1;

	if( reload ) {
		mol_dgram_t *dg;

		send_dgram3( sv.fd,  kMDG_read_dpage, ea & ~0xfff, context, use_dtrans );
		if( !(dg=dgram_wait( kMDG_dpage_data )) )
			return 0xDEAD;
		memcpy( c_pagebuf, dg->data, sizeof(c_pagebuf) );
		free( dg );

		c_ea = ea & ~0xfff;
		c_context = context;
		c_use_dtrans = use_dtrans;
	}
	return *(ulong*)&c_pagebuf[(ea - c_ea) & 0x1fff];
}

int 
dbg_is_breakpoint( ulong mvptr, char *identifier )
{
	mol_dgram_t *dg;
	char ch;
	
	if( c_break_ea == 1 || mvptr < c_break_ea || mvptr >= c_break_ea + BREAK_BUF_SIZE*4 ) {
		c_break_ea = mvptr - (BREAK_BUF_SIZE*4)/2;
		if( c_break_ea >= 0xfffff000 )
			c_break_ea = mvptr;
		send_dgram1( sv.fd,  kMDG_is_breakpoint, c_break_ea );
		if( (dg=dgram_wait( kMDG_is_breakpoint )) ) {
			memcpy( c_breakbuf, dg->data, BREAK_BUF_SIZE );
			free( dg );
		}
	}

	ch = c_breakbuf[ (mvptr - c_break_ea)/4 ];
	if( identifier )
		*identifier = ch;

	if( ch )
		return 1;
	return 0;
}


void
set_ppc_mode( int in_ppc_mode ) 
{
	send_dgram1( sv.fd,  kMDG_in_ppc_mode, in_ppc_mode );
}

int
do_remote_dbg_cmd( int argc, char **argv )
{
	mol_dgram_t *dg;
	remote_cmd_t cmd;
	int i, s, ret=0;
	
	for(i=0, s=0; i<argc && i<MAX_CMD_NUM_ARGS; i++ ) {
		if( s + strlen(argv[i]) + 1 >= sizeof(cmd.buf) ) {
			printm("do_remote_dbg_cmd: overflow\n");
			return 0;
		}
		cmd.offs[i] = s;
		s += strlen(argv[i]) + 1;
		strcpy( cmd.buf + cmd.offs[i], argv[i] );
	}
	cmd.argc = i;
	send_dgram_buf( sv.fd,  kMDG_dbg_cmd, (char*)&cmd, sizeof(cmd) );

	if( (dg=dgram_wait( kMDG_result )) ) {
		ret = dg->p0;
		free(dg);
	}
	return ret;
}


/**************************************************************
*  get_inst_context / get_data_context
*
**************************************************************/

int
get_inst_context( void ) 
{
	if( mregs->msr & MSR_IR ) {
		if( mregs->msr & MSR_PR )
			return kContextMapped_U;
		return kContextMapped_S;
	}
	return kContextUnmapped;
}

int
get_data_context( void ) 
{
	if( mregs->msr & MSR_DR ) {
		if( mregs->msr & MSR_PR )
			return kContextMapped_U;
		return kContextMapped_S;
	}
	return kContextUnmapped;
}

void 
dbg_add_breakpoint( ulong addr, int flags, int data )
{
	send_dgram3( sv.fd, kMDG_add_breakpoint, addr, flags, data );
}


void
send_mregs_to_mol( void ) 
{
	send_dgram_buf( sv.fd, kMDG_write_mregs, (char*)mregs, sizeof(*mregs) );
}

void
refetch_mregs( void )
{
	mol_dgram_t *dg;
	clear_cache();

	send_dgram( sv.fd, kMDG_mregs );
	if( (dg=dgram_wait( kMDG_mregs )) )
		free( dg );
}
