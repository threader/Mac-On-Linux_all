/* 
 *   Creation Date: <2002/11/16 16:11:26 samuel>
 *   Time-stamp: <2004/02/07 19:00:09 samuel>
 *   
 *	<ioports.c>
 *	
 *	Memory mapped I/O emulation
 *   
 *   Copyright (C) 1997, 1999-2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "extralib.h"
#include "molcpu.h"
#include "debugger.h"
#include "ioports.h"
#include "booter.h"
#include "hacks.h"
#include "drivers.h"
#include "driver_mgr.h"

/* #define PERFORMANCE_INFO */
/* #define IOPORTS_VERBOSE  */

typedef struct io_range {
	struct io_range	*next;		/* list, addition order */
	struct io_range *snext;		/* ordered list */

	char		*name;
	int		id;		/* identifier */

	ulong		mphys;		/* base */
	ulong		size;		/* must be a multiple of 8 */
	int		flags;

	char		*buf;		/* IO-buffer */

	void		*usr;		/* user information */
	io_ops_t	ops;

#ifdef PERFORMANCE_INFO
	ulong		num_reads;	/* performance counter */
	ulong		num_writes;	/* performance counter */
#endif
} io_range_t;

static io_range_t	*root=0;
static io_range_t	*sroot=0;	/* only intended for verbose output */
static int		next_id=1;


/************************************************************************/
/*	unmapped read/write						*/
/************************************************************************/

static io_range_t *
get_range( ulong addr ) 
{
	io_range_t 	*cur;
	
	for( cur=root; cur; cur=cur->next )
		if( addr >= cur->mphys && addr <= cur->mphys+cur->size-1 )
			return cur;
	return 0;
}

static void 
print_io_access( int isread, ulong addr, ulong value, int len, void *usr )
{
	io_range_t *ior = get_range( addr );
	
	printm("%s: %s Address %08lX  Data: ", 
	       ior? ior->name : "** NO RANGE **",
	       isread? "I/O read " : "I/O write", addr );

	printm( (len==1)? "%02lX" : (len==2)? "%04lX" : "%08lX", value );
	printm("\n");
}

static ulong 
unmapped_read( ulong mphys, int len, void *usr )
{
#ifdef OLDWORLD_SUPPORT
	/* XXX: this is starmax stuff. It should be
	 * moved somewhere else. In particular the identification
	 * register in 0xf8000000.
	 */
	if( get_hacks() & hack_starmax ) {
		if( mphys == 0xf8000000 )
			return 0x10020000;
		else if( mphys == 0xf301a000 )
			return 0xff77;
	}
#endif
	/* detect unmapped RAM access */
	if( mphys < 0x80000000 ){
		if( (mregs->msr & MSR_DR) || !is_oldworld_boot() ){
			printm("Unmapped 'RAM-read-access', %08lx\n",mphys );
			stop_emulation();
		}
		return 0;
	}
#ifdef IOPORTS_VERBOSE
	if( mphys >= 0xf0000000 )
		printm("IOPORTS: Read from unmapped memory %08lX\n", mphys );
#endif
	/* silently ignore */
	return 0;
}

static void 
unmapped_write( ulong mphys, ulong data, int len, void *usr )
{
	if( mphys < 0x80000000 ){
		if( (mregs->msr & MSR_DR) || !is_oldworld_boot() ) {
			printm("Unmapped RAM-write-access, %08lx\n",mphys );
			/* stop_emulation(); */
		}
		return;
	}
#ifdef IOPORTS_VERBOSE
	if( mphys >= 0xf0000000 )
		printm("IOPORTS: Write to unmapped memory %08lX\n", mphys );
#endif
	/* silently ignore write to unampped I/O */
	return;
}

int 
add_io_range( ulong mbase, size_t size, char *name, int flags, io_ops_t *ops, void *usr )
{
	io_range_t *ior, **p;

	if( mbase & 0x7 || size & 0x7 ){
		printm("ERROR: IO-area has incorrect alignment: %08lX %08X\n",
		       mbase, (int)size);
		return 0;
	}

	if( !name )
		name = "NoName";
	if( !(ior = (io_range_t *) calloc(1, sizeof(io_range_t)+strlen(name)+1 )) ){
		printm("Out of memory");
		exit(1);
	}

	/* sorted list */
	for(p=&sroot; *p; p=&(**p).snext )
		if( (**p).mphys >= mbase )
			break;
	ior->snext = *p;
	*p = ior;

	/* ordered by addition time */
	ior->next = root;
	root = ior;

	ior->name = (char*)ior + sizeof(io_range_t);
	strcpy( ior->name, name );

	ior->id = next_id++;
	ior->mphys = mbase;
	ior->size = size;
	if( ops )
		ior->ops = *ops;
	if( !ior->ops.read )
		ior->ops.read = unmapped_read;
	if( !ior->ops.write )
		ior->ops.write = unmapped_write;
	if( !ior->ops.print )
		ior->ops.print = print_io_access;
		
	ior->flags = flags;
	ior->usr = usr;
	
	/* We do the size check to avoid VRAM/ROM etc, 
	 * which should be handled differently.
	 */
#if 1
	if( size <= 0x2000 )
		_add_io_range( mbase, size, ior );
#endif
	return ior->id;
}

int 
remove_io_range( int iorange_id )
{
	io_range_t **pn, *next, **pn2;

	for( pn = &root; *pn; pn = &(**pn).next )
		if( (**pn).id == iorange_id ) {
			/* Tell kernel this mapping is to be removed */
			_remove_io_range( (**pn).mphys, (**pn).size );

			/* We must remove ourselves from the sorted list also... */
			for( pn2 = &sroot; *pn2; pn2 = &(**pn2).snext )
				if( *pn2 == *pn ) {
					*pn2 = (**pn2).snext;
					break;
				}

			/* And cleanup */
			next = (**pn).next;
			free( *pn );
			*pn = next;
			return 0;
		}
	return 1;
}


/************************************************************************/
/*	io_{read,write} (must be reentrant, could be called from DMA)	*/
/************************************************************************/

static void 
mem_io_read( ulong mphys, int len, ulong *retvalue ) 
{
	io_range_t 	*cur;

	cur = get_range( mphys );
	if( !cur ) {
		*retvalue = unmapped_read( mphys, len, NULL );
		return;
	}
	do_io_read( cur, mphys, len, retvalue );
}

void 
do_io_read( void *_ior, ulong ptr, int len, ulong *retvalue )
{
	io_range_t *ior = _ior;
	ulong ret;

	if( !ior ) {
		mem_io_read( ptr, len, retvalue );
		return;
	}

#ifdef PERFORMANCE_INFO
	ior->num_reads++;	/* Performance statistics */
#endif

	if( ior->flags & IO_STOP )
		stop_emulation();

	ret = ior->ops.read( ptr, len, ior->usr );

	if( ior->flags & IO_VERBOSE )
		ior->ops.print( 1 /*read*/, ptr, ret, len, ior->usr );

	if( retvalue )
		*retvalue = ret;
}

static void 
mem_io_write( ulong mphys, ulong data, int len ) 
{
	io_range_t *cur;

	cur = get_range( mphys );
	if( !cur ) {
		unmapped_write( mphys, data, len, NULL );
		return;
	}

	do_io_write( cur, mphys, data, len );
	return;
}

void 
do_io_write( void *_ior, ulong mphys, ulong data, int len )
{
	io_range_t *ior = _ior;
	if( !ior ) {
		mem_io_write(mphys, data, len);
		return;
	}

#ifdef PERFORMANCE_INFO
	ior->num_writes++;	/* Performance statistics */
#endif
	if( ior->flags & IO_STOP )
		stop_emulation();

	if( ior->flags & IO_VERBOSE )
		ior->ops.print( 0 /* write */, mphys, data, len, ior->usr );

	ior->ops.write( mphys, data, len, ior->usr );
}


/************************************************************************/
/*	convenience functions						*/
/************************************************************************/

ulong 
read_mem( char *ptr, int len )
{
	switch( len ) {
	case 1:
		return *(unsigned char*)ptr;
	case 2:
		return *(unsigned short*)ptr;
	case 4:
		return *(ulong*)ptr;
	default:
		printm("Error in read_mem\n");
		break;
	}
	return 0;
}

void 
write_mem( char *ptr, ulong data, int len )
{
	switch( len ){
	case 1:
		*(unsigned char *)ptr = data;
		break;
	case 2:
		*(unsigned short*)ptr = data;
		break;
	case 4:
		*(ulong*)ptr = data;
		break;
	default:
		printm("Error in write_mem\n");
		break;
	}
}


/************************************************************************/
/*	Debugger CMDs							*/
/************************************************************************/

/* IO show */
static int __dcmd
cmd_ios( int numargs, char** args){
	io_range_t *cur;
	if( numargs != 1 )
		return 1;

	for( cur=sroot; cur; cur=cur->snext ) {
		printm("%c%c",
		       cur->flags & IO_STOP ? '*' : ' ',
		       cur->flags & IO_VERBOSE ? 'V' : ' ');
		printm("  Start: %08lX  Size: %08lX  ",cur->mphys,cur->size );
		if( cur->name )
			printm("%s\n",cur->name );
		else
			printm("\n");
	}
	return 0;
}

/* IO debug break */
static int __dcmd
cmd_iob( int argv, char **args) {	/* IO debug break */
	io_range_t *r;
	ulong	addr;
	
	if( argv != 2 )
		return 1;

	addr = string_to_ulong(args[1]);
	r =  get_range( addr );
	if( !r ) {
		printm("No IO-range covers the address %08lX\n",addr );
		return 0;
	}
	
	if( r->flags & IO_STOP )
		r->flags &= ~IO_STOP;
	else
		r->flags |= IO_STOP;
	
	printm("%s: %08lX-%08lX ", r->name, r->mphys, r->mphys+r->size-1);
	if( r->flags & IO_STOP )
		printm("will break on access\n");
	else
		printm("break cleared\n");
	return 0;
}

/* IO debug print */
static int __dcmd
cmd_iov( int argv, char **args) {	/* IO debug break */
	io_range_t *r;
	ulong	addr;
	
	if( argv != 2 )
		return 1;

	addr = string_to_ulong(args[1]);
	r =  get_range( addr );
	if( !r ) {
		printm("No IO-range covers the address %08lX\n",addr );
		return 0;
	}
	
	if( r->flags & IO_VERBOSE )
		r->flags &= ~IO_VERBOSE;
	else
		r->flags |= IO_VERBOSE;
	
	printm("%s: %08lX-%08lX ",r->name, r->mphys, r->mphys+r->size+1);
	if( r->flags & IO_VERBOSE )
		printm("will be verbose on access\n");
	else
		printm("verbose flag cleared\n");
	return 0;
}

/* IO-range clear verbose */
static int __dcmd
cmd_iocv( int argv, char **args ) {
	io_range_t *r;
	if( argv != 1 )
		return 1;
	for( r=root; r; r=r->next )
		r->flags &= ~IO_VERBOSE;
	printm("Verbose flags cleared\n");
	return 0;
}

/* IO-range clear break */
static int __dcmd
cmd_iocb( int argv, char **args ) {
	io_range_t *r;
	if( argv != 1 )
		return 1;
	for( r=root; r; r=r->next )
		r->flags &= ~IO_STOP;
	printm("Break flags cleared\n");
	return 0;
}

/* IO-range set verbose */
static int __dcmd
cmd_iosv( int argv, char **args ) {
	io_range_t *r;
	if( argv != 1 )
		return 1;
	for( r=root; r; r=r->next )
		r->flags |= IO_VERBOSE;
	printm("Verbose flags set\n");
	return 0;
}

/* IO-range clear break */
static int __dcmd
cmd_iosb( int argv, char **args ) {
	io_range_t *r;
	if( argv != 1 )
		return 1;
	for( r=root; r; r=r->next )
		r->flags |= IO_STOP;
	printm("Break flags set\n");
	return 0;
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static dbg_cmd_t dbg_cmds[] = {
#ifdef CONFIG_DEBUGGER
	{ "ios",  cmd_ios,  "ios \nshow IO-ranges\n" },
	{ "iob",  cmd_iob,  "iob addr \ntoggle IO-range break\n" },
	{ "iov",  cmd_iov,  "iov addr \ntoggle IO-range verbose flag\n" },
	{ "iocv", cmd_iocv, "iocv \nclear all IO-range verbose flags\n" },
	{ "iocb", cmd_iocb, "iocb \nclear all IO-range break flags\n" },
	{ "iosv", cmd_iosv, "iosv \nset all IO-range verbose flags\n" },
	{ "iosb", cmd_iosb, "iosb \nset all IO-range break flags\n" },
#endif
};

static int 
ioports_init( void )
{
	next_id = 0;
	root = 0;

	add_io_range( 0x80000000, 0x80000000, "IO_unmapped", 0, NULL, NULL );
	add_dbg_cmds( dbg_cmds, sizeof(dbg_cmds) );

	return 1;
}

static void 
ioports_cleanup( void ) 
{
	io_range_t *cur, *next;
	
#ifdef PERFORMANCE_INFO
	for( cur=root; cur; cur=cur->next )
		printm("%s R/W (%ld/%ld)\n", cur->name, cur->num_reads, cur->num_writes );
#endif
	for( cur=root; cur; cur=next ) {
		next = cur->next;
		free( cur );
	}
}

driver_interface_t ioports_driver = {
	"ioports", ioports_init, ioports_cleanup
};
