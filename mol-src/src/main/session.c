/* 
 *   Creation Date: <2000/08/26 16:44:40 samuel>
 *   Time-stamp: <2003/08/09 15:51:11 samuel>
 *   
 *	<session.c>
 *	
 *	Session management (fast save/restore)
 *   
 *   Copyright (C) 2000, 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "session.h"
#include "verbose.h"
#include "res_manager.h"
#include "version.h"
#include "timer.h"
#include "booter.h"
#include "mol_assert.h"
#include "molcpu.h"
#include "debugger.h"

#define DISABLE_SAVE_SESSION		/* TEMPORARY DISABLED */

SET_VERBOSE_NAME("session");

#define SESSION_FILE_RESOURCE	"session_file"

#define MAX_SAVE_PROCS		5
#define MAX_PREPARE_PROCS	5
#define MAX_SAVE_PROCS_DYN	20
#define MAX_NUM_CHUNKS		40

#define HEAD_VERSION		0x1
#define HEAD_MAGIC		0x4d4f4c20 	/* 'MOL ' */
#define INVALID_MAGIC		0x4d4f4c49 	/* 'MOLI' */

typedef struct {
	int		type;
	int		id;
	off_t		offs;
	size_t		size;
} chunk_t;

typedef struct {
	int		magic;
	int		header_version;
	int		mol_version;

	int		dyn_offs;		/* offset to dynamical data */
	int		num_chunks;
	int		reserved[3];

	/* array of chunk_t follows */ 
} head_t;

static struct {
	session_save_fp	save_procs[ MAX_SAVE_PROCS ];
	session_save_fp	save_procs_dyn[ MAX_SAVE_PROCS_DYN ];
	session_prepare_fp prep_procs[ MAX_PREPARE_PROCS ];

	int		nsave_procs, nsave_procs_dyn, nprep_procs;

	int		loading_session;
	int		fd;

	chunk_t		*chunks;

	int		dyn_offs;		/* offset to dynamical data */
	off_t		eof;			/* EOF of data in use */

	int		initialized;
} sd;

void 
session_save_proc( session_save_fp proc, session_prepare_fp prep, int dynamic_data ) 
{
	assert( sd.initialized );
	
	if( sd.nsave_procs_dyn >= MAX_SAVE_PROCS_DYN ||
	    sd.nsave_procs >= MAX_SAVE_PROCS ||
	    sd.nprep_procs >= MAX_PREPARE_PROCS )
	{
		LOG("Number of save-procs exceeded (%d)\n", dynamic_data );
		exit(1);
	}
	
	if( dynamic_data && proc )
		sd.save_procs_dyn[ sd.nsave_procs_dyn++ ] = proc;
	if( !dynamic_data && proc )
		sd.save_procs[ sd.nsave_procs++ ] = proc;
	if( prep )
		sd.prep_procs[ sd.nprep_procs++ ] = prep;
}

static int
chunk_compress( void )
{
	chunk_t *k1, *k2;
	int i, num=0;
	
	for( k1=k2=sd.chunks, i=0; i<MAX_NUM_CHUNKS; i++, k1++ ){
		if( k1->type ) {
			*k2++ = *k1;
			num++;
		}
	}
	return num;
}

static void
save_session_( int dummy_id, void *dummy, int info )
{
	save_session();
}

void
schedule_save_session( int usecs ) 
{
	if( is_oldworld_boot() )
		printm("Session support is only available in the newworld setting\n");
	else
		usec_timer( usecs, save_session_, NULL );
}

/* Ret: 0 success, 1 try later, -1 error */
int
save_session_now( void )
{
	int i, err=0;
	char *name = get_filename_res( SESSION_FILE_RESOURCE );
	head_t head;
	size_t size;
	chunk_t *k;

#ifdef DISABLE_SAVE_SESSION
	printm("Session support is unavailable (temporary disabled)\n");
	return -1;
#endif
	
	if( is_oldworld_boot() ){
		printm("Session support is not available in the oldworld setting\n");
		return -1;
	}

	if( !name ) {
		printm("Resource %s is missing\n", SESSION_FILE_RESOURCE );
		return -1;
	}

	/* First check that we are in a recoverable state */
	for(i=0; i<sd.nprep_procs; i++ )
		if( (*sd.prep_procs[i])() )
			return 1;
	
	/* Create/open session file if needed */ 
	if( sd.fd <= 0 ) {
		if( (sd.fd = open( name, O_RDWR | O_CREAT, 00644 )) < 0 ) {
			LOG_ERR("Could not create/open session file '%s'", name );
			return -1;
		}
		sd.eof = sizeof(head_t) + MAX_NUM_CHUNKS*sizeof(chunk_t);
		sd.dyn_offs = sd.eof;

		sd.chunks = calloc( MAX_NUM_CHUNKS, sizeof(chunk_t) );
	}
	
	/* Clear out dynamic chunks */
	for(i=0, k=sd.chunks; i<MAX_NUM_CHUNKS; i++, k++ )
		if( k->offs + k->size > sd.dyn_offs )
			k->type = 0;
	
	sd.eof = sd.dyn_offs;

	/* First run procs whose data must be static (e.g. file-mapped RAM) */
	for( i=0; !err && i<sd.nsave_procs; i++ )
		err = (*sd.save_procs[i])();

	sd.dyn_offs = sd.eof;

	for( i=0; !err && i<sd.nsave_procs_dyn; i++ )
		err = (*sd.save_procs_dyn[i])();

	memset( &head, 0, sizeof(head) );
	head.magic = HEAD_MAGIC;
	head.header_version = HEAD_VERSION;
	head.mol_version = MOL_VERSION;
	head.dyn_offs = sd.dyn_offs;
	head.num_chunks = chunk_compress();
	lseek( sd.fd, 0, SEEK_SET );
	err = err || write( sd.fd, &head, sizeof(head) ) != sizeof(head);
	size = sizeof(chunk_t)*head.num_chunks;
	err = err || write( sd.fd, sd.chunks, size ) != size;

	if( err ) {
		printm("Failed saving the session\n");
		close(sd.fd);
		sd.fd = -1;
		unlink( name );
		return -1;
	}
	return 0;
}

static chunk_t *
find_chunk( int type, int id )
{
	int 	i;
	chunk_t *k;
	
	for(k=sd.chunks, i=0; i<MAX_NUM_CHUNKS; i++, k++ )
		if( k->type && k->type == type && k->id == id )
			return k;
	return NULL;
}


/************************************************************************/
/*	data read/write							*/
/************************************************************************/

static int
stype_to_type( char *stype )
{
	if( !stype || ((strlen(stype) != 4 && strlen(stype) != 3)) ) {
		printm("Bad stype %s (must be three or four charachter long)\n", stype );
		return 0;
	}
	return *(int*)stype;
}

int
loading_session( void )
{
	assert( sd.initialized );
	return sd.loading_session;
}

void
session_is_loaded( void )
{
	head_t head;

	if( !loading_session() )
		return;

#if 1
	lseek( sd.fd, 0, SEEK_SET );
	if(read( sd.fd, &head, sizeof(head) ) != sizeof(head)) {
		printm("*** Unable to read session file ***\n");
		return;
	}

	head.magic = INVALID_MAGIC;

	lseek( sd.fd, 0, SEEK_SET );
	if(write( sd.fd, &head, sizeof(head) ) != sizeof(head)) {
		printm("*** Unable to write session file ***\n");
		return;
	}
#else
	printm("*** should invalidate session file ***\n");
#endif
}

int
read_session_data( char *stype, int id, char *ptr, size_t size ) 
{
	chunk_t *k;
	int s, type = stype_to_type(stype);

	if( sd.fd <= 0 ) {
		LOG("Internal error in read_session_data\n");
		return -1;
	}
	if( !(k=find_chunk(type, id)) )
		return 1;
 	if( size != k->size ) {
		LOG("Size mismatch\n");
		return -1;
	}

	lseek( sd.fd, k->offs, SEEK_SET );
	s=read( sd.fd, ptr, size );

	if( s != size ){
		LOG_ERR("read_session_data");
		return -1;
	}
	return 0;
}

int
get_session_data_fd( char *stype, int id, int *fd, off_t *offs, ssize_t *size )
{
	int type = stype_to_type(stype);
	chunk_t *k;
	if( !(k = find_chunk( type, id )))
		return -1;
	*fd = sd.fd;
	if( offs )
		*offs = k->offs;
	if( size )
		*size = k->size;
	return 0;
}

int
get_session_data_size( char *stype, int id )
{
	int type = stype_to_type(stype);
	chunk_t *k;

	if( !(k = find_chunk( type, id )) )
		return -1;
	return k->size;
}

void
align_session_data( int boundary )
{
	if( (sd.eof & (boundary-1)) )
		sd.eof = (sd.eof & ~(boundary-1)) + boundary;
}

int
write_session_data( char *stype, int id, char *ptr, ssize_t size )
{
	int type = stype_to_type(stype);
	chunk_t *k;
	
	if( sd.fd <= 0 ) {
		LOG("Internal error in write_session_data!\n");
		return -1;
	}
	k = find_chunk( type, id );

	if( k && k->size < size ) {
		LOG("*** Static key '%s' increased in size! ***\n", stype );
		k->type = 0;
		k = NULL;
	}

	if( !k ) {
		int i;
		for(i=0, k=sd.chunks; i<MAX_NUM_CHUNKS && k->type; i++, k++ )
			;
		if( i >= MAX_NUM_CHUNKS ) {
			LOG("number of chunks exceeded!\n");
			return -1;
		}
		k->type = type;
		k->id = id;
		k->offs = sd.eof;
		sd.eof += size;
	}
	k->size = size;
	if( !ptr )
		return 0;

	if( lseek( sd.fd, k->offs, SEEK_SET ) < 0 ) {
		LOG_ERR("lseek");
		return -1;
	}
	if( write( sd.fd, ptr, k->size ) < 0 ){
		LOG_ERR("write");
		return -1;
	}
	return 0;
}

ulong
calc_str_checksum( char *str )
{
	if( !str )
		return 0;
	return calc_checksum( str, strlen(str) );
}

ulong
calc_checksum( char *ptr, int size )
{
	int i, ret=0;

	for( i=0; i<size; i++ )
		ret += ptr[i] * (i%19+i);
	return ret;
}

void
session_failure( char *err )
{
	char *name = get_filename_res( SESSION_FILE_RESOURCE );

	if( err )
		printm("*** %s", err );
	printm("*** Failed to restore the saved session.\n");
	printm("*** Delete the session file '%s' to do a normal boot.\n", name );

	exit(1);
}


/************************************************************************/
/*	Debugger CMDs							*/
/************************************************************************/

static int __dcmd
cmd_save_session( int argc, char **argv )
{
	int err;
	if( argc != 1 )
		return 1;

	if( !(err=save_session_now() )) {
		printm("Session saved successfully\n");
	} else if( err==1 ) {
		printm("Unsafe state - session not saved\n");
	}
	return 0;
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

void 
session_init( void )
{
	char *s;
	int fd=-1;
	head_t head;
	
	memset( &sd, 0, sizeof(sd) );
	sd.initialized = 1;
#ifdef DISABLE_SAVE_SESSION
	/* printm("The session save/restore feature is disabled\n"); */
	return;
#endif
	add_cmd( "save_session", "save_session\nSave session\n", -1, cmd_save_session );

	if( !(s=get_filename_res( SESSION_FILE_RESOURCE )))
		return;
	if( (fd=open(s, O_RDWR)) < 0 )
		return;

	if( read(fd, &head, sizeof(head)) != sizeof(head)) {
		LOG_ERR("error reading session-file header");
		session_failure(NULL);
	}
	if( head.header_version != HEAD_VERSION )
		session_failure("Session file has bad header version!\n");

	if( head.magic == INVALID_MAGIC ){
		close(fd);
		// unlink(s);
		return;
	}
	if( head.magic != HEAD_MAGIC )
		session_failure("Session file header magic is invalid\n");

	if( head.num_chunks > MAX_NUM_CHUNKS )
		session_failure("Session file contains too many chunks (limit exceeded)\n");
	
	sd.chunks = calloc( MAX_NUM_CHUNKS, sizeof(chunk_t) );
	if( read( fd, sd.chunks, sizeof(chunk_t)*head.num_chunks ) != sizeof(chunk_t)*head.num_chunks ) {
		LOG_ERR("Failed reading chunk headers");
		session_failure(NULL);
	}
	sd.dyn_offs = head.dyn_offs;
	sd.fd = fd;
	sd.loading_session = 1;
}

void
session_cleanup( void )
{
	if( !sd.initialized )
		return;
	if( sd.fd > 0 )
		close( sd.fd );
	if( sd.chunks )
		free( sd.chunks );
}
