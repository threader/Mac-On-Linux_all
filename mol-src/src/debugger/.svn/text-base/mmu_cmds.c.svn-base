/* 
 *   Creation Date: <97/07/19 17:20:37 samuel>
 *   Time-stamp: <2004/02/08 17:51:02 samuel>
 *   
 *	<mmu_cmds.c>
 *	
 *	MMU related debugger commands
 *   
 *   Copyright (C) 1997-2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include "molcpu.h"
#include "memory.h"
#include "extralib.h"
#include "mmu_contexts.h"
#include "wrapper.h"
#include "nub.h"
#include "mmutypes.h"
#include "processor.h"

extern int __dbg get_inst_context( void );
extern int __dbg get_data_context( void );
extern int __dbg ea_to_lvptr( ulong ea, int context, char **lvptr, int data_trans );


/************************************************************************/
/*	kernel module interface						*/
/************************************************************************/

int __dbg
get_inst_context( void ) 
{
	int msr = mregs->msr;
	return !(msr & MSR_IR) ? kContextUnmapped : (msr & MSR_PR)? 
		kContextMapped_U : kContextMapped_S;
}

int __dbg
get_data_context( void ) 
{
	int msr = mregs->msr;
	return !(msr & MSR_DR) ? kContextUnmapped : (msr & MSR_PR)? 
		kContextMapped_U : kContextMapped_S;
}

static int __dbg
ea_to_mphys( ulong ea, int context, ulong *ret_mphys, int data_access ) 
{
	dbg_op_params_t pb;
	int ret;

	pb.operation = DBG_OP_TRANSLATE_EA;
	pb.ea = ea;
	pb.context = context;
	pb.param = data_access;

	ret = _debugger_op( &pb );

	if( ret_mphys )
		*ret_mphys = pb.ret.phys;
	return ret;
}

int __dbg
ea_to_lvptr( ulong ea, int context, char **lvptr, int data_access )
{
	ulong mphys;
	if( ea_to_mphys(ea, context, &mphys, data_access) )
		return 1;
	return mphys_to_lvptr(mphys, lvptr) < 0;
}


/************************************************************************/
/*	F U N C T I O N S						*/
/************************************************************************/

static inline int __dbg
_translate_dea( int context, ulong ea, ulong *mphys ) 
{
	return ea_to_mphys( ea, context, mphys, 1 );
}

static inline int __dbg
_translate_iea( int context, ulong ea, ulong *mphys )
{
	return ea_to_mphys( ea, context, mphys, 0 );
}

static int __dbg
_get_PTE( int context, ulong ea, mPTE_t *ret_pte )
{
	dbg_op_params_t pb;
	int ret;

	pb.operation = DBG_OP_GET_PTE;
	pb.ea = ea;
	pb.context = context;

	ret = _debugger_op( &pb );

	if( ret_pte )
		*ret_pte = pb.ret.pte;
	return ret;
}

static int __dbg
_dbg_get_physical_page( ulong lvptr, dbg_page_info_t *pinfo )
{
	dbg_op_params_t pb;
	int ret;
	
	pb.operation = DBG_OP_GET_PHYS_PAGE;
	pb.ea = lvptr;
	ret = _debugger_op( &pb );
	
	if( pinfo )
		*pinfo = pb.ret.page;
	return ret;
}

static void __dbg
_emulate_tlbia( void ) 
{
	dbg_op_params_t pb;
	pb.operation = DBG_OP_EMULATE_TLBIA;
	_debugger_op( &pb );
}

static void __dbg
_emulate_tlbie( int ea ) 
{
	dbg_op_params_t pb;
	pb.operation = DBG_OP_EMULATE_TLBIE;
	pb.ea = ea;
	_debugger_op( &pb );
}


/************************************************************************/
/*	helper functions						*/
/************************************************************************/

static int __dbg
context_from_char( char ch ) 
{
	switch( toupper(ch) ) {
	case 'X':	/* unmapped */
		return kContextUnmapped;
	case 'S':	/* supervisor mode, mapped */
		return kContextMapped_S;
	case 'U':	/* user mode, mapped */
		return kContextMapped_U;
	}
	printm("'%c' - No such context!\n",ch );
	return 0;
}

static char __dbg
char_from_context( int context ) 
{
	switch( context ) {
	case kContextUnmapped:
		return 'X';
	case kContextMapped_U:
		return 'U';
	case kContextMapped_S:
		return 'S';
	}
	printm("%d - No such context number!\n",context );
	return 0;
}

static int __dbg
print_pte( ulong ea, int context ) 
{
	mPTE_t pte;
	int num;
	
	num = _get_PTE( context, ea, &pte );
	if( num ) {
		printm("%08lX -> %08X ",ea&0xfffff000, pte.rpn<<12 );

		printm("[%c%c] [%c%c%c] %c%c",
		       pte.r? 'R' : ' ',
		       pte.c? 'C' : ' ',
		       pte.w? 'W' : ' ',
		       pte.i? 'I' : ' ',
		       pte.m? 'M' : ' ',
		       pte.g? 'G' : ' ',
		       pte.h? 'H' : ' ');
		printm(" %02lX",hexbin(pte.pp) );
		printm(" VSID: %08x\n", pte.vsid );
		if( num!=1 ) {
			printm("\nINTERNAL ERROR! %d valid PTE:s exists!\n",num);
		}
		return 0;
	}
	return 1;
}


/************************************************************************/
/*	the commands							*/
/************************************************************************/

static int __dbg
do_cmd_tea( int numargs, char **args, int data_translation )
{
	ulong mac_ea, mphys;
	int context;
	char ch;

	context = get_data_context();
	
	if( numargs!=2 && numargs!=3 )
		return 1;
	if( numargs==3 && !(context = context_from_char(args[2][0])))
		return 1;

	ch = char_from_context( context );
	mac_ea = string_to_ulong(args[1]);

	if( ea_to_mphys(mac_ea, context, &mphys, data_translation) ) {
		printm("No translation found for ea %08lX (%c)\n",mac_ea,ch );
		return 0;
	}
	printm("(%c) %s  EA: %08lX --> %08lX\n", ch,
	       data_translation? "Data" : "Inst", mac_ea, mphys );
	return 0;
}

/* display linux virtual address of ea */
static int __dbg
cmd_itea( int numargs, char **args ) 
{
	return do_cmd_tea( numargs, args, 0 );
}
/* display linux virtual address of ea */
static int __dbg
cmd_tea( int numargs, char **args ) 
{
	return do_cmd_tea( numargs, args, 1 );
}

/* display linux PTE of corresponding to ea */
static int __dbg
cmd_lpte( int numargs, char **args ) 
{
	ulong mac_ea;
	int context;
	char ch;
	
	context = get_data_context();
	
	if( numargs!=2 && numargs!=3 )
		return 1;
	if( numargs==3 && !(context = context_from_char(args[2][0])))
		return 1;

	mac_ea = string_to_ulong(args[1]);

	ch = char_from_context(context);
	if(!ch)
		return 0;
	printm("%c: ",ch );
	if( print_pte( mac_ea, context ) )
		printm("No translation found\n");
	return 0;
}

/* display linux PTE of corresponding to ea */
static int __dbg
cmd_lpter( int numargs, char **args ) 
{
	ulong ea, start, end, oldea;
	int skip_printed=0, context, segindex=0;
	char ch;
	
	context = get_data_context();
	start = 0;
	end = 0xffffffff;
	
	if( numargs<1 || numargs>4 )
		return 1;

	if( numargs==2 )	/* lpter XSU */
		segindex = 1;
	if( numargs==3 || numargs==4 ) {   /* lpter start end */
		start = string_to_ulong(args[1]);
		end = string_to_ulong(args[2]);		
	}
	if( numargs==4 )
		segindex = 3;
	
	if( segindex && !(context = context_from_char(args[segindex][0])))
		return 1;

	ch = char_from_context(context);
	if(!ch)
		return 0;
	printm("HASH TABLE: (%c)\n",ch );

	oldea = start;
	for( ea=start ; ea<=end && oldea <=ea ; ea+=0x1000 ) {
		if( print_pte( ea, context ) ) {
			if( !(skip_printed % 0x10000 ))
				printm("...\n");
			skip_printed++;
		} else {
			skip_printed=0;
		}
		oldea = ea;
	}
	return 0;
}

/* translate range, (ea to linux virtual) */
static int __dbg
do_cmd_tear( int numargs, char **args, int data_access ) 
{
	ulong ea, ea2, mphys, start=0, end=0xfffff000;
	int run, dots, segindex=0, context=get_data_context();
	char ch;

	switch( numargs ) {
	case 1:
		break;
	case 2:
		segindex = 1;
		break;
	case 4:
		segindex = 3;
		/* fall through */
	case 3:
		start = string_to_ulong(args[1]) & ~0xfff;
		end = string_to_ulong(args[2]) & ~0xfff;
		break;
	default:
		return 1;
	}
	if( segindex && !(context = context_from_char(args[segindex][0])))
		return 1;

	if( context == kContextUnmapped )
		context = kContextMapped_S;

	if( !(ch = char_from_context(context) ))
		return 1;

	printm("%s ", data_access ? "Data" : "Inst" );
	printm("effective address -> Mac physical: (%c)\n",ch );
	for( dots=0, run=1, ea=start; run ; ea+=0x1000 ) {
		if( ea == end )
			run = 0;
		if( ea_to_mphys(ea, context, &mphys, data_access) ) {
			if( !(ea & 0x0fffffff) ) {
				printm("...\n");
				dots=0;
			} else if( dots==1 )
				dots=2;
			continue;
		}
		/* find end of block... */
		for( ea2=ea+0x1000 ; ea2!=end+0x1000; ea2+=0x1000 ) {
			ulong mphys2;
			if( !(ea2 & 0x0fffffff) )
				break;
			if( ea_to_mphys(ea2, context, &mphys2, data_access) )
				break;
			if( mphys2 != mphys + ea2-ea )
				break;
		}
		if( dots==2 )
			printm("...\n");
		dots=1;
		printm("EA: %08lX - %08lX  -->  MPHYS: %08lX - %08lX\n",
		       ea,ea2-1,  mphys, mphys+ea2-ea-1 );
		ea=ea2-0x1000;
		if( ea == end )
			run = 0;
	}
	return 0;
}

static int __dbg
cmd_tear( int numargs, char **args )
{
	return do_cmd_tear( numargs, args, 1 );
}

static int __dbg
cmd_itear( int numargs, char **args )
{
	return do_cmd_tear( numargs, args, 0 );
}

/* translate ea to physical address */
static int __dbg
cmd_eatop( int numargs, char **args ) 
{
	int not_found=0, context=get_data_context();
	ulong mac_ea, mphys;
	dbg_page_info_t page;
	char ch, *lvptr;
	
	if( numargs!=2 && numargs!=3 )
		return 1;
	if( numargs==3 && !(context=context_from_char(args[2][0])))
		return 1;

	mac_ea = string_to_ulong(args[1]);

	if( !(ch=char_from_context(context)) )
		return 0;

	if( ea_to_mphys(mac_ea, context, &mphys, 1 /*data*/) ) {
		printm("DATA EA %08lX is unmapped\n", mac_ea );
		not_found=1;
	}
	if( ea_to_mphys(mac_ea, context, &mphys, 0 /*inst*/) ){
		printm("INST EA %08lX is unmapped\n", mac_ea );
		not_found=1;
	}
	if( not_found )
		return 0;
	if( mphys_to_lvptr(mphys, &lvptr) < 0) {
		printm("MPHYS %08lX is not within RAM/ROM\n",mphys);
		return 0;
	}

	if( _dbg_get_physical_page((ulong)lvptr, &page) )
		return 0;
	printm("(%c) EA: %08lX   LV: %08lX  PPN: ", ch,
	       mac_ea&0xfffff000, (ulong)lvptr&0xfffff000 );

	if( page.mflags & M_PAGE_PRESENT )
		printm("%08X\n", page.phys );
	else
		printm("unmapped\n");
	
	return 0;
}


/* translate lv to physical address */
static int __dbg
cmd_lvtop( int numargs, char **args ) 
{
	int flags, context=get_data_context();
	dbg_page_info_t page;
	char *lvptr;

	if( numargs!=2 && numargs!=3 )
		return 1;
	if( numargs==3 && !(context = context_from_char(args[2][0])))
		return 1;

	lvptr = (char*)string_to_ulong(args[1]);

	/* we do a physical read to ensure the pages are present */
	if( lvptr >= ram.lvbase && lvptr < ram.lvbase + ram.size ) {
		char tmp = *((volatile char*)lvptr);
		tmp++;	/* just to avoid GCC warnings... */
	}

	if( _dbg_get_physical_page((ulong)lvptr, &page) ) {
		printm("No linux page found\n");
		return 0;
	}
	flags = page.mflags;
	printm("LV: %08lX  ", (ulong)lvptr & ~0xfff );
	printm( (flags & M_PAGE_PRESENT)? "[P" : "[p");
	printm( (flags & M_PAGE_HASHPTE)? "H": "h");
	printm( (flags & M_PAGE_USER)? " U" : " u");
	printm( (flags & M_PAGE_RW)? "W" : "w");
	printm( (flags & M_PAGE_GUARDED)? "G" : "g");
	printm( (flags & M_PAGE_ACCESSED)? " R" :  " r");
	printm( (flags & M_PAGE_DIRTY)? "C" :  "c");
	printm( (flags & M_PAGE_WRITETHRU)? " W" :  " w");
	printm( (flags & M_PAGE_NO_CACHE)? "I" :  "i");
	printm( (flags & M_PAGE_COHERENT)? "M]" :  "m]");
	printm("  PPN: %08X\n", page.phys );
	
	return 0;
}

static int __dbg
cmd_tlbia( int numargs, char **args ) 
{
	printm("All (S) and (U) hash entries flushed\n");
	_emulate_tlbia();
	return 0;
}

static int __dbg
cmd_tlbie( int numargs, char **args ) 
{
	ulong ea, end;

	if( numargs!=2 && numargs!=3 )
		return 1;

	ea = string_to_ulong( args[1] );
	end = numargs==3 ? string_to_ulong( args[2] ) : ea;
	ea &= ~0xfff;
	end &= ~0xfff;

	if( ea == end )
		printm("Entries with page-index %08lX flushed\n",ea );
	else
		printm("Pages %08lX - %08lX flushed\n", ea, end );
	for( ; ea <= end ; ea += 0x1000 )
		_emulate_tlbie(ea);

	return 0;
}

/* mmu consistency check */
static int __dbg
cmd_mmucc( int numargs, char **args ) 
{
	int i, num, context;
	ulong mac_ea=0;
	dbg_page_info_t page;
	char *lvptr;
	mPTE_t pte;

	context = get_data_context();
	
	if( numargs!=1 && numargs!=2 )
		return 1;
	if( numargs==2 && !(context=context_from_char(args[1][0])))
		return 1;
	
	for( i=0; i<0xfffff; i++ ) {
		mac_ea = i<<12;
		/* ea_to_lvptr first uses the mac-MMU to convert ea to
		 * mphys, then a mphys -> lvptr is performed
		 */
		if( ea_to_lvptr(mac_ea, context, &lvptr, 0) )
			continue;
		/* _dbg_get_physical_page obtains the physical address from linux page tables */
		if( _dbg_get_physical_page((ulong)lvptr, &page) )
			continue;
		/* _get_PTE examines what's actually in the hash table */
		if( (num=_get_PTE(context, mac_ea, &pte)) > 1 )
			printm("Duplicate PTE, CONTEXT: %d, EA: %08lx\n", context, mac_ea);
		if( !num )
			continue;

		if( pte.rpn != (page.phys>>12) ) {
			printm("Inconsistency: EA: %08lx -> LVPTR: %p -> PHYS: %x (flags %x) "
			       "PTE-PHYS: %08x\n",
			       mac_ea, lvptr, page.phys, page.mflags, pte.rpn << 12 );
		}
	}
	return 0;
}

/* write romimage */
static int __dbg
cmd_romwi( int argc, char **argv )
{
	int fd;

	if( argc != 2 )
		return 1;
	if( (fd=open(argv[1], O_RDWR | O_CREAT | O_EXCL, 0644)) == -1 ) {
		perrorm("open");
		return 0;
	}
	if( write(fd, rom.lvbase, rom.size) != rom.size )
		printm("An error occured\n");
	else
		printm("Wrote ROM-image '%s'.\n", argv[1] );

	close(fd);
	return 0;
}


/* write word to memory... XXX: THIS SHOULD BE GENERALIZED */
static int __dbg
cmd_setw( int argc, char **argv )
{
	ulong val, mphys, *lvptr;
	
	if( argc != 3 )
		return -1;

	mphys = string_to_ulong( argv[1] );
	val = string_to_ulong( argv[2] );
	if( !(lvptr=(ulong*)transl_mphys(mphys)) ) {
		printm("MPHYS %08lX is not within RAM\n", mphys);
		return 0;
	}
	*lvptr = val;
	return 0;
}


static void __dbg
do_search( const char *key, char *lvbase, int mbase, int size )
{
	int i, j;

	for( j=0, i=0; i<size ; i++ ) {
		if( key[j] )
			j = (lvbase[i] == key[j])? j+1 : 0;
		else {
			int phys = mbase + i - j;
			unsigned char *p = (unsigned char *)(lvbase + i - j);
			while( *p >= 32 && *p < 127 )
				p--;
			j=0;
			printm("Match at %08x: ", phys );
			printm("%s\n", ++p );
		}
	}
}

/* seach for string */
static int __dbg
cmd_find( int argc, char **argv  )
{
	if( argc != 2 )
		return 1;

	printm("Searching for string '%s'\n", argv[1] );
	do_search( argv[1], ram.lvbase, ram.mbase, ram.size );
	do_search( argv[1], rom.lvbase, rom.mbase, rom.size );
	return 0;
}

/* search for ulong */
static int __dbg
cmd_findl( int argc, char **argv  )
{
	ulong *p1,key;
	int num=0;
	
	if( argc != 2 )
		return 1;
	key = string_to_ulong( argv[1] );
	printm("Seraching for %08lx\n",key );
	
	p1 = (ulong*)ram.lvbase;
	while( (char*)p1 < ram.lvbase+ram.size-3 ){
		if( *p1 == key ) {
			printm("Match at %p\n", 
			       (char*)p1 - (ulong)ram.lvbase + (ulong)ram.mbase );
			if( num++ > 10 )
				break;
		}
		p1=(ulong*)((char*)p1+2);
	}
	return 0;
}

static mPTE_t *
lookup_mpte( ulong vsid, ulong ea )
{
	ulong phash, cmp, pteg, v, mask, hash_mask, *p;
	char *hash_base;
	int i;

	v = mregs->spr[S_SDR1];
	hash_mask = ((v & 0x1ff)<<16) | 0xffff;
	mphys_to_lvptr( (v & ~hash_mask), &hash_base );

	/* we are only interested in the page index */
	ea &= 0x0ffff000;
	mask = hash_mask>>6;

	/* calculate primary hash function */
	phash = (ea >> 12) ^ (vsid & 0x7ffff);
	pteg = ((phash & mask) << 6);

	/* construct compare word */
	cmp = MOL_BIT(0) | (vsid <<7) | ((ea&0x0fffffff)>>22);

	/* look in primary PTEG */
	p=(ulong*)((ulong)hash_base + pteg);
	for(i=0; i<8; i++, p+=2 )
		if( cmp == *p )
			return (mPTE_t*)p;
				
	/* look in secondary PTEG */
	p = (ulong*)( (ulong)hash_base + (pteg ^ (mask << 6)) );
	cmp |= MOL_BIT(25);

	for( i=0; i<8; i++,p+=2 )
		if( cmp == *p )
			return (mPTE_t*)p;
	return NULL;
}

/* display mac PTE of corresponding to ea */
static int __dbg
cmd_mpter( int numargs, char **args ) 
{
	ulong ea, start, end, oldea;
	
	start = 0;
	end = 0xffffffff;
	
	if( numargs!=1 )
		return 1;

	printm("MAC HASH TABLE: \n" );

	oldea = start;
	for( ea=start ; ea<=end && ea >= oldea; ea+=0x1000 ) {
		mPTE_t *pte;
		oldea = ea;
		if( !(pte = lookup_mpte( mregs->segr[ea>>28], ea )) )
			continue;
		printm("EA: %08lX --> MPHYS %08lX   ", ea, (ulong)(pte->rpn << 12) );
		printm("[%c%c] [%c%c%c] %c%c",
		       pte->r? 'R' : ' ',
		       pte->c? 'C' : ' ',
		       pte->w? 'W' : ' ',
		       pte->i? 'I' : ' ',
		       pte->m? 'M' : ' ',
		       pte->g? 'G' : ' ',
		       pte->h? 'H' : ' ');
		printm(" %02lX",hexbin(pte->pp) );
		printm(" VSID: %08x\n", pte->vsid );
	}
	return 0;
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static dbg_cmd_t dbg_cmds[] = {
	{ "tea",	cmd_tea,	"tea ea [XSU] \ntranslate DATA effective address\n"	},
	{ "itea",	cmd_itea,	"itea ea [XSU] \ntranslate INST effective address\n"	},
	{ "tear",	cmd_tear,	"tear start end [SU] \ntranslate DATA range ea->mphys\n" },
	{ "itear",	cmd_itear,	"itear start end [SU] \ntranslate INST range ea->mphys\n" },
	{ "lpte",	cmd_lpte,	"lpte ea [XSU] \ndisplay linux PTE\n"			},
	{ "lpter",	cmd_lpter,	"lpter start end [XSU] \ndisplay linux PTE\n"	 	},
	{ "mpter",	cmd_mpter,	"mpter \ndisplay mac PTEs\n"				},
	{ "eatop",	cmd_eatop,	"eatop ea [XSU] \neffective address to physical\n"	},
	{ "lvtop",	cmd_lvtop,	"lvtop ea \nlinux virtual  address to physical\n"	},
	{ "tlbia",	cmd_tlbia,	"tlbia \nflush linux hash table\n"			},
	{ "tlbie",	cmd_tlbie,	"tlbie ea [end]\nflush entry from hash\n"		},
	{ "mmucc",	cmd_mmucc,	"mmucc [XSU]\ncheck MMU consistency\n"			},
	{ "romwi",	cmd_romwi,	"romwi filename\nWrite ROM-image to file\n"		},
	{ "setw",	cmd_setw,	"setw addr data\n Write data to memory at addr\n"	},
	{ "find",	cmd_find,	"find string \nFind string [in RAM]\n"			},
	{ "findl",	cmd_findl,	"find ulong \nFind value [in RAM]\n"			},
};

void
add_mmu_cmds( void ) 
{
	add_dbg_cmds( dbg_cmds, sizeof(dbg_cmds) );
}
