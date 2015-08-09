/* 
 *   Creation Date: <97/07/03 13:43:50 samuel>
 *   Time-stamp: <2003/10/22 13:46:14 samuel>
 *   
 *	<promif.c>
 *	
 *	OF Device Tree
 *   
 *   Copyright (C) 1997-2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/param.h>
#include <stdarg.h>
#include "min_obstack.h"

#include "wrapper.h"
#include "promif.h"
#include "debugger.h"
#include "res_manager.h"
#include "booter.h"
#include "mol_assert.h"
#include "os_interface.h"
#include "memory.h"

#define obstack_chunk_alloc	malloc
#define obstack_chunk_free	free

#define INDENT_VALUE		4

static struct {
	struct obstack		stack;		/* storage */

	mol_device_node_t	*root;
	mol_device_node_t	*allnext;

	int			initialized;
	int			next_phandle;
} oftree;

static char *drop_properties[] = {		/* properties to drop on export */
	"nvramrc",
	"driver-ist",				/* make sure no MacOS created properties */
	"driver-pointer",			/* properties comes through (BootX...) */
	"driver-ref",
	"driver-descriptor",
	"driver-ptr",
	"did",	/* ? */
	"driver,AAPL,MacOS,PowerPC",
	NULL
};

static char *drop_nodes[] = {
	NULL
};


/************************************************************************/
/*	node lookup							*/
/************************************************************************/

mol_device_node_t *
prom_get_root( void ) 
{
	return oftree.root;
}

mol_device_node_t *
prom_get_allnext( void )
{
	return oftree.allnext;
}

/* returns linked list with nodes */
mol_device_node_t *
prom_find_type( const char *type )
{
	mol_device_node_t *head, **prevp, *np;
	int len;
	
	prevp = &head;
	for( np=oftree.allnext; np ; np=np->allnext) {
		unsigned char *p = prom_get_property( np, "device_type", &len);
		if( strlen(type)+1 == len && !strcasecmp( (const  char *) type, (const char *) p) ) {
			*prevp = np;
			prevp = &np->next;
		}
	}
	*prevp = 0;
	return head;
}

/* Returns the node name */
static unsigned char *
node_name( mol_device_node_t *dn ) 
{
	unsigned char *p;
	int len;

	p = prom_get_property( dn, "name", &len );
	if(!len || p[len-1])
		return (unsigned char *) "";
	else
		return p;
}

mol_device_node_t *
prom_find_dev_by_path( const char *path )
{
	char *s, *s2, *str, *orgstr;
	mol_device_node_t *dn, *dn2;

	str = orgstr = strdup( path );
	dn = prom_get_root();
	while( (s=strsep( &str, "/" )) && dn ) {
		if( !strlen(s) )
			continue;

		/* strip ':flags' from path */
		s2 = s;
		strsep(&s2,":");

		dn2 = dn;
		for( dn=dn->child ; dn ; dn=dn->sibling )
			if( !strcasecmp((char *) node_name(dn), (char *) s) )
				break;
		if( dn )
			continue;

		/* support @N:flags addressing using unit_string. 
		 * A unit string '*' matches anything (hack).
		 */
		if( !(s2=strsep(&s, "@")) )
			continue;
		for( dn=dn2->child; dn ; dn=dn->sibling ) {
			if( !dn->unit_string )
				continue;
			/* handle the /parent/node@2 case */
			if( strlen(s2) && strcasecmp( (char *)node_name(dn), (char *)s2) )
				continue;
			if( *dn->unit_string == '*' )
				break;
			if( strtoul(s,NULL,16) == strtoul(dn->unit_string,NULL,16) )
				break;
		}
	}
 	free( orgstr );
	return dn;
}

mol_device_node_t *
prom_find_devices( const char *name )
{
	mol_device_node_t *head, **prevp, *np;

	prevp = &head;
	for( np=oftree.allnext; np ; np=np->allnext ) {
		if( !strcasecmp( (char *)node_name(np), (char *)name) ) {
			*prevp = np;
			prevp = &np->next;
		}
	}
	*prevp = 0;
	return head;
}

int 
is_ancestor( mol_device_node_t *ancestor, mol_device_node_t *dn )
{
	for( ; dn; dn=dn->parent )
		if( dn == ancestor )
			return 1;
	return 0;
}

mol_device_node_t *
prom_phandle_to_dn( ulong phandle )
{
	mol_device_node_t *dn;
	
	for( dn=oftree.allnext; dn ; dn=dn->allnext )
		if( (ulong)dn->node == phandle )
			break;
	return dn;
}


/************************************************************************/
/*	properties							*/
/************************************************************************/

static p_property_t *
find_property( mol_device_node_t *dn, const char *name )
{
	p_property_t *pp;
	if( !dn )
		return NULL;
	for( pp=dn->properties; pp; pp=pp->next )
		if( !strcmp(pp->name, name) )
			return pp;
	return NULL;
}

unsigned char *
prom_get_property( mol_device_node_t *dn, const char *name, int *lenp )
{
	p_property_t *pp;
	
	if( lenp )
		*lenp=0;
	if( (pp=find_property( dn, name )) ) {
		if( lenp )
			*lenp = pp->length;
		return pp->value ? (unsigned char*)pp->value : (unsigned char *)"";
	}
	return NULL;
}

unsigned char *
prom_get_prop_by_path( const char *path, int *retlen )
{
	mol_device_node_t *dn;
	char *p, *cpy = strdup(path);
	unsigned char *ret=NULL;

	if( retlen )
		*retlen=0;
	if( (p=strrchr(cpy, '/')) ) {
		*p=0;
		if( (dn=prom_find_dev_by_path(cpy)) )
			ret = prom_get_property( dn, p+1, retlen );
	}
	free( cpy );
	return ret;
}

/* NULL to obtain first property */
char *
prom_next_property( mol_device_node_t *dn, const char *prev_name )
{
	p_property_t *pp = dn->properties;
	
	if( !prev_name || !strlen( prev_name ) )
		return pp ? (char*) pp->name : NULL;
	for( ; pp ; pp=pp->next ) {
		if( !strcmp(pp->name, prev_name ) )
			return pp->next ? (char*) pp->next->name : NULL;
	}
	return NULL;
}

int
prom_get_int_property( mol_device_node_t *dn, const char *name, int *retval )
{
	int *p, len;
	if( !(p=(int*)prom_get_property(dn,name,&len)) || len != sizeof(int) )
		return 1;
	*retval = *p;
	return 0;
}

/* dynamically modify the device tree (dn must belong to the emulated tree!)  */
void
prom_add_property( mol_device_node_t *dn, const char *name, const char *data, int len )
{
	p_property_t *pp;
	unsigned char *ptr = NULL;

	if( (pp=find_property(dn, name)) ) {
		if( pp->length >= len )
			ptr = pp->value;
	} else {
		pp = obstack_alloc( &oftree.stack, sizeof( p_property_t ));
		pp->name = obstack_copy0( &oftree.stack, name, strlen(name) );
		pp->next = dn->properties;
		dn->properties = pp;
	}
	if( !ptr )
		ptr = obstack_alloc( &oftree.stack, len );
	pp->value = ptr;
	pp->length = len;
	memmove( pp->value, data, len );
}


/************************************************************************/
/*	node creation							*/
/************************************************************************/

static void
_get_full_name( mol_device_node_t *dn, char *buf, int len )
{
	if( dn->parent )
		_get_full_name( dn->parent, buf, len );
	
	/* this avoids a two leading / */
	if( !(dn->parent && !dn->parent->parent) )
		strncat0( buf, "/", len );

	/* root node name (e.g. devic-tree) should not be included */
	if( dn->parent )
		strncat0( buf, (char *) node_name(dn), len );
	if( dn->unit_string )
		strncat3( buf, "@", dn->unit_string, len );
}

static char *
get_full_name( mol_device_node_t *dn, char *buf, int len )
{
	if( !len )
		return NULL;

	buf[0] = 0;
	_get_full_name( dn, buf, len );
	return buf;
}

/* modifies path! */
static mol_device_node_t *
create_node( char *path, char *name )
{
	mol_device_node_t *par;
	char *p;

	par = prom_find_dev_by_path( path );
	if( !par ) {
		while( (p=strrchr(path, '/')) && !*(p+1) )
			*p = 0;
		if( !p )
			goto out;
		*p = 0;
		par = create_node( path, p+1 );
		if( !par )
			goto out;
	}
	if( !name )
		return par;

	return prom_add_node( par, name );
 out:
	printm("Unexpected error in create_node\n");
	return NULL;
}

mol_device_node_t *
prom_create_node( const char *path )
{
	mol_device_node_t *dn;
	char *str = strdup( path );
	dn = create_node( str, NULL );
	free( str );

	return dn;
}

mol_device_node_t *
prom_add_node( mol_device_node_t *par, const char *_name )
{
	mol_device_node_t *dn;
	char *p, *name = strdup( _name );
	
	dn = obstack_alloc( &oftree.stack, sizeof( mol_device_node_t) );
	memset( dn, 0, sizeof(mol_device_node_t) );

	if( (p=strchr(name, '@')) ) {
		dn->unit_string = obstack_copy0( &oftree.stack, p+1, strlen(p+1) );
		*p = 0;
	}
	dn->node = (p_phandle_t)oftree.next_phandle++;
	dn->parent = par;
	dn->sibling = par->child;
	par->child = dn;
	dn->allnext = par->allnext;
	par->allnext = dn;

	prom_add_property( dn, "name", name, strlen(name)+1 );

	free( name );
	return dn;
}

int
prom_delete_node( mol_device_node_t *dn )
{
	mol_device_node_t **dd;

	if( !dn )
		return 0;
	assert(dn->parent);

	while( dn->child )
		prom_delete_node( dn->child );

	/* Unlink us from parent */
	for( dd = &dn->parent->child; *dd && *dd != dn; dd = &(**dd).sibling )
		;
	assert( *dd );
	*dd = (**dd).sibling;

	/* ...and from allnext list */
	for( dd=&oftree.allnext ; *dd && *dd != dn ; dd = &(**dd).allnext )
		;
	assert( *dd );
	*dd = (**dd).allnext;

	return 0;
}


/************************************************************************/
/*	interrupt tree lookup						*/
/************************************************************************/

static mol_device_node_t *
irq_parent( mol_device_node_t *dn )
{
	mol_device_node_t *dn2;
	int par;

	if( !prom_get_int_property(dn, "interrupt-parent", &par) ) {
		if( !(dn2=prom_phandle_to_dn(par)) )
			printm("Bogus interrupt-parent (%s)\n", node_name(dn) );
		return dn2;
	}
	return dn->parent;
}

static int
get_icells( mol_device_node_t *dn )
{
	mol_device_node_t *dn2 = dn;
	int val;

	dn = irq_parent(dn);
	for( ; dn ; dn=irq_parent(dn) )
		if( !prom_get_int_property(dn, "#interrupt-cells", &val) ) {
			//printm("get_icells: %d\n", val );
			return val;
		}
	printm("irq_cell_size failed (%s)\n", node_name(dn2) );
	return 1;
}

static int
get_acells( mol_device_node_t *dn )
{
	int val;
	while( dn ) {
		dn = dn->parent;
		if( !prom_get_int_property(dn, "#address-cells", &val) )
			return val;
	}
	/* default */
	return 1;
}

static int
map_irq( mol_device_node_t *dn, int irq_ind, int **ret_irq, mol_device_node_t **ret_ic )
{
	int *mask, *m, *reg, maplen, masklen, rlen;
	int i, match, il, acells = get_acells( dn );
	int icells = get_icells( dn );
	int *intr = (int*)prom_get_property( dn, "interrupts", &il );
	mol_device_node_t *p;

	reg = (int*)prom_get_property( dn, "reg", &rlen );
	rlen /= sizeof(int);
	il /= sizeof(int);

	if( irq_ind >= il/icells || !intr )
		return 0;
	intr += irq_ind * icells;

	for( p=irq_parent(dn) ; p ; ) {
		if( prom_get_property(p, "interrupt-controller", NULL) )
			break;
		if( !(m=(int*)prom_get_property(p, "interrupt-map", &maplen)) ) {
			p = irq_parent(dn);
			continue;
		}
		if( !(mask=(int*)prom_get_property(p, "interrupt-map-mask", &masklen)) ) {
			printm("interrupt-map-mask missing\n");
			return 0;
		}
		maplen /= sizeof(int);
		masklen /= sizeof(int);

		if( rlen < acells ) {
			printm("map_irq: bad rlen\n");
			return 0;
		}
		for( i=0, match=1 ; maplen > 0; maplen--, m++, i++ ) {
			mol_device_node_t *ic;
			int xa, xb;

			if( i < acells ) {
				if( (*m & mask[i]) != (reg[i] & mask[i]) )
					match = 0;
				continue;
			}
			if( i - acells < icells ) {
				if( (*m & mask[i]) != (reg[i] & intr[i-acells]) )
					match = 0;
				continue;
			}
			ic = prom_phandle_to_dn( *m++ );
			maplen--;
			if( !maplen || !ic ) {
				printm("map_irq: lookup failure (%08x %08x)\n", m[-1], (int)ic );
				return 0;
			}
			if( prom_get_int_property(ic, "#interrupt-cells", &xa) ) {
				printm("map_irq: error\n");
				return 0;
			}
			if( prom_get_int_property(ic, "#address-cells", &xb) )
				xb = 0;

			/* iterate the new reg/irq pair */
			if( match && maplen >= xa + xb ) {
				p = ic;
				icells = xa;
				rlen = acells = xb;
				intr = m;
				reg = intr - acells;
				break;
			}
			maplen -= xa + xb - 1;
			m += xa + xb - 1;
			match = 1;
			i = -1;
		}
		if( maplen <= 0 ) {
			printm("map_irq: no match\n");
			return 0;
		}
	}
	if( !p ) {
		printm("map_irq: no_match!\n");
		return 0;
	}
	if( ret_ic )
		*ret_ic = p;
	if( ret_irq )
		*ret_irq = intr;
	return icells;
}

static int
has_irq_map( void ) 
{
	mol_device_node_t *dn;
	for( dn=oftree.allnext; dn; dn=dn->allnext )
		if( prom_get_property(dn, "interrupt-controller", NULL) )
			return 1;
	return 0;
}

int
prom_irq_lookup( mol_device_node_t *dn, irq_info_t *irqinfo )
{
	mol_device_node_t *ic;
	int *p, i, *ip, nirq = 0;

	p = has_irq_map() ? (int*)prom_get_property(dn, "interrupts", NULL) : NULL;

	/* interrupt tree traversal */
	for( i=0; i<sizeof(irqinfo->irq)/sizeof(irqinfo->irq[0]) ; i++ ) {
		irqinfo->irq[i] = -1;
		irqinfo->controller[i] = 0;
		if( p && map_irq(dn, i, &ip, &ic) ) {
			irqinfo->irq[i] = *ip;
			irqinfo->controller[i] = prom_dn_to_phandle( ic );
			nirq = i+1;
		}
	}

	/* AAPL,interrupts (oldworld) */
	if( !p && !nirq ) {
		ip = (int*)prom_get_property( dn, "AAPL,interrupts", &nirq );
		if( !ip && dn->parent )
			ip = (int*)prom_get_property( dn->parent, "AAPL,interrupts", &nirq );
		nirq /= sizeof(int);

		for( i=0; i<nirq; i++ )
			irqinfo->irq[i] = *ip++;
	}

	irqinfo->nirq = nirq;
	return nirq;
}

static void
check_irq_tree( int do_print ) 
{
	mol_device_node_t *dn;
	irq_info_t info;
	int i;
	
	for( dn=oftree.allnext; dn ; dn=dn->allnext) {
		if( prom_irq_lookup(dn, &info) && do_print ) {
			printm("   %-26s ", node_name(dn) );
			for( i=0; i<info.nirq; i++ )
				printm("%d ", info.irq[i] );
			printm("\n");
		}
	}
}


/************************************************************************/
/*	device tree export						*/
/************************************************************************/

static int 
indprint( FILE *file, int ind, const char *fmt,... ) 
{
	va_list args;
	int ret, i;

	va_start( args, fmt );

	if( ind<0 )
		ind = 0;
	for( i=0; i<ind; i++ )
		fprintf(file, " " );
	
	ret = vfprintf( file, fmt, args );
	va_end( args );
	return ret;
}

static void 
export_node( mol_device_node_t *dn, FILE *file, int ind )
{
	p_property_t *pr;
	char **ns, buf[256];
	int i;

	if( !dn )
		return;

	/* drop uninteresting nodes */
	for( ns=&drop_nodes[0]; *ns; ns++ )
		if( !strcmp((char *) node_name(dn), *ns) ) {
			export_node( dn->sibling, file, ind );
			return;
		}
	
	indprint(file, ind, "#\n");
	indprint(file, ind, "# **************** NODE %s ***************\n", node_name(dn) );
	indprint(file, ind, "# %s\n", get_full_name(dn, buf, sizeof(buf)) );
	indprint(file, ind, "#\n");
	indprint(file, ind, "{\n");
	ind += INDENT_VALUE;

	if( dn->unit_string && strlen( dn->unit_string ) )
		indprint(file, ind, "(unit_string %s )\n", dn->unit_string );

	if( (ulong)dn->node > PHANDLE_BASE )
		indprint(file, ind, "(mol_phandle 0x%08x )\n",(ulong)dn->node );
	
	/* properties */
	for( pr=dn->properties; pr; pr=pr->next ) {
		char **dp;

		/* drop uninteresting properties */
		for( dp=&drop_properties[0]; *dp; dp++ )
			if( pr->name && !strcmp(pr->name, *dp) )
				break;
		if( *dp )
			continue;

		/* skip all AAPL properties - probably a side effect of BootX 
		 * (hrm... some AAPL-properties seems to be non-generated)
		 */
		//if( pr->name && !strncmp(pr->name, "AAPL", 4) )
		//	continue;

		indprint(file, ind, "(property %-18s ", pr->name );

		if( !pr->length ) {
			fprintf(file, ")\n");
			continue;
		}

		/* detect likely strings/string arrays */
		for( i=0; i<pr->length ; i++ ) {
			char ch = pr->value[i];
			if( !ch && i && !pr->value[i-1] && i!=pr->length-1 )
				break;
			if( ch && !isgraph(ch) && !isspace(ch) )
				break;
		}
		if( i==pr->length && !pr->value[i-1] ) {
			fprintf(file, "<str>  \t");
			ind += INDENT_VALUE;
			for( i=0; i<pr->length; i += strlen((char *) &pr->value[i]) + 1 ) {
				if( i )
					fprintf(file, ", ");
				if( pr->length > 50 ) {
					fprintf(file, "\n");
					indprint(file, ind, "" );
				}
				fprintf(file, "\"%s\"", &pr->value[i] );
			}
			fprintf(file, " )\n");
			ind -= INDENT_VALUE;
			continue;
		}

		/* print long words */
		if( !(pr->length % 4) ) {
			int nw = 4;
			if( !(pr->length % (4*5)) )
				nw=5;
			else if( !(pr->length % (4*6)) )
				nw = 6;
			else if( (pr->length % (4*4)) )
				nw=2;
			fprintf(file, "<word> \t" );
			ind += INDENT_VALUE;
			for( i=0; i<pr->length; i+=4 ) {
				if( pr->length > 8 && !((i/4)%nw) ) {
					fprintf(file, "\n");
					indprint(file, ind, "");
				}
				fprintf(file, "0x%08lx ",*(ulong*)&pr->value[i] );
			}
			fprintf(file, " )\n" );
			ind -= INDENT_VALUE;
			continue;
		}

		/* assume byte */
		fprintf(file, "<byte> \t" );
		ind += INDENT_VALUE;
		for( i=0; i<pr->length; i++ ) {
			if( !(i%12) && pr->length > 8 ) {
				fprintf(file, "\n");
				indprint(file, ind, "");
			}
			fprintf(file, "0x%02x ", pr->value[i]);
		}
		ind -= INDENT_VALUE;
		fprintf(file, " )\n" );
	}

	if( dn->child ) {
		indprint(file, ind, "\n");
		export_node( dn->child, file, ind );
	}
	ind -= INDENT_VALUE;
	indprint(file, ind, "}\n" );

	export_node( dn->sibling, file, ind );
}

static int __dcmd
cmd_ofexport( int argc, char **argv )
{
	mol_device_node_t *root = prom_get_root();
	FILE *file;
	
	if( argc!=2 )
		return 1;

	file = fopen( argv[1], "w" );
	if( !file ){
		printm("Could not create file '%s'\n", argv[1] );
		return 0;
	}
	indprint(file, 0, "# *************************\n");	
	indprint(file, 0, "#  Exported OF Device Tree \n");
	indprint(file, 0, "# *************************\n\n");
	export_node( root, file, 0 );
	fclose( file );
	return 0;
}

static int __dcmd
cmd_showirqs( int argc, char **argv )
{
	if( argc != 1 )
		return 1;
	check_irq_tree(1);
	return 0;
}


/************************************************************************/
/*	oftree parsing							*/
/************************************************************************/

static char *
hex_format( char *str, int *retlen, int refcon )
{
	int add, len=0, v;
	char *str2;

	if( (str2=strchr( str, '(' )) )         /* compatibility */
		str = str2+1;
	
	for( ;; ) {
		if( sscanf(str, " 0x%x%n\n", &v, &add) <= 0 )
			if( sscanf(str, " %d%n\n", &v, &add) <= 0 )
				break;
		str += add;
		if( refcon == 1 ) {
			unsigned char vch = v;
			obstack_grow( &oftree.stack, &vch, 1 );
		} else {
			obstack_grow( &oftree.stack, &v, 4 );
		}
		len += refcon;
	}
	*retlen = len;
	return len ? obstack_finish( &oftree.stack ) : NULL;
}

static char *
string_format( char *str, int *retlen, int refcon )
{
	int on=0, len=0, add;
	char ch;

	while( sscanf(str,"%c%n\n", &ch, &add) > 0 ) {
		str+=add;
		if( ch == '"' ) {
			on=!on;
			continue;
		}
		if( on ) {
 			obstack_1grow( &oftree.stack, ch );
			len++;
		} else if( ch == ',' ){
			obstack_1grow( &oftree.stack, 0 );
			len++;
		}
	}
	obstack_1grow( &oftree.stack, 0 );
	len++;
	
	*retlen = len;
	return obstack_finish( &oftree.stack );
}

static char *
empty_format( char *str, int *retlen, int refcon )
{
	*retlen = 0;
	return NULL;
}

typedef char *format_proc( char *str, int *retlen, int refcon );
typedef struct {
	char 		*key;
	format_proc	*proc;
	int		refcon;
} format_entry;

static format_entry format_table[]  = {
	{ "<byte>",	hex_format, 1  },
	{ "<word>",	hex_format, 4  },
	{ "<str>",	string_format, 0 },
	{ ")",		empty_format, 0 },
	{ NULL, NULL }
};

/* Find second parenthesis of (..(...)) expression.
 * Assumes buf[0] == '('. Returns pointer to ')'.
 */
static char *
balance( char *buf ) 
{
	int count=1;
	char *p;
	
	if( !buf || buf[0] !='(' || !buf[0] )
		return NULL;
	p=buf;
	while( count && (p=strpbrk(p+1, "()")) )
		count += (*p == '(')? 1 : -1;
	return p;
}

/*
 * Find key expression, that is an expression of the form (name xxxxx).
 * Returns 1 if successful. Sets start to first data, and end to the closing
 * parenthesis.
 */
static int 
findkey( char *buf, char *key, char **start, char **end ) 
{
	char *savestart = buf;
	int comment = 1;
	char *p, *p2;

	for( p=buf; p; p=balance(p) ) {
		if( !(p=strchr( p, '(' )) )
			break;

		comment = 0;
		for( p2 = p-1; p2 >= savestart; p2-- ){
			if( isblank(*p2) )
				continue;
			if( *p2 == '\n' || *p2 == '\r' )
				break;
			/* commented line */
			comment++;
			break;
		}
		if( comment )
			continue;

		p2 = p+1;
		while( isspace(*p2) )
			p2++;
     		if( !strncmp(p2, key, strlen(key)) ) {
			if( start )
				*start = p2 + strlen(key);
			p = balance( p );
			if( end )
				*end = p;
			return p ? 1 : 0;
		}
	}
	return 0;
}

/*
 * Build node. Buf contains an the description of this node (without the
 * enclosing brackets and without any child nodes).
 */
static mol_device_node_t *
build_node( char *buf )
{
	char tmpbuf[100], tmpbuf2[40];
	p_property_t **prop_ptr;
	mol_device_node_t *dn;
	char *start, *end;

	union {
		void *	v;
		int *	i;
	} node_num;

	dn = obstack_alloc( &oftree.stack, sizeof(mol_device_node_t) );
	memset( dn, 0, sizeof(mol_device_node_t) );

	/* unit_string */
	tmpbuf[0] = 0;
	if( findkey( buf, "unit_string", &start, NULL )) {
		sscanf( start, "%99s", tmpbuf );
		dn->unit_string = obstack_copy( &oftree.stack, tmpbuf, strlen(tmpbuf)+1 );
	}

	/* phandle field -- used internally by MOL as node identifier */
	if( findkey( buf, "mol_phandle", &start, NULL )) {
		node_num.v = &dn->node;
		sscanf( start, "%x", node_num.i);
		if( (ulong)dn->node < PHANDLE_BASE ) {
			printm("-----> molrc: mol_phandle field 0x%lX < 0x%x\n", 
			       (ulong)dn->node, PHANDLE_BASE );
			dn->node = 0;
		}
	}
	if( !dn->node )
		dn->node = (p_phandle_t)oftree.next_phandle++;
	
	/* properties */
	prop_ptr = &dn->properties;
	while( findkey(buf,"property", &start, &end) ) {
		p_property_t *pr;
		format_entry *fe;
		
		/* prepare for next property */
		buf = end;

		/* read name and data format */
		tmpbuf[0] = tmpbuf2[0]=0;
		sscanf( start, "%99s %39s", tmpbuf, tmpbuf2 );

		/* allocate and insert property */
		pr = obstack_alloc( &oftree.stack,sizeof( p_property_t ) );
		memset(pr, 0, sizeof(p_property_t) );
		*prop_ptr = pr;
		prop_ptr = &pr->next;
		
		pr->name = obstack_copy( &oftree.stack, tmpbuf, strlen(tmpbuf)+1 );

		for( fe = format_table; fe->key ; fe++ ) {
			if( strcmp( tmpbuf2, fe->key ) )
				continue;
			start = strchr( start, '>' ) + 1;
			*end = 0;
			pr->value = (unsigned char *)((*fe->proc)( start, &pr->length, fe->refcon ));
			*end = ')';
			break;
		}
		if( !fe->key )
			printm("WARNING: Data format %s unimplemented\n", tmpbuf2 );
	}

	return dn;
}

/*
 * The format of a node is as follows:
 *   { 
 *	...node definitions...
 *	{ child node 1 }
 *	{ child node 2 }
 *	...
 *   }
 */
static mol_device_node_t *
read_node( char **rbuf, mol_device_node_t *parent ) 
{
	char *buf, *child, *start, *end;
	mol_device_node_t **next_sib;
	mol_device_node_t *retsib=NULL;

	buf = *rbuf;
	next_sib = &retsib;

	for(;;) {
		mol_device_node_t	*dn;

		if( !(start=strstr( buf, "{" )) )
			break;
		if( !(end=strstr( buf, "}" )) ) {
			printm("Parse error: Missing '{'\n");
			break;
		}

		/* end this hierarchy level if '}' precede '{' */
		if( start > end ) {
			buf = end+1;
			break;
		}

		/* replace next '}' and any child marker '{' with 0 */
		*end = 0;
		if( (child=strstr( start+1, "{")) )
			*child = 0;

		dn = build_node( start+1 );

		dn->parent = parent;
		*end++ = '}';

		/* read children */
		if( child ){
			*child = '{';
			end = child;
			dn->child = read_node( &end, dn );  /* child */
		}

		dn->allnext = oftree.allnext;
		oftree.allnext = dn;

		/* prepare for next sibling */
		*next_sib = dn;
		next_sib = &dn->sibling;		/* sibling */
		buf = end;
	}
	*rbuf = buf;
	return retsib;
}


/************************************************************************/
/*	oftree import							*/
/************************************************************************/

static mol_device_node_t *
import_node( const char *filename, mol_device_node_t *par )
{
	mol_device_node_t *node;
	char *buf, *rbuf;
	int fd;
	off_t size;
	
	if( (fd=open(filename, O_RDONLY)) == -1 ) {
		printm("Failed opening '%s'\n", filename );
		return NULL;
	}
	size = lseek( fd, 0, SEEK_END );
	lseek( fd, 0, SEEK_SET );

	if( !(buf=malloc( size+1 )) ) {
		close(fd);
		return NULL;
	}
	
	if(read( fd, (void*)buf, size ) < size) {
		free(buf);
		close(fd);
		return NULL;
	}

	buf[size]=0;
	
	rbuf = buf;
	node = read_node( &rbuf, NULL );

	free(buf);
	close( fd );

	if( par && node ) {
		mol_device_node_t *last_sib;
		for( last_sib = node; last_sib->sibling; last_sib = last_sib->sibling )
			;
		last_sib->sibling = par->child;
		par->child = node;
		node->parent = par;
	}
     	return node;
}

static int
import_oftree( char *filename )
{
	oftree.allnext = NULL;
	oftree.root = import_node( filename, NULL );
	return oftree.root ? 0:1;
}

mol_device_node_t *
prom_import_node( mol_device_node_t *par, const char *filename )
{
	mol_device_node_t *dn;

	if( !par ) {
		printm("import_node: NULL parent\n");
		return NULL;
	}
	dn = import_node( filename, par);
	return dn;
}


/************************************************************************/
/*	OSI interface							*/
/************************************************************************/

#define to_ph	prom_dn_to_phandle
#define to_dn	prom_phandle_to_dn

static int
osip_prom_iface( int sel, int *args )
{
	int len, ph = args[1];
	mol_device_node_t *dn;
	char *p, *buf;
	char tmpbuf[256];
	unsigned char *pdata;
	
	if( args[0] == kPromClose ) {
		if( !debugger_enabled() )
			promif_cleanup();
		return 0;
	}
	
	if( !(dn=to_dn(ph)) && (args[0] != kPromPeer || ph) )
		return -1;
	
	switch( args[0] ) {
	case kPromPeer:
		if( !ph )
			return to_ph( prom_get_root() );
		return dn ? to_ph( dn->sibling ) : -1;
	case kPromChild:
		return to_ph( dn->child );
	case kPromParent:
		return to_ph( dn->parent );

	case kPromPackageToPath: /* ph, buf, size */
		if( !(p=transl_mphys(args[2])) )
			return -1;
		strncpy( p, get_full_name(dn,tmpbuf,sizeof(tmpbuf)), args[3] );
		return strlen( tmpbuf );

	case kPromGetPropLen: /* ph, name */
		if( !(p=transl_mphys(args[2])) )
			return -1;
		return prom_get_property(dn, p, &len) ? len : -1;

	case kPromGetProp: /* ph, name, buf, size */
		p = transl_mphys( args[2] );
		buf = transl_mphys( args[3] );
		if( !p || !buf )
			return -1;
		if( !(pdata=prom_get_property(dn, p, &len)) || args[4] < 0 )
			return -1;
		memcpy( buf, pdata, MIN(len,args[4]) );
		return len;

	case kPromNextProp: /* ph, prev, buf */
		p = transl_mphys( args[2] );
		buf = transl_mphys( args[3] );
		if( !p || !buf )
			return -1;
		if( !(p=prom_next_property(dn, p)) )
			return 0;
		strcpy( buf, p );
		return 1;

	case kPromSetProp: /* ph, name, buf, len */
		p = transl_mphys( args[2] );
		buf = transl_mphys( args[3] );
		if( !p || !buf || args[4] < 0 )
			return -1;
		if( args[4] > 0x1000 ) {
			printm("kPromSetProp: limit exceeded\n");
			return -1;
		}
		prom_add_property( dn, p, buf, args[4] );
		return args[4];

	case kPromChangePHandle: /* old_ph, new_ph */
		if( !prom_phandle_to_dn(args[2]) )
			dn->node = (void*)args[2];
		else if( dn->node != (void*)args[2] ) {
			printm("duplicate phandle\n");
			return -1;
		}
		return 0;
	default:
		printm("bad selector\n");
		return -1;
	}
	return -1;
}

static int
osip_prom_path_iface( int sel, int *args )
{
	char *path = transl_mphys( args[1] );
	int ret;
	
	if( !args[1] || !path )
		return -1;

	switch( args[0] ) {
	case kPromCreateNode:
		ret = to_ph( prom_create_node(path) );
		return (!ret)? -1 : ret;
	case kPromFindDevice:
		ret = to_ph( prom_find_dev_by_path(path) );
		return (!ret)? -1 : ret;
	default:
		printm("bad selector\n");
	}
	return -1;
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

void 
promif_init( void ) 
{
	obstack_init( &oftree.stack );
	oftree.next_phandle = 1;
	oftree.initialized = 1;

	/* load em_tree (emulation tree from file) */
	if( is_oldworld_boot() )
		printm("OF device tree: %s\n", gPE.oftree_filename);
	if( import_oftree(gPE.oftree_filename) )
		fatal("---> Import of OF device tree failed\n");

	check_irq_tree(0);

	os_interface_add_proc( OSI_PROM_IFACE, osip_prom_iface );
	os_interface_add_proc( OSI_PROM_PATH_IFACE, osip_prom_path_iface );

	add_cmd("ofexport", "ofexport filename\nDump OF device tree to file\n",-1, cmd_ofexport );
	add_cmd("showirqs", "show_irqs\nShow OF interrupts\n",-1, cmd_showirqs );
}

/* promif_cleanup might be called from the OSI interface (i.e. before MOL exits) */
void 
promif_cleanup( void ) 
{
	if( !oftree.initialized )
		return;
	
	os_interface_remove_proc( OSI_PROM_IFACE );
	os_interface_remove_proc( OSI_PROM_PATH_IFACE );

	obstack_free( &oftree.stack, NULL );
	memset( &oftree, 0, sizeof(oftree) );
}
