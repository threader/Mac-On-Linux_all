/* 
 *   Creation Date: <97/06/30 15:13:55 samuel>
 *   Time-stamp: <2004/02/07 20:22:51 samuel>
 *   
 *	<symbols.c>
 *	
 *	Symbols <-> addresses translation interface
 *   
 *   Copyright (C) 1997, 1999, 2000, 2002, 2003, 2004 Samuel Rydh
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "symbols.h"
#include "res_manager.h"
#include "cmdline.h"
#include "wrapper.h"
#include "monitor.h"

#define HASH_TABLE_INIT_SIZE		30000
/* when hash_used > hash_size/REHASH_LIMIT a rehash will occur */
#define REHASH_LIMIT			3

typedef struct {
	char 		*symbol;
	ulong		addr;
	int		flag;
	int		other;			/* index of corresponding entry in the other table */
} sym_rec_t;

enum{
	kUsed=1, kDeleted=2
};

static sym_rec_t	*hash_atos;		/* address to symbol */
static sym_rec_t	*hash_stoa;		/* symbol to address */

static int		hash_size;		/* both tables have the same size */
static int		hash_used;

static void		rehash( int offset );


static ulong 
find_prime( ulong num ) 
{
	ulong i, p;

	p = (num % 2)? num : num+1;
	for( ;; ) {
		for(i=3; i<p; i+=2 )
			if( !(p%i) )
				break;
		if( i==p )
			break;
		p+=2;
	}
	return p;
}

static int 
str_to_key( char *p ) 
{
	int base=3;
	ulong key=0;
	
	while( *p ) {
		key = key + ((int)*p) * base;
		base = (base << 2) + base;
		p++;
	}
	return( key % hash_size );
}

static int 
addr_to_key( ulong addr ) 
{
	return( (addr>>2) % hash_size );
}

static int
atos_index_from_addr( ulong addr ) 
{
	int index;

	if( !hash_size ) {
		fprintf(stderr, "Symbol hash table not allocated\n");
		return -1;
	}
	
	index = addr_to_key( addr );
	for( ;; ) {
		sym_rec_t *p;
		p = &hash_atos[index];
		
		if( p->addr == addr && (p->flag & kUsed) )
			return index;
		if( !(p->flag & kUsed) && !(p->flag & kDeleted) )
			break;

		index = (index + 1) % hash_size;
	}
	return -1;
}

static int 
stoa_index_from_symbol( char *symbol ) 
{
	int index;
	if( !hash_size ) {
		fprintf(stderr, "Symbol hash table not allocated\n");
		return -1;
	}
	
	index = str_to_key( symbol );
	for( ;; ) {
		sym_rec_t *p;
		p = &hash_stoa[index];
		
		if( p->symbol && (p->flag & kUsed) && !strcmp(p->symbol,symbol))
			return index;
		if( !(p->flag & kUsed) && !(p->flag & kDeleted) )
			break;

		index = (index + 1) % hash_size;
	}
	return -1;
}

static void 
hash_symbol( char *symstr, ulong addr ) 
{
	int index, atos_index;
	char *symbol;

	/* check if a rehash is needed */
	if( hash_used > hash_size/REHASH_LIMIT )
		rehash(0);

	symbol = (char*) malloc( strlen(symstr)+1 );
	strcpy( symbol,symstr );
	
	index = addr_to_key( addr );
	/* hash in atos */
	for( ;; ) {
		sym_rec_t *p = &hash_atos[index];

		if( p->addr == addr && (p->flag & kUsed) ) {
			/* entry alredy exists! */
			free( p->symbol );
			p->symbol = symbol;
			p->flag = kUsed;
			/* delete symbol in the other table */
			hash_stoa[p->other].flag = kDeleted;
			break;
		}
		if( !(p->flag & kUsed) ) {
			/* new entry */
			p->addr = addr;
			p->symbol = symbol;
			p->flag = kUsed;
			break;
		}
		index = (index + 1) % hash_size;
	}
	atos_index = index;

	/* hash in stoa */
	index = str_to_key( symstr );
	for( ;; ) {
		sym_rec_t *p;
		p = &hash_stoa[index];
		
		if( p->addr == addr || !(p->flag & kUsed) ) {
			p->symbol = symbol;
			p->flag = kUsed;
			p->addr = addr;
			p->other = atos_index;
			break;
		}
		index = (index + 1) % hash_size;
	}
	hash_atos[atos_index].other = index;
	
	hash_used++;
}

static void 
rehash( int offset )
{
	sym_rec_t *old_hash_atos = hash_atos;
	sym_rec_t *old_hash_stoa = hash_stoa;
	int old_hash_size = hash_size;
	sym_rec_t *p;
	int i;

	if( !offset )
		hash_size = find_prime( old_hash_size * 2 );
	hash_used = 0;
	hash_atos = (sym_rec_t*)calloc( hash_size, sizeof(sym_rec_t) );
	hash_stoa = (sym_rec_t*)calloc( hash_size, sizeof(sym_rec_t) );

	p = old_hash_atos;
	for( i=0; i<old_hash_size; i++,p++ ) {
		if( p->flag & kUsed )
			hash_symbol( p->symbol, p->addr + offset );
		free( p->symbol );
	}
	/* the two tables share the same string-memory */
	free( old_hash_atos );
	free( old_hash_stoa );
}

static void
hash_symbol_from_str( const char *str, int realloc_base )
{
	char dummy, buf2[102];
	char *p, *s = strdup(str);
	ulong addr;
	int valid = 1;
	
	if( s[0]=='*' )
		valid = sscanf(s+1,"%lx %100s", &addr, buf2 ) == 2;
	else {
		valid = (sscanf(s,"%lx %c %100s", &addr, &dummy, buf2) == 3)
			|| (sscanf(s,"0x%lx %100s,", &addr, buf2) == 2)
			|| (sscanf(s, "%lx %100s", &addr, buf2) == 2 );
	}
	
	buf2[sizeof(buf2)-1]=0;
	if( (p=strchr( buf2, '(' )) )
		*p = 0;
	if( valid ) {
		if( realloc_base != -1 ) {
			addr &= ~0xf0000000;
			addr += realloc_base;
		}
		hash_symbol( buf2, addr );
	}
	free(s);
}

static void
do_hash_symbols_from_file( char *filename, int realloc_base )
{
	char buf[200];
	FILE *f;

	if( !(f=fopen(filename, "r")) ) {
		printm("Symbol file '%s' not found\n",filename );
		return;
	}
	printm("Hashing symbols from '%s'\n", filename);

	while( fgets(buf, sizeof(buf), f) )
		hash_symbol_from_str( buf, realloc_base );
	fclose( f );
}


static void
hash_symbols_from_file( char *filename, int realloc_base ) 
{
	int i;
	if( filename ) {
		do_hash_symbols_from_file( filename, realloc_base );
		return;
	}
	for( i=0; (filename=get_filename_res_ind("symfile",i,0)) ; i++ ) {
		char *s = get_filename_res_ind( "symfile", i, 1 );
		realloc_base = s ? strtol( s, NULL, 0 ) : -1;
		do_hash_symbols_from_file( filename, realloc_base );
	}
}

static void 
export_symbols( char *filename ) 
{
	FILE *f;
	int i;
	
	if( !filename && !(filename=get_filename_res("export_symfile")) ) {
		printm("Please specify a filename\n");
		return;
	}
	if( !(f=fopen(filename, "w")) ) {
		printm("File '%s' could not be created\n", filename );
		return;
	}
	for( i=0; i<hash_size; i++)
		if( hash_atos[i].flag & kUsed )
			fprintf(f,"*%08lX\t%s\n",hash_atos[i].addr, hash_atos[i].symbol );
	fclose( f );
}

static int 
delete_hash_addr( ulong addr ) 
{
	int index = atos_index_from_addr( addr );
	if( index<0 )
		return -1;

	free( hash_atos[index].symbol );
	hash_atos[index].flag = kDeleted;
	hash_stoa[ hash_atos[index].other ].flag = kDeleted;
	hash_used--;
	return 0;
}

static void 
delete_all_hash_symbols( void ) 
{
	int i;
	sym_rec_t empty = {0,0,0,0};

	for(i=0; i<hash_size; i++) {
		if( hash_atos[i].flag & kUsed ) {
			free( hash_atos[i].symbol );
		}
		hash_atos[i] = hash_stoa[i] = empty;
	}
	hash_used = 0;
}

/************************************************************************/
/*	CMDs								*/
/************************************************************************/

/* add symbol */
static int 
cmd_as( int numargs, char **args ) 
{
	ulong addr, erraddr;
	char *errsym;
	
	if( numargs==1 || numargs>3 )
		return 1;

	erraddr = addr_from_symbol(args[1]);
	if( !erraddr )
		errsym = args[1];

       	addr = (numargs == 2)? get_nip() : string_to_ulong( args[2] );
	if( (errsym=symbol_from_addr(addr)) )
		erraddr = addr;

	if( erraddr ) {
		printm("Symbol '%s' with address 0x%08lx removed\n", errsym, erraddr );
		delete_hash_addr( erraddr );
	}
	printm("Symbol '%s' with address 0x%08lx added\n",args[1],addr );
	hash_symbol( args[1], addr );

	redraw_inst_win();
	return 0;
}

/* import symbols */
static int 
cmd_is( int numargs, char **args )
{
	if( numargs > 2 )
		return 1;

	printm("Importing symbols...\n");
	hash_symbols_from_file( (numargs==2)? args[1] : NULL, -1 );

	redraw_inst_win();
	return 0;
}

/* export symbols */
static int 
cmd_es( int numargs, char **args )
{
	if( numargs > 2 )
		return 1;

	if( !yn_question("Are you sure? ", 1) )
		return 0;

	printm("Exporting symbols...\n");
	export_symbols( (numargs==2)? args[1] : NULL );
	return 0;
}

/* clear symbols */
static int 
cmd_cs( int numargs, char **args ) 
{
	if( numargs > 1 )
		return 1;
	
	if( !yn_question("Do you really want to remove all symbols? ", 0 ))
		return 0;

	delete_all_hash_symbols();
	printm("All symbols removed\n");

	redraw_inst_win();
	return 0;
}

/* remove symbol */
static int 
cmd_rs( int numargs, char **args ) 
{
	ulong addr;
	
	if( numargs != 2 )
		return 1;

	if( is_number_str(args[1] ))
		addr = string_to_ulong( args[1] );
	else
		addr = addr_from_symbol( args[1] );

	if( delete_hash_addr(addr) == -1 ) {
		printm("No symbol found with address %08lx\n",addr );
		return 0;
	}

	printm("Symbol with address 0x%08lx removed\n", addr );
	redraw_inst_win();
	return 0;
}

/* move symbol */
static int 
cmd_ms( int numargs, char **args ) 
{
	ulong src, dest;

	if( numargs != 3 )
		return 1;
	src = string_to_ulong( args[1] );
	dest = string_to_ulong( args[2] );
	printm("Offseting symbols: %lx -> %lx (%lx)\n", src, dest, dest-src );
	rehash( dest - src );
	return 0;
}


#if 0
static int 
delete_hash_symbol( char *symbol ) 
{
	int index = stoa_index_from_symbol( symbol );
	if( index<0 )
		return -1;

	free( hash_stoa[index].symbol );
	hash_stoa[index].flag = kDeleted;
	hash_atos[ hash_stoa[index].other ].flag = kDeleted;
	hash_used--;
	
	return 0;
}
#endif

/************************************************************************/
/*	world interface							*/
/************************************************************************/

ulong 
addr_from_symbol( char *symbol ) 
{
	int index;
	
	index = stoa_index_from_symbol( symbol );
	if( index>=0 )
		return hash_stoa[index].addr;
	return 0;
}

char *
symbol_from_addr( ulong addr ) 
{
	int index;
	
	index = atos_index_from_addr( addr );

	if( index >= 0 )
		return hash_atos[index].symbol;
	return 0;
}

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

void 
symbols_init( void )
{
	hash_size = find_prime( HASH_TABLE_INIT_SIZE );
	hash_atos = (sym_rec_t*)calloc( hash_size, sizeof(sym_rec_t) );
	hash_stoa = (sym_rec_t*)calloc( hash_size, sizeof(sym_rec_t) );
	hash_used = 0;

	hash_symbols_from_file( NULL, -1 );

	add_cmd( "is", "is [filename] \nimport symbols from file\n", -1, cmd_is );
	add_cmd( "es", "es [filename] \nexport symbols to file\n", -1, cmd_es );
	add_cmd( "as", "as label [addr] \nadd symbol\n", -1, cmd_as );
	add_cmd( "rs", "rs label \nremove symbol\n", -1, cmd_rs );
	add_cmd( "ms", "ms from to \nmove all symbols (offset from-to)\n", -1, cmd_ms );
	add_cmd( "cs", "cs \nclear all symbols\n", -1, cmd_cs );
}

void 
symbols_cleanup( void ) 
{
	int i;

	for( i=0; i<hash_size; i++ )
		if( hash_atos[i].flag & kUsed )
			free( hash_atos[i].symbol );
	
	free( hash_atos );
	free( hash_stoa );
}
