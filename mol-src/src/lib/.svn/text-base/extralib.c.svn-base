/* 
 *   Creation Date: <97/07/02 19:51:35 samuel>
 *   Time-stamp: <2004/04/03 18:29:18 samuel>
 *   
 *	<extralib.c>
 *	
 *	Miscellaneous functions
 *   
 *   Copyright (C) 1997, 1999, 2001, 2002, 2003, 2004 Samuel Rydh
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "mol_assert.h"
#include "extralib.h"
#include "debugger.h"
#include <stdarg.h>
#include <sys/wait.h>
#include <sys/uio.h>

/* rounded up */
int
ilog2( int val )
{
	int n=0;
	while( val > (1U << n) )
		n++;
	return n;
}


/************************************************************************/
/*	string miscellaneous						*/
/************************************************************************/

ulong 
string_to_ulong( char *str ) 
{
	ulong ret;
	
	/* for now, assume hexadecimal string */
	if( sscanf(str, "%lx",&ret ) )
		return ret;
	return 0;
}

char *
num_to_string( ulong value ) 
{
	char *str;
	
	str = (char*)malloc( 12 );
	sprintf(str,"%lX",value );

	return str;
}

int 
is_number_str( char *str ) 
{
	while( *str ) {
		if( !isxdigit(*str) )
			return 0;
		str++;
	}
	return 1;
}

/* 
 * hexbin - convert numbers to "binhex" format (e.g. 0xA -> 0x1010)
 */
ulong
hexbin( int num ) 
{
	ulong 	ret=0;
	int	i, base=1;
	
	for( i=0; i<8; i++, base=base*16 )
		if( num & (1<<i) )
			ret += base;
	return ret;
}

char *
strncpy0( char *dest, const char *src, size_t n )
{
	char *org = dest;

	if( n<=0 )
		return org;
	while( --n && (*dest++=*src++) )
		;
	if( !n )
		*dest=0;
	return org;
}

char *
strncat0( char *dest, const char *src, size_t n )
{
	char *org = dest;

	if( n<=0 )
		return org;
	while( --n && *dest )
		dest++;

	strncpy0( dest, src, ++n );
	return org;
}

char*
strncat3( char *dest, const char *s1, const char *s2, size_t n )
{
	strncat0( dest, s1, n );
	return strncat0( dest, s2, n );
}

char *
strnpad( char *dest, const char *s1, size_t n )
{
	char *org = dest;
	int len;
	
	if( n<=0 )
		return org;
	strncpy0( dest, s1, n );

	len = strlen( dest );
	n -= len;
	dest += len;

	while( --n )
		*dest++ = ' ';
	*dest++ = 0;
	return org;
}

/************************************************************************/
/*	iovec variants of memcpy					*/
/************************************************************************/

int
memcpy_tovec( const struct iovec *vec, size_t nvec, const char *src, unsigned int len )
{
	int i, n, s;

	for( n=0, i=0; i < nvec && len ; i++, n+=s, len-=s ) {
		if( (s=vec[i].iov_len) > len )
			s = len;
		memcpy( vec[i].iov_base, src + n, s );
	}
	return n;
}

int
memcpy_fromvec( char *dst, const struct iovec *vec, size_t nvec, unsigned int len )
{
	int i, n, s;

	for( n=0, i=0; i < nvec && len ; i++, n+=s, len-=s ) {
		if( (s=vec[i].iov_len) > len )
			s = len;
		memcpy( dst + n, vec[i].iov_base, s );
	}
	return n;
}

int
iovec_getbyte( int offs, const struct iovec *vec, size_t nvec )
{
	int i;
	
	for( i=0; i<nvec; i++ ) {
		int len = vec[i].iov_len;
		if( offs < len )
			return *((unsigned char*)vec[i].iov_base + offs);
		offs -= len;
	}
	return 0;
}

int
iovec_skip( int skip, struct iovec *vec, size_t nvec )
{
	int i;

	for( i=0; i<nvec; i++ ) {
		int len = vec[i].iov_len;
		if( len >= skip ) {
			vec[i].iov_base += skip;
			vec[i].iov_len -= skip;
			return 0;
		}
		vec[i].iov_len = 0;
		skip -= len;
	}
	return 1;
}


/************************************************************************/
/*	run a script (as root)						*/
/************************************************************************/

int
script_exec( char *name, char *arg1, char *arg2 )
{
	char buf[256];
	int status;
	pid_t pid;

	if( !name )
		return -1;

	if( !(pid=fork()) ) {
		buf[0]=0;
		if( name[0] != '/' )
			strncat0( getcwd(buf, sizeof(buf)), "/", sizeof(buf) );
		strncat0( buf, name, sizeof(buf) );

		setuid(0);	/* our scripts need root privileges */
		if( arg2 )
			execle( buf, name, arg1, arg2, NULL, NULL);
		else if( arg1 )
			execle( buf, name, arg1, NULL, NULL);
		else
			execle( buf, name, NULL, NULL);
		perrorm("Failed to execute '%s'\n", buf );
		_exit(-1);
	}
	(void) waitpid(pid, &status, 0);
	if( !WIFEXITED(status) ) {
		printm("!WIFEXITED()\n");
		return -1;
	}
	return WEXITSTATUS(status);
}


/************************************************************************/
/*	verbose output and logging					*/
/************************************************************************/

static FILE	*logfile;
static int 	(*print_hook)( char *buf );
static void 	(*print_guard)( void );

void
open_logfile( const char *name )
{
	if( !name )
		return;
	if( !(logfile=fopen(name, "w")) )
		printm("Failed to create logfile %s\n", name );
}

void
close_logfile( void )
{
	if( logfile )
		fclose( logfile );
	logfile = NULL;
}

void
set_print_hook( int (*hook)(char *buf) )
{
	print_hook = hook;
}

void
set_print_guard( void (*hook)( void ) )
{
	print_guard = hook;
}

int 
printm( const char *fmt,... ) 
{
        va_list args;
	char buf[256];
	int ret;

	if( print_guard )
		(*print_guard)();

	va_start( args, fmt );
	ret = vsnprintf( buf, sizeof(buf), fmt, args );
	va_end( args );

	buf[sizeof(buf)-1] = 0;

	if( logfile ) {
		fprintf( logfile, "%s", buf );
		fflush( logfile );
	}

	if( print_hook && (*print_hook)(buf) )
		return ret;

	fprintf( stderr, "%s", buf ); 
	return ret;
}

int
aprint( const char *fmt,... )
{
	va_list args;
	char buf[512];
	int ret=0;

	va_start( args, fmt );
	ret = vsnprintf( buf, sizeof(buf), fmt, args );
	buf[ sizeof(buf)-1 ] = 0;
	fprintf( stderr, "%s", buf );
	if( logfile )
		fprintf( logfile, "%s", buf );
	va_end( args );
	return ret;
}

void 
perrorm( const char *fmt, ... ) 
{
        va_list args;
	int ret, err=errno;
	char *p, buf[256];

	if( print_guard )
		(*print_guard)();

	va_start( args, fmt );
	ret = vsnprintf( buf, sizeof(buf), fmt, args );
	va_end( args );
	buf[sizeof(buf)-1] = 0;

	if( (p=strrchr(buf, '\n')) )
		memmove( p, p+1, strlen(p+1)+1 );

	printm("%s: %s\n", buf, strerror(err) );
}

void __attribute__((noreturn))
fatal( const char *fmt, ... ) 
{
        va_list args;
	char buf[256];

	va_start( args, fmt );
	vsnprintf( buf, sizeof(buf), fmt, args );
	va_end( args );
	buf[ sizeof(buf)-1 ] = 0;
	printm("Fatal error: %s\n", buf );
	exit(1);
}

/************************************************************************/
/*	Error checking							*/
/************************************************************************/

extern void
fail_nil( void *p )
{
	if( !p ) {
		printm("Allocation failure\n");
		exit(1);
	}
}
