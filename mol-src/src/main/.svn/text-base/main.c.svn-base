/* 
 *   Creation Date: <2002/06/27 22:47:46 samuel>
 *   Time-stamp: <2004/02/28 16:03:42 samuel>
 *   
 *	<main.c>
 *	
 *	
 *   
 *   Copyright (C) 1997, 1999-2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include <signal.h>
#include "memory.h"
#include "wrapper.h"
#include "promif.h"
#include "drivers.h"
#include "res_manager.h" 
#include "debugger.h"
#include "thread.h"
#include "timer.h"
#include "version.h"
#include "os_interface.h"
#include "booter.h"
#include "async.h"
#include "session.h"
#include "molcpu.h"
#include "rvec.h"
#include "misc.h"

static void	exit_hook( void );

/* GLOBALS */
int 		in_security_mode;
ulong		g_session_magic;
mac_regs_t	*mregs;


/************************************************************************/
/*	lockfile creation						*/
/************************************************************************/

static int
remove_lockfile( void )
{
	char buf[64], *name = get_lockfile();
	int fd, del=0;
	pid_t pid;
	FILE *f;
	
	if( (f=fopen(name, "r")) ) {
		del = (fscanf(f, "%d", &pid) != 1 || pid == getpid());
		fclose(f);
		if( !del ) {
			sprintf( buf, "/proc/%d", pid );
			if( (fd=open(buf, O_RDONLY)) >= 0 ) {
				close( fd );
				return 1;
			}
			if( errno != ENOENT )
				return 1;
			printm("Removing stale lockfile %s\n", name );
		}
		unlink( name );
	}
	return 0;
}

static int
create_lockfile( void )
{
	char *name = get_lockfile();
	FILE *f;

	/* printm("Session %d. Lockfile: %s\n", g_session_id, name ); */
	
 	if( (f=fopen(name, "r")) ) {
		fclose(f);
		if( remove_lockfile() ) {
			printm("MOL lockfile detected! Make sure MOL isn't running\n"
			       "before deleting the lockfile %s\n", get_lockfile() );
			return 1;
		}
	}

	if( !(f=fopen(name, "w+")) )
		printm("Warning: could not create the lockfile %s\n", name );
	else {
		fprintf( f, "%d\n", getpid() );
		fclose( f );
	}
	return 0;
}


/************************************************************************/
/*	signal handlers							*/
/************************************************************************/

int 
common_signal_handler( int sig_num )
{
	static time_t warntime = 0;
	static int once = 0;
	time_t cur;
	
	if( sig_num != SIGINT && sig_num != SIGTRAP ) {
		aprint("***** SIGNAL %d [%s] in thread %s *****\n", 
		       sig_num, (sig_num < NSIG)? sys_siglist[sig_num]:"?",
		       get_thread_name() );
	}

	switch( sig_num ) {
	case SIGINT:
		if( !is_main_thread() )
			return 1;

		/* backtrace(); */
		time( &cur );
		if( !warntime || cur-warntime>1 ) {
			aprint("Signal INT\nOne more to kill emulator\n");
			warntime = cur;
			/* break emulation */
			stop_emulation();
			return 1;
		}
		if( !once++ ) {
			quit_emulation();
			return 1;
		}
		/* hrm... lets try this if quit_emulation() does not work */
		exit(1);
		break;

	case SIGPIPE:
		/* usually the debugger... */
		return 1;
	}

	/* unhandled */
	return 0;
}

static void 
set_signals( void )
{
	struct sigaction act;
	sigset_t set;
	static int signals[] = {
		SIGINT, SIGHUP, SIGILL, SIGBUS, SIGSEGV, SIGTERM, SIGTRAP, SIGPIPE, SIGCHLD,
	};
	int i;
			
	sigemptyset( &set );
	sigaddset( &set, SIGCHLD );
	pthread_sigmask( SIG_BLOCK, &set, NULL );

	memset( &act, 0, sizeof(act) );
	sigemptyset(&act.sa_mask);
	act.sa_flags = SA_RESTART;
#ifdef __linux__
	act.sa_sigaction = (void*)signal_handler;
	act.sa_flags |= SA_SIGINFO;
	act.sa_restorer = NULL;
	sigaction( SIGSTKFLT, &act, NULL );
#else
	act.sa_handler = (void*)signal_handler;
#endif
	for( i=0; i<sizeof(signals)/sizeof(signals[0]) ; i++ )
		sigaction( signals[i], &act, NULL );
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static void
authenticate( void )
{
	char *s1, *s, *user = getlogin();
	int i, j, deny_all=0;

	s1 = NULL;
	for( i=0; get_str_res_ind( "allow", i, 0 ) ; i++ )
		for( j=0; (s1=get_str_res_ind( "allow", i, j )) && strcmp(s1,user) ; j++ )
			;
	s = NULL;
	for( i=0; get_str_res_ind( "deny", i, 0 ) ; i++ )
		for( j=0; (s=get_str_res_ind( "deny", i, j )) && strcmp(s,user) ; j++ )
			deny_all |= !strcasecmp( s, "all" );

	if( s || (!s1 && deny_all) ) {
		printm("User %s is not allowed to run MOL.\n", user );
		exit(1);
	}
}

static volatile int tty_signo;

static void
tty_sighandler( int signo )
{
	switch( signo ) {
	case SIGCHLD:
		exit(0);
		break;
	}
	tty_signo = signo;
}

static void
tty_loop( void )
{
	sigset_t set;
	pid_t p;
	char *s;

	if( !(s=get_lockfile()) )
		exit(1);
	s = strdup( s );
	res_manager_cleanup();

	signal( SIGCHLD, tty_sighandler );
	signal( SIGINT, tty_sighandler );
	signal( SIGTERM, tty_sighandler );
	signal( SIGHUP, tty_sighandler );

	sigemptyset( &set );
	sigaddset( &set, SIGCHLD );
	sigaddset( &set, SIGINT );
	sigaddset( &set, SIGTERM );
	sigaddset( &set, SIGHUP );
	sigprocmask( SIG_UNBLOCK, &set, NULL );

	for( ;; ) {
		FILE *f;

		while( !tty_signo )
			sleep(120);
		if( (f=fopen(s, "r")) ) {
			if( fscanf(f, "%d", &p) == 1 )
				kill( p, tty_signo );
			fclose( f );
			tty_signo = 0;
			continue;
		}
		exit(1);
	}
}

static void
do_clearenv( void )
{
#ifdef __linux__
	/* both DISPLAY and HOME are needed in order to get X11 working over ssh */
	const char *preserve[] = { "DISPLAY", "HOME" };
	char *s, *saved[ sizeof(preserve)/sizeof(preserve[0]) ];
	int i;

	for( i=0; i<sizeof(preserve)/sizeof(preserve[0]); i++ ) {
		if( (s=getenv(preserve[i])) )
			s = strdup( s );
		saved[i] = s;
	}
	clearenv();
	for( i=0; i<sizeof(preserve)/sizeof(preserve[0]); i++ ) {
		if( saved[i] ) {
			setenv( preserve[i], saved[i], 1 );
			free( saved[i] );
		}
	}
#endif
}


int
main( int argc, char **argv )
{
	/* Timespec struct for nanosleep workaround below */
	struct timespec tv;
	tv.tv_sec = 0;
	tv.tv_nsec = 1000000;

	printm("Mac-on-Linux %s [%s]\n", MOL_RELEASE, MOL_BUILD_DATE );
	printm("Copyright (C) 1997-2004 Samuel Rydh\n");

	/* If we are not root, disable privileged features. */
	in_security_mode = (getuid() != 0);

	/* clear most environmental variables (for extra security) */
	do_clearenv();
	
	res_manager_init( 1, argc, argv );	/* sets g_session_id */

	printm("Starting MOL session %d\n", g_session_id );
	authenticate();

	if( seteuid(0) ) {
		fprintf( stderr,"Mac-on-Linux must be setuid root!\n");
		exit(1);
	}
	load_mods();

	/* 
	 * It is necessary to run in a new session (for the console/FB stuff).
	 * The fork is necessary since setsid() usually fails otherwise.
	 */
	if( fork() ) {
		if( get_bool_res("detach_tty") != 1 && (isatty(0) || isatty(1) || isatty(2)) )
			tty_loop();
		return 0;
	}
	/* There is a race if we exit immediately (and if the lockfile is present);
	 * the SIGCHLD signal might be lost. It is not a serious problem but this
	 * removes it for all practical purposes.
	 */
	nanosleep(&tv, NULL);

	if( setsid() < 0 )
		perrorm("setsid failed!\n");

	wrapper_init();

	/* initialize session */
	if( create_lockfile() )
		exit(1);

	if( open_session() ) {		/* Provides mregs and MOL ioctls */
		remove_lockfile();
		exit(1);
	}

	atexit( exit_hook );

	open_logfile( get_filename_res("logfile") );
	determine_boot_method();

	/* we must be careful with dependencies here. */
	set_signals();
	threadpool_init();		/* Provides threads */
	async_init();			/* Provides async IO capabilities */
	debugger_init();		/* Provides logging and debugger */
	mainloop_init();		/* Provides set_rvector */
	os_interface_init();		/* Provides register_osi_call */

	session_init();
	booter_init();			/* Provides gPE.xxx */

	promif_init();			/* Provides OpenFirmware device tree  */
	timer_init();			/* Provides timer services */

	mem_init();			/* Memory subsystem */
	molcpu_init();			/* CPU / emulation */
	driver_mgr_init();		/* All drivers */
	booter_startup();		/* Boot loader */
	session_is_loaded();

	if( !is_main_thread() )
		printm("*** is_main_thread not working? ***\n");

	start_async_thread();		/* start event processing */
	mainloop_start();

	/* cleanup through exit_hook */
	return 0;
}

static void 
do_cleanup( void ) 
{
	printm("cleaning up...\n");
#ifdef __linux__
	console_make_safe();
#endif
	driver_mgr_cleanup();
	timer_cleanup();
	molcpu_cleanup();
	booter_cleanup();
	mem_cleanup();
	promif_cleanup();

	debugger_cleanup();
	async_cleanup();
	threadpool_cleanup();
	session_cleanup();

	mainloop_cleanup();
	os_interface_cleanup();

	close_session();
	wrapper_cleanup();

	close_logfile();
	remove_lockfile();
	res_manager_cleanup();
}

static void 
exit_hook( void )
{
	if( !is_main_thread() ) {
		/* Umm... one of the sub-threads has probably segfaulted.
		 * We better kill -9 the main thread and then tries cleanup
		 * things from this thread.
		 */ 
		printm("Exiting due to a crashed thread\n");
		pthread_kill( get_main_th(), 9 );
	}

	/* reset signal handlers */
	signal( SIGILL, SIG_DFL );
	signal( SIGSEGV, SIG_DFL );
	signal( SIGINT, SIG_DFL );
	signal( SIGTERM, SIG_DFL );
#ifdef __linux__
	signal( SIGSTKFLT, SIG_DFL );
#endif	
	do_cleanup();

	printm( "DONE\n" );
	/* execlp("/etc/mol.sh", "mol.sh", "stop", NULL ); */
}
