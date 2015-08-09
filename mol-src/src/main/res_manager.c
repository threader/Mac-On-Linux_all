/* 
 *   Creation Date: <1999/03/29 07:35:25 samuel>
 *   Time-stamp: <2004/02/07 16:50:25 samuel>
 *   
 *	<res_manager.c>
 *	
 *	Resource manager & option parsing
 * 	Note: This file is used both stand alone and in MOL
 *
 *   Copyright (C) 1999, 2000, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "min_obstack.h"
#include "debugger.h"
#include "mol_assert.h"
#include "res_manager.h"
#include "version.h"

#include "cmdline_opts.h"

#define obstack_chunk_alloc	malloc
#define obstack_chunk_free	free

typedef struct darg {
	char		*data;
	struct darg	*next;		/* next data struct */
} darg_t;

typedef struct res {
	char 		*key;

	darg_t	 	*first_darg;
	struct res	*next;
} res_t;

typedef struct res_head {
	struct obstack 	stack;
	res_t		*first_res;
} res_head_t;

static res_head_t head;

/* keys that may occur multiple times in the config file */
static char *multi_res[] = {
	"netdev", "blkdev", "symfile", "pseudofile", "pseudofile_", "@pseudofile", "remap_key",
	"remap_xkey", "remap_vnckey", "macos_rompath", "macos_rompath_", "allow", "deny", "drivers",
	"scsi_dev", "pci_proxy_device", 
	NULL
};

enum { kAddHead=1, kAddTail=2, kAddSecure=4 };

/* Globals */
int	g_session_id = 0;


/************************************************************************/
/*	Molrc keywords							*/
/************************************************************************/

typedef struct {
	int	in_if;		/* if depth */
	int	dummy_if;	/* if depth (inactive code block) */
	int	skipping;	/* skip code */

	char	*str;		/* for next_field... */
	int	n;
	int	len;
} parser_state_t;

static void	parse_res_file( const char *fname, int how );


static char *
expand( char *dest, const char *src_org, int size )
{
	static int nesting=0;
	char *s, *e, *src;
	int i;
	
	src = alloca( strlen(src_org) + 1 );
	strcpy( src, src_org );

	dest[0] = 0;

	while( nesting < 10 && (s=strstr(src, "${")) && (e=strchr(s,'}')) ) {
		*s = *e = 0;
		strncat0( dest, src, size );
		src = e + 1;
		s +=2;
		if( !(e=get_str_res(s)) ) {
			strncat0( dest, "${", size );
			strncat3( dest, s, "}", size );
			continue;
		}
		for( i=0; (e=get_str_res_ind(s, 0, i)); i++ ) {
			if( i )
				strncat0( dest, " ", size );
			nesting++;
			expand( dest + strlen(dest), e, size - strlen(dest) );
			nesting--;
		}
	}
	strncat0( dest, src, size );
	return dest;
}

static char *
next_field( const char *s, int *n, int len )
{
	for( s+=*n ; *n < len && !*s ; (*n)++, s++ )
		;
	if( *n < len ) {
		*n += strlen( s );
		return (char*)s;
	}
	return NULL;
}

static char *
next_field_( parser_state_t *st, char *buf, int size )
{
	const char *s;
	if( !(s=next_field( st->str, &st->n, st->len )) )
		return NULL;
	return expand( buf, s, size );
}

static void
keyw_include( parser_state_t *st ) 
{
	char buf[128], *s;
	static int depth=0;

	if( depth > 6 )
		fatal("config file nesting depth exceeded\n");

	if( (s=next_field_( st, buf, sizeof(buf) )) ) {
		depth++;
		parse_res_file( buf, kAddTail );
		depth--;
	}
	return;
}

static void
keyw_do_if( parser_state_t *st, int flag )
{
	if( st->skipping ) {
		st->dummy_if++;
		st->skipping++;
	} else {
		st->in_if++;
		st->skipping = !flag;
	}
}

static void
keyw_ifeq( parser_state_t *st ) 
{
	char *s1, *s2, buf1[256], buf2[256];
	int flag = 0;
	
	s1 = next_field_( st, buf1, sizeof(buf1) );
	while( (s2=next_field_(st, buf2, sizeof(buf2))) )
		if( !strcmp(s1,s2) )
			flag = 1;

	keyw_do_if( st, flag );
}

static void
keyw_ifempty( parser_state_t *st ) 
{
	char buf[256], *s = next_field_( st, buf, sizeof(buf) );
	keyw_do_if( st, !(s && s[0] && s[0] != '$') );
}

static void
keyw_ifdef( parser_state_t *st ) 
{
	char buf[256], *s = next_field_( st, buf, sizeof(buf) );
	keyw_do_if( st, (s && s[0] && s[0] != '$') );
}

static void
keyw_endif( parser_state_t *st ) 
{
	char *s, buf[256];

	/* handle '} else {' */
	while( (s=next_field_(st, buf, sizeof(buf))) ) {
		if( !strncmp(s,"else",4) ){
			if( !st->dummy_if )
				st->skipping = !st->skipping;
			return;
		}
	}
	if( st->dummy_if ) {
		st->dummy_if--;
		st->skipping--;
	} else {
		st->in_if--;
		st->skipping = 0;
	}
}

static void
keyw_echo( parser_state_t *st ) 
{
	char *s, buf[256];
	while( (s=next_field_(st, buf, sizeof(buf))) )
		printm("%s ", s );
	printm("\n");
}

static void
keyw_fail( parser_state_t *st ) 
{
	keyw_echo( st );
	exit(1);
}

typedef struct {
	int	skip_interp;
	char	*kword;
	void 	(*action)( parser_state_t *s );
} molrc_keyw_t;

static molrc_keyw_t keywords[] = {
	{ 0, "include",		keyw_include },
	{ 0, "echo",		keyw_echo },
	{ 0, "fail",		keyw_fail },
	{ 1, "ifeq",		keyw_ifeq },
	{ 1, "ifempty",		keyw_ifempty },
	{ 1, "ifndef",		keyw_ifempty },
	{ 1, "ifdef",		keyw_ifdef },
	{ 1, "}",		keyw_endif },
	{ 0, NULL,		NULL }
};


/************************************************************************/
/*	low-level manipulation						*/
/************************************************************************/

static char *
push_str( const char *str )
{
	return str ? obstack_copy( &head.stack, str, strlen(str)+1 ) : NULL;
}

static int 
do_add_res( const char *const_res_str, int how, parser_state_t *st ) 
{
	molrc_keyw_t *k;
	parser_state_t st2;
	darg_t *darg, **next_darg;
	char buf[256], *s, *field;
	res_t *res;
	int i, n, len, cflag1, cflag2;

	if( !st ) {
		memset( &st2, 0, sizeof(st2) );
		st = &st2;
	}

	/* the original string must not be altered */
	s = alloca( strlen(const_res_str)+1 );
	strcpy( s, const_res_str );

	cflag1 = cflag2 = 0;
	for( len=strlen(s), n=0; n < len; n++ ) {
		if( !cflag2 && s[n] == '"' )
			cflag1 = !cflag1, s[n] = 0;
		if( !cflag1 && s[n] == '\'' )
			cflag2 = !cflag2, s[n] = 0;
		if( cflag1 || cflag2 )
			continue;
		if( strspn( s + n, "\t\n\r, ") )
			s[n] = 0;
		if( s[n] == '#' )
			while( n < len )
				s[n++] = 0;
	}
	n = 0;
	if( !(field=next_field( s, &n, len )) )
		return 0;

	/* parse keywlords */
	for( k=keywords; !(how & kAddSecure) && k->kword; k++ )
		if( (!st->skipping || k->skip_interp) && !strcmp( field, k->kword ) ) {
			st->str = s; st->n = n; st->len = len;
			(*k->action)( st );
			return 0;
		}
	if( st->skipping )
		return 0;

	/* parse resource line */
	if( field[strlen(field)-1] != ':' || strlen(field) == 1 )
		return 1;

	field[strlen(field)-1] = 0;
	field = expand( buf, field, sizeof(buf) );

	/* only allow multiple instances for certain resources  */
	if( res_present(field) && !(how & kAddHead) ) {
		for( i=0; multi_res[i] && strcmp(multi_res[i], field) ; i++ )
			;
		if( !multi_res[i] )
			return 0;
	}

	/* head */
	res = obstack_alloc( &head.stack, sizeof(res_t) );
       	memset( res,0,sizeof(res_t) );
	res->key = push_str( field );
	
	/* arguments */
	next_darg = &res->first_darg;
	while( (field=next_field(s, &n, len)) ) {
		field = expand( buf, field, sizeof(buf) );
		darg = obstack_alloc( &head.stack, sizeof(darg_t) );
		memset( darg, 0, sizeof(darg_t) );
		darg->data = push_str( field );
		*next_darg = darg;
		next_darg = &darg->next;
	}

	/* insert res in table */
	if( how & kAddTail ) {
		res_t **rp = &head.first_res;
		for( ; *rp ; rp = &(**rp).next )
			;
		*rp = res;
	} else {
		res->next = head.first_res;
		head.first_res = res;
	}
	return 0;
}

static void
add_multiple_res( const char *res_buf, int how )
{
	char *copy;
	int n;
	
	while( strlen(res_buf) ) {
		if( !(n=strcspn(res_buf, "\n;")) ) {
			res_buf++;
			continue;
		}
		copy = malloc(n+1);
		memcpy( copy, res_buf, n );
		copy[n]=0;
		res_buf += n;
		do_add_res( copy, how, NULL );
		free(copy);
	}
}

static void
parse_res_file_( FILE *f, const char *fname, int how )
{
	parser_state_t st;
	char buf[4096];
	int n, ln=0;
	
	memset( &st, 0, sizeof(st) );
	
	while( fgets(buf, sizeof(buf), f) && strlen(buf) ) {
		ln++;
		do {
			n = strlen(buf);
			if( n >= sizeof(buf)-1 || n < 2 || buf[n-2] != '\\' )
				break;
			buf[n-2] = ' ';
			ln++;
		} while( fgets(buf + n-1, sizeof(buf)-n+1, f) );

		if( do_add_res(buf, how, &st) )
			fatal("Parse error in %s:%d\n", fname, ln );
	}
	fclose( f );

	if( st.in_if || st.dummy_if )
		fatal("Unbalanced if in %s\n", fname );
}

static void
parse_res_file( const char *fname, int how )
{
	FILE *f;
	if( !(f=fopen(fname, "r")) )
		fatal("Failed to open %s\n", fname );

	parse_res_file_( f, fname, how );
}


static res_t *
find_res( const char *key, int index )
{
	res_t *res;
	
	for( res=head.first_res; res; res=res->next ) {
		if( !strcmp(res->key, key ) )
			index--;
		if( index < 0 )
			break;
	}
	return res;
}

void
missing_config_exit( const char *s )
{
	fatal("---> Missing molrc config '%s'.\n", s );
}

void
missing_config( const char *s )
{
	printm("---> Missing config '%s'. Should perhaps be specified in '%s'\n", 
	       s, get_molrc_name() );
}


#if 0
/************************************************************************/
/*	DEBUG								*/
/************************************************************************/

static void
dump_resources( void )
{
	res_t *r = head.first_res;
	darg_t *arg;
	
	for( ; r ; r=r->next ) {
		printm("%s: ", r->key );
		for( arg=r->first_darg; arg; arg=arg->next )
			printm("\"%s\" ", arg->data );
		printm("\n");
	}
}
#endif

/************************************************************************/
/*	Interface functions						*/
/************************************************************************/

char *
add_res( const char *key, const char *value )
{
	char *buf;
	int len = strlen(key)+strlen(value)+10;
	buf = alloca( len );

	snprintf( buf, len, "%s: %s", key, value );
	do_add_res( buf, kAddHead | kAddSecure, NULL );
	return get_str_res( key );
}

static void
add_numeric_res( const char *key, int val )
{
	char buf[16];
	sprintf(buf, "%d", val );
	add_res( key, buf );
}

void
default_res( const char *key, const char *value )
{
	char *buf;
	int len = strlen(key)+strlen(value)+10;
	buf = alloca( len );

	snprintf( buf, len, "%s: %s", key, value );
	do_add_res( buf, kAddTail, NULL );
}

int
res_present( const char *key )
{
	return (find_res(key, 0))? 1: 0;
}

char *
get_str_res( const char *key )
{
	return get_str_res_ind( key, 0, 0 );
}

char *
get_str_res_ind( const char *key, int index, int argnum )
{
	res_t *res;
	darg_t *darg;
	
	if( (res = find_res( key, index )) == NULL )
		return NULL;

	for( darg=res->first_darg; darg && argnum>0; argnum--, darg=darg->next )
		;
	return darg? darg->data : NULL;
}

/* returns -1 if not found (otherwise 0 or 1) */
int 
get_bool_res( const char *key )
{
	char *str;
	
	str = get_str_res( key );
	if( !str )
		return -1;
	if( strpbrk(str, "yY1tT") )
		return 1;
	if( strpbrk(str,"nN0fF") )
		return 0;

	printm("Bad boolean option: '%s'\n",str );
	return -1;
}

/* returns -1 if not found */
long
get_numeric_res( const char *key )
{
	char *str = get_str_res(key);
	return !str ? -1 : strtol( str, NULL, !strncmp("0x", str, 2) ? 16 : 10 );
}

/* match arguments against an option table and return the resulting flags */
ulong
parse_res_options( const char *key, int index, int first_arg, opt_entry_t *opts, 
		   const char *err_str )
{
	ulong flags=0;
	char *opt, *s;
	int i;
	
	for( ;; ) {
		if( !(opt=get_str_res_ind(key, index, first_arg++)) )
			break;
		/* allow unexpanded variables */
		if( !strncmp(opt, "${", 2) )
			continue;

		for( s=opt ; s && *s ; s=strchr(s+1,'-') ) {
			char *end = strpbrk(s," \t");
			int n = end ? end - s : strlen(s);

			for( i=0; opts[i].name && strncmp(s, opts[i].name, n); i++ )
				;
			if( opts[i].name )
				flags |= opts[i].flag;
			else
				printm("%s '%s'\n", err_str? err_str : "WARNING: Unknown option", s );
		}
	}
	return flags;
}


/************************************************************************/
/*	filename handling						*/
/************************************************************************/

char *get_libdir( void )  	{ return get_str_res("lib"); }
char *get_datadir( void ) 	{ return get_str_res("data"); }
char *get_vardir( void )	{ return get_str_res("var"); }
char *get_bindir( void )	{ return get_str_res("bin"); }
char *get_lockfile( void )	{ return get_str_res("lockfile"); }
char *get_molrc_name( void )	{ return get_str_res("molrc"); }

/************************************************************************/
/*	option parsing							*/
/************************************************************************/

#ifndef HAVE_GETOPT_H
/* Note: Optional arguments are unsupported */
static int 
getopt_long( int argc, char **argv, const char *opts, struct option *opt, int *opt_index )
{
	int i;

	optarg = NULL;
	if( opt_index )
		*opt_index = 0;
	for( ; optind < argc ; ) {
		char *s = argv[optind++];
		
		if( !strcmp("--", s) )
			return -1;

		if( strncmp("--", s, 2) ) {
			optind--;
			break;
		}
		s += 2;
		
		for( i=0; opt[i].optname; i++ ) {
			if( strncmp(opt[i].optname, s, strlen(opt[i].optname)) )
				continue;
			if( opt_index )
				*opt_index = i;
			if( opt[i].has_arg )
				if( (optarg=strchr(s, '=')+1) == (char*)1 )
					optarg = (++optind < argc) ? argv[optind] : NULL;
			return opt[i].val;
		}
		return '?';
	}
	return getopt( argc, argv, opts );
}
#endif

static void 
parse_options( int is_mol, int argc, char **argv )
{
	int c;
	
	optind=1;	/* should not be needed */
	for( ;; ) {
		struct opt_res_entry *or_ptr;
		int match=0;
		
		opterr=0;	/* suppress errors */
		if( (c=getopt_long(argc, argv, "adexhVX0123456789", opt, NULL)) == -1 )
			break;

		/* convert to resource */
		for( or_ptr=or_table; or_ptr->opt_code; or_ptr++ ){
			if( or_ptr->opt_code != c )
				continue;
			match=1;
			if( or_ptr->root_only && in_security_mode ) {
				printm("WARNING: A root-only flag was ignored\n");
				break;
			}
			switch( or_ptr->res_type ) {
			case kResLine:
				add_multiple_res( optarg, kAddHead );
				break;
			case kNumericRes:
				if( optarg )
					add_numeric_res( or_ptr->str, atol(optarg) );
				break;
			case kStringRes:
				if( optarg )
					add_res( or_ptr->str, optarg );
				break;
			case kParseRes:
				add_multiple_res( or_ptr->str, kAddHead );
				break;
			}
			if( or_ptr->opt_code == kNumericRes || or_ptr->opt_code == kStringRes )
				if( !optarg )
					printm("Warning: Missing option argument\n");

			/* several entries may match the same opt_code */
		}
		if( c >= '0' && c <='9' ) {
			add_numeric_res("session", c-'0' );
			c=0;
		}
		
		/* options which require special attention */
		switch(c) {
		case 0:
			break;
		case 'V':
			/* mol prints version info in main() */
			if( is_mol )
				exit(0);
			break;
		case 'h':
			if( is_mol ){
				fprintf(stderr, "%s", helpstr );
				exit(0);
			}
			break;
		case '?':
			fprintf( stderr, "option '%s' not recognized\n", argv[optind-1] );
			exit(1);
			break;
		default:
			if( !match )
				fprintf(stderr, "getopt returned unexpected value %d\n", c );
			break;
		}
	}

	if( optind < argc ) {
		fprintf(stderr, "Options ignored: ");
		while( optind < argc )
			fprintf(stderr, "%s ",argv[optind++]);
		fprintf(stderr, "\n");
	}
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

void 
res_manager_init( int is_mol, int argc, char **argv )
{
	char buf[256], *s;

	memset( &head, 0, sizeof(head) );
	obstack_init( &head.stack );

	/* add default paths */
	add_res( "lib", MOL_LIB_DIR );
	add_res( "var", MOL_VAR_DIR );
	add_res( "etc", MOL_ETC_DIR );
	strcpy( buf, "${lib}/bin" );
	add_res( "bin", buf );
	add_res( "arch", ARCH_STR );

	parse_options( is_mol, argc, argv );

	/* change directory to the datadir */
	if( !(s=get_str_res("data")) )
		s = add_res( "data", MOL_DATA_DIR );
	if( chdir(s) < 0 )
		fatal("Could not change working directory to %s\n", s );

	/* parse the various molrc files */
	parse_res_file( "config/molrc.sys", kAddTail );
	strncpy0( buf, get_str_res("etc"), sizeof(buf) );
	add_res( "molrc", buf );
	strncat0( buf, "/session.map", sizeof(buf) );
	
	parse_res_file( buf, kAddTail );
	parse_res_file( "config/molrc.post", kAddTail );

	/* determine session number */
	if( (g_session_id=get_numeric_res("session")) < 0 )
		missing_config_exit("session");
}

void
res_manager_cleanup( void )
{
	obstack_free( &head.stack, NULL );
}
