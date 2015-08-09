/* 
 *   Creation Date: <2000/09/09 12:07:19 samuel>
 *   Time-stamp: <2004/02/28 20:20:57 samuel>
 *   
 *	<nub.c>
 *	
 *	MOL-side debugger interface
 *   
 *   Copyright (C) 2000, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/param.h>
#include <stdarg.h>

#include "poll_compat.h"
#include "debugger.h"
#include "res_manager.h"
#include "dgram.h"
#include "async.h"
#include "molcpu.h"
#include "memory.h"
#include "nub.h"
#include "mmu_contexts.h"
#include "constants.h"
#include "mol_assert.h"
#include "timer.h"
#include "breakpoints.h"
#include "processor.h"

typedef struct cmd_entry {
	const char 	*cmdname;
	const char 	*help;
	dbg_cmd_fp	func;
	
	struct cmd_entry *next;
} cmd_entry_t;


typedef struct {
	int	sock_listen;
	int	async_listener_id;

	/* Fields describing the connection */
	int 	sock;
	int	dbg_attached;
	int	async_client_id;
	int	hup_count;
	
	dgram_receiver_t *dgram_receiver;

	/* debugger fields */
	int	in_ppc_mode;
	cmd_entry_t *cmd_root;
} nub_sv_t;

static int 		nub_inited;
static char 		*socket_name;
static nub_sv_t 	sv;


/************************************************************************/
/*	outgoing 							*/
/************************************************************************/

static void
send_mregs( void ) 
{
	if( sv.dbg_attached ) {
		make_mregs_dec_valid();
		send_dgram_buf( sv.sock, kMDG_mregs, (char*)mregs, sizeof(*mregs) );
	}
}

void
redraw_inst_win( void ) 
{
	if( nub_inited && sv.dbg_attached )
		send_dgram( sv.sock, kMDG_refresh_instwin );
}

void
refresh_debugger_window( void )
{
	if( nub_inited && sv.dbg_attached )
		send_dgram( sv.sock, kMDG_refresh_dbgwin );
}

void
refresh_debugger( void )
{
	if( nub_inited && sv.dbg_attached ) {
		send_mregs();
		send_dgram( sv.sock, kMDG_refresh_debugger );
	}
}

/************************************************************************/
/*	incoming							*/
/************************************************************************/

static void
debug_action( int action )
{
	stop_emulation();

	switch( action ) {
	case kDbgNOP:
		break;
	case kDbgGo:
		resume_emulation();
		break;
	case kDbgGoRFI:
		set_break_flag( BREAK_RFI );
		resume_emulation();
		break;
	case kDbgGoUser:
		set_break_flag( BREAK_USER );
		resume_emulation();
		break;
	case kDbgStep:
		set_break_flag( BREAK_SINGLE_STEP );
		restore_breakpoints();
		resume_emulation();
		break;
	case kDbgStop:
		stop_emulation();
		break;
	case kDbgExit:
		// quit_emulation();
		break;
	}
}

static void
receive_mregs( mac_regs_t *new_mregs )
{
	mac_regs_t old = *mregs;
	int i, spr_dirty = 0;
	
	*mregs = *new_mregs;
	if( mregs->msr != old.msr )
		msr_modified();

	for(i=0; i<NUM_SPRS; i++ )
		if( mregs->spr[i] != old.spr[i] )
			spr_dirty = 1;
	if( spr_dirty )
		_spr_changed();
}


static int
do_dbg_cmd( remote_cmd_t *cmd )
{
	cmd_entry_t *ce;
	char *argv[ MAX_CMD_NUM_ARGS ];
	int i;

	if( !cmd )
		return -1;
	for(i=0; i<MAX_CMD_NUM_ARGS; i++ )
		argv[i] = (char*)&cmd->buf + cmd->offs[i];

	for(ce=sv.cmd_root; ce; ce=ce->next ){
		if( !strcmp( ce->cmdname, argv[0] ) )
			break;
	}
	if( !ce ) {
		printm("Debugger command '%s' is missing\n", argv[0] );
		return 0;
	}
	return ce->func( cmd->argc, argv );
}


/************************************************************************/
/*	socket communication						*/
/************************************************************************/

/* Close connection to debugger client and free associated resources */
static void
close_sock( void )
{
	sv.dbg_attached = 0;
	
	if( sv.async_client_id )
		delete_async_handler( sv.async_client_id );
	sv.async_client_id = 0;

	if( sv.dgram_receiver )
		free_dgram_receiver( sv.dgram_receiver );
	sv.dgram_receiver = NULL;

	if( sv.sock > 0 )
		close( sv.sock );
	sv.sock = -1;
}

static void
handle_msg( mol_dgram_t *dg )
{
	cmd_entry_t *ce = sv.cmd_root;
	int i, val;
	
	switch( dg->what ) {
	case kMDG_connect:
		printm("Debugger attached\n");
		sv.dbg_attached = 1;
		send_dgram( sv.sock, kMDG_connect );
		send_mregs();
		send_dgram( sv.sock, kMDG_refresh_debugger );

		/* send debugger commands */
		for( ce=sv.cmd_root; ce; ce=ce->next ) {
			int s = strlen( ce->cmdname ) + 1;
			int t = strlen( ce->help ) + 1;
			char *b = malloc( s + t );
			strcpy( b, ce->cmdname );
			strcpy( b+s, ce->help );
			send_dgram_buf2( sv.sock, kMDG_dbg_cmd, b, s+t, s, -1 /*dummy*/ );
			free( b );
		}
		break;

	case kMDG_disconnect:
		sv.dbg_attached = 0;
		close_sock();
		printm("Debugger detached\n");
		break;

	case kMDG_mregs:
		send_mregs();
		break;

	case kMDG_write_mregs:
		receive_mregs( (mac_regs_t*)dg->data );
		break;

	case kMDG_read_dpage: { /* ea, context, data_access */
		char buf[0x2000];

		dg->p0 &= ~0xfff;
		restore_breakpoints();
		for( i=0; i<2; i++ ) {
			char *lvptr;
			if( ea_to_lvptr( dg->p0 + i*0x1000, dg->p1, &lvptr, dg->p2 ) )
				memset( buf+i*0x1000, 0xDE, 0x1000 );
			else
				memcpy( buf+i*0x1000, lvptr, 0x1000 );
		}
		setup_breakpoints();
		send_dgram_buf( sv.sock, kMDG_dpage_data, buf, 0x2000 );
		break;
	}

	case kMDG_in_ppc_mode:	/* flag */
		sv.in_ppc_mode = dg->p0;
		break;

	case kMDG_debug_action: /* action (go, single-step, etc.) */
		debug_action( dg->p0 );
		break;

	case kMDG_add_breakpoint: /* addr, flags, data */
		add_breakpoint( dg->p0, dg->p1, dg->p2 );
		break;

	case kMDG_is_breakpoint: { /* mvptr */ 
		char breakbuf[ BREAK_BUF_SIZE ];
		for(i=0; i<BREAK_BUF_SIZE; i++ ){
			if( !is_breakpoint( dg->p0 + i*4, &breakbuf[i] ) )
				breakbuf[i] = 0;
		}
		send_dgram_buf1( sv.sock, kMDG_is_breakpoint, breakbuf, BREAK_BUF_SIZE, dg->p0 );
		break;
	}
	
	case kMDG_dbg_cmd:
		val = do_dbg_cmd( (remote_cmd_t*)dg->data );
		send_dgram1( sv.sock, kMDG_result, val );
		break;

	default:
		printm("Unknown dbg-message %d received\n", dg->what );
		break;
	}
}

static void
nub_rcv( int fd, int dummy_events ) 
{
	struct pollfd ufds;
	mol_dgram_t *dg;
	int err, events;

	/* the events parameter can not be trusted since a queued async events can
	 * be stolen by a debugger_nub_poll() call.
	 */
	ufds.fd = fd;
	ufds.events = POLLHUP | POLLIN | POLLERR;
	if( poll(&ufds, 1, 0) <= 0 )
		return;
	events = ufds.revents;
	
	if( events & POLLHUP ) {
		if( sv.hup_count++ > 0 ) {
			sv.hup_count = 0;
			printm("Debugger connection lost\n");
			close_sock();
		}
		return;
	}

	if( events & ~POLLIN ) {
		fprintf(stderr, "nub_rcv events %x\n", events );
		return;
	}

	if( sv.dgram_receiver ) {
		if( (dg=receive_dgram(sv.dgram_receiver, &err)) ) {
			handle_msg(dg);
			free(dg);
		} else if( err ) {
			printm("nub_rcv: an error occured\n");
		}
	} else {
		printm("Unexpected nub_rcv\n");
	}
}

static void
nub_rcv_connection( int fd, int dummy_events )
{
	struct sockaddr addr;
	struct pollfd ufds;
	int events, len = sizeof(addr);

	/* the events parameter can not be trusted since a queued async events can
	 * be stolen by a debugger_nub_poll() call.
	 */
	ufds.fd = fd;
	ufds.events = POLLHUP | POLLIN | POLLERR;
	if( poll(&ufds, 1, 0) <= 0 )
		return;
	events = ufds.revents;

	if( events & POLLIN ) {
		/* close previous connection */
		close_sock();
		if( (sv.sock=accept(fd, &addr, (unsigned int *)&len)) < 0 ) {
			perrorm("accept\n");
			return;
		}
		sv.dgram_receiver = create_dgram_receiver( sv.sock );
		sv.async_client_id = add_async_handler( sv.sock, POLLIN, nub_rcv, 1 /* SIGIO */ );
		return;
	}
	printm("SOCKET: Event %x\n", events );
}

/* when the engine is not running, events are polled */
void
debugger_nub_poll( void )
{
	struct pollfd ufds[2];
	int n=1;
	
	if( !debugger_enabled() )
		return;

	ufds[0].fd = sv.sock_listen;
	ufds[0].events = POLLIN;
	
	if( sv.sock != -1 ) {
		n++;
		ufds[1].fd = sv.sock;
		ufds[1].events = POLLHUP | POLLIN;
	}
	if( poll(ufds, n, 0) > 0 ) {
		if( n>1 && ufds[1].revents )
			nub_rcv( sv.sock, ufds[1].revents );
		if( ufds[0].revents )
			nub_rcv_connection( sv.sock_listen, ufds[0].revents );
	}
}

static int
debugger_print( char *str ) 
{
	if( nub_inited && sv.dbg_attached )
		send_dgram_str( sv.sock, kMDG_printm, str );
	return 0;
}


/************************************************************************/
/*	world functions							*/
/************************************************************************/

int
debugger_enabled( void )
{
	static int flag = -2;

	/* Important! The debugger requires true root privileges */
	if( flag == -2 )
		flag = in_security_mode ? 0 : get_bool_res("debug");
	return flag;
}

int
debugger_in_68k_mode( void )
{
	return !sv.in_ppc_mode;
}

int
debugger_attached( void )
{
	return sv.dbg_attached;
}

void 
add_cmd( const char *cmdname, const char *help, int dummy, dbg_cmd_fp func )
{
	cmd_entry_t *ce;

	if( !debugger_enabled() )
		return;
	assert( nub_inited );

	ce = malloc( sizeof(cmd_entry_t) );

	ce->cmdname = cmdname;
	ce->help = help;
	ce->func = func;
	ce->next = sv.cmd_root;
	sv.cmd_root = ce;
}

void
add_dbg_cmds( dbg_cmd_t *table, int tablesize )
{
	int i;
	for( i=0; i<tablesize/sizeof(table[0]); i++ )
		add_cmd( table[i].name, table[i].help, -1, table[i].func );
}

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

void
debugger_init( void )
{
	struct sockaddr_un addr;

	/* only enable the debugger nub when it is explicitely requested by the user */
	if( !debugger_enabled() )
		return;
	printm("Debugger nub enabled\n");
	sv.sock = -1;

	/* create a unix socket for the debugger */
	socket_name = molsocket_name();		/* malloced */

	unlink( socket_name );
	if( (sv.sock_listen=socket(PF_UNIX, SOCK_STREAM, 0)) < 0 )
		fatal_err("socket");

	addr.sun_family = AF_UNIX;
	strcpy( addr.sun_path, socket_name );
	if( bind(sv.sock_listen, (struct sockaddr*)&addr, sizeof(addr)) < 0 )
		fatal_err("bind");

	if( listen(sv.sock_listen, 2) < 0 )
		fatal_err("listen");

	sv.hup_count = 0;
	sv.async_listener_id = add_async_handler( sv.sock_listen, POLLIN,
						  nub_rcv_connection, 1 /* SIGIO */ );
	/* initialize debugger */
	nub_inited = 1;
	if( debugger_enabled() ) {
		breakpoints_init();
		add_mmu_cmds();
	}
	set_print_hook( debugger_print );
}

void
debugger_cleanup( void )
{
	if( !nub_inited )
		return;
	if( sv.dbg_attached )
		send_dgram( sv.sock, kMDG_disconnect );

	if( sv.sock_listen > 0 ) {
		delete_async_handler( sv.async_listener_id );
		close( sv.sock_listen );
	}
	close_sock();
	sv.sock = sv.sock_listen = -1;
	unlink( socket_name );
	free( socket_name );

	if( debugger_enabled() )
		breakpoints_cleanup();

	while( sv.cmd_root ) {
		cmd_entry_t *ce = sv.cmd_root;
		sv.cmd_root = ce->next;
		free(ce);
	}
	nub_inited = 0;
}
