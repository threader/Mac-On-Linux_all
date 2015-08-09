/* 
 *   Creation Date: <97/07/01 12:49:47 samuel>
 *   Time-stamp: <2004/02/22 14:19:48 samuel>
 *   
 *	<breakpoints.c>
 *	
 *	Handles a table with breakpoints
 *   
 *   Copyright (C) 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include "mol_assert.h"

#include "extralib.h"
#include "molcpu.h"
#include "memory.h"
#include "breakpoints.h"
#include "constants.h"
#include "mmu_contexts.h"

#define TABLE_SIZE	10

typedef struct {
	ulong 	addr;
	int	context;	/* context of breakpoint */
	ulong	*lvptr;		/* used only for linux br_ram */
	int	type;
	int	flags;		/* misc flags */
	int	data;		/* breakpoint specific data */
	ulong	saved_instr;	/* original instruction */
} breakpoint_t;

enum { /* breakpoint.type */
	br_nop=0, br_ram=1, br_rom=2, br_unmapped=3
};

static breakpoint_t 	*br_table;
static int 		table_size;
static int		num_bp;
static int		bp_written;



/************************************************************************/
/*	add/remove breakpoint						*/
/************************************************************************/

static void 
flush_caches( ulong *lvptr )
{
	asm volatile("dcbst 0,%0" :: "r" (lvptr) );
	asm volatile("sync" ::);
	asm volatile("icbi 0,%0" :: "r" (lvptr) );
	asm volatile("sync" ::);
	asm volatile("isync" ::);
}

static int 
remove_breakpoint( ulong addr ) 
{
	int i,ind;

	assert( !bp_written );
	
	for( i=0; i<num_bp; i++ ) {
		if( br_table[i].addr == addr )
			break;
		if( br_table[i].type == br_ram && (ulong)br_table[i].lvptr == addr )
			break;
	}
	
	if( i>=num_bp )
		return 1;  /* breakpoint not found */

	ind = i;
	switch( br_table[ind].type ) {
	case br_ram:
		/* The breakpoint is probably not in the ram 
		 * at the moment, but we check for it anyway 
		 */
		if( *br_table[ind].lvptr == BREAKPOINT_OPCODE ) {
			*br_table[ind].lvptr = br_table[ind].saved_instr;
			flush_caches( br_table[ind].lvptr );
		}
		break;
	case br_rom:
	case br_unmapped:
	case br_m68k:
		break;
	default:
		printm("remove_breakpoint: Unknown breakpoint type\n");
	}

	/* contract table */
	for(i=ind+1; i<num_bp; i++)
		br_table[i-1] = br_table[i];
	num_bp--;

	return 0;
}

int 
add_breakpoint( ulong addr, int flags, int data )
{
	breakpoint_t *b;
	int i;

	/* printm("add_breakpoint: addr %08lx, flags %x, data %d\n", addr, flags, data ); */
	
	restore_breakpoints();

	/* make sure it is aligned... */
	addr &= ( flags & br_m68k )? ~0x1 : ~0x3;

	/* remove old breakpoint with same target addr */
	for( i=0; i<num_bp; i++ ) {
		if( addr==br_table[i].addr  ) {
			/* remove it properly to restore memory contents */
			remove_breakpoint( addr );
			break;
		}
	}

	/* resize table if needed */
	if( num_bp >= table_size ) {
		breakpoint_t *new_table;

		/* double table size */
		new_table = malloc( table_size*2*sizeof(breakpoint_t) );
		for(i=0; i<num_bp; i++)
			new_table[i] = br_table[i];
		free( br_table );
		br_table = new_table;
		table_size *= 2;
	}
	
	/* insert breakpoint in table */
	b = &br_table[num_bp];
	b->addr = addr;
	b->context = get_inst_context();
	b->lvptr = 0;
	b->type = br_unmapped;
	b->data = 0;
	b->flags = 0;
	b->saved_instr = 0xDEADBBEF;		/* debugging number */

	if( flags & br_phys_ea )
		b->context = kContextUnmapped;
	if( flags & br_transl_ea )
		b->context = kContextMapped_S;

	/* Decrementer breakpoint */
	if( flags & br_dec_flag ) {
		b->data = data;
		b->flags |= br_dec_flag;
	}

	/* Mac 68k breakpoint */
	if( flags & br_m68k ) {
		b->type = br_m68k;
		if( flags & br_rfi_flag )
			b->flags |= br_rfi_flag;
	}
	num_bp++;

	setup_breakpoints();
	return 0;
}

static void 
clear_all_breakpoints( void ) 
{
	restore_breakpoints();
	while( num_bp )
		remove_breakpoint( br_table[0].addr );
}


/************************************************************************/
/*	setup/restore breakpoints					*/
/************************************************************************/

void 
setup_breakpoints( void ) 
{
	/* int singlestep=0; */
	breakpoint_t *b;
	char *p;
	int i;

	union {
		ulong ** b;
		char ** c;
	} b_pun;
	
	if( bp_written++ )
		return;
	
	for( b=br_table, i=0; i<num_bp; i++, b++ ) {
		switch( b->type ) {
		case br_rom:
			/* singlestep++; */
			break;

		case br_unmapped:
			/* see if the address is now mapped */
			b_pun.b = &b->lvptr;
			if( ea_to_lvptr(b->addr, b->context, (char **)b_pun.c, 0 /* inst trans */)) {
				mregs->mdbg_ea_break = b->addr &~ 0xfff;
				set_break_flag( BREAK_EA_PAGE );
				/* singlestep++; */
				break;
			}
			p = (char*)b->lvptr;
			if( lvptr_is_ram(p) || (lvptr_is_rom(p) && rom_is_writeable()) )
				b->type = br_ram;
			else {
				b->type = br_rom;
				/* singlestep++; */				
				break;
			}

			/* fall through to br_ram */
		case br_ram:
			b->saved_instr = *b->lvptr;
			*b->lvptr = BREAKPOINT_OPCODE;
			flush_caches( b->lvptr );
			break;
			
		case br_m68k:
			set_break_flag( BREAK_SINGLE_STEP | BREAK_SINGLE_STEP_CONT );
			break;

		default:
			printm("Warning: Unknown brakpoint type!\n");
			break;
		}
	}
}

void 
restore_breakpoints( void )
{
	int i, remove=-1;

	if( !bp_written )
		return;
	bp_written = 0;

	for( i=0; i<num_bp; i++ ) {
		switch( br_table[i].type ) {
		case br_unmapped:
		case br_rom:
		case br_m68k:
			break;
		case br_ram:
			*br_table[i].lvptr = br_table[i].saved_instr;
			flush_caches( br_table[i].lvptr );
			break;
		}

		/* handle decrement breakpoints */
		if( (br_table[i].flags & br_dec_flag) && !br_table[i].data ) {
			remove = i;
			/* best not to remove breakpoints while looping them */
		}
	}

	if( remove>=0 )
		remove_breakpoint( br_table[remove].addr );
}


/************************************************************************/
/*	breakpoint probing						*/
/************************************************************************/

/* this function is intended for the debugger display */ 
int 
is_breakpoint( ulong mvptr, char *identifier )
{
	int i;
	
	for( i=0; i<num_bp; i++ )
		if( br_table[i].addr== mvptr ) {
			if( identifier ) {
				switch( br_table[i].type ) {
				case br_rom:
				case br_unmapped:
					*identifier = '+';
					break;
				case br_ram:
					*identifier = '*';
					break;
				case br_m68k:
					*identifier = '#';
					break;
				default:
					*identifier = '?';
				}
			}
			return 1;
		}

	return 0;
}


/* called in trace mode to detect breakpoints. 0=no breakpoint, 1=break, -1=conditional bp */
int 
is_stop_breakpoint( ulong mvptr ) 
{
	int i;
	
	for( i=0; i<num_bp; i++ ) {
		/* 
		 * 68k breakpoints: The 68k emulator uses a 128k jump table for 
		 * the first 16 bits of the instruction beeing emulated.
		 * The pc of the started instruction equals r24-2.
		 */
		if( br_table[i].type == br_m68k ) {
			if( (mvptr & ~0x7FFFF) != MACOS_68K_JUMP_TABLE )
				continue;
			if( mvptr & 0x7 )
				continue;
			/* printm("Instruction at %08X\n", mregs->gpr[24]-2 ); */
			if( !br_table[i].addr || mregs->gpr[24] -2 == br_table[i].addr ) {
				if( br_table[i].flags & br_rfi_flag ) {
					/* hi byte of SR is in low byte of r25 */
					if( mregs->gpr[25] & 0x07 )	/* break when INT mask = 0 */
						continue;
				}
				if( br_table[i].flags & br_dec_flag )
					br_table[i].data--;
				if( br_table[i].data )
					continue;
				/* breakpoints are removed in restore_breakpoints */
				return 1;
			}
			continue;
		}

		if( br_table[i].addr == mvptr ) {
			/* handle dec breakpoints */
			if( br_table[i].flags & br_dec_flag ) {
				if( br_table[i].data ) 
					br_table[i].data--;
				if( br_table[i].data )
					return -1;
				/* can't remove the breakpoint here - */
				/* must be done in restore_breakpoints */
				/* if it's a physical breakpoint */
			}
			/* breakpoint, do stop */
			return 1;
		}
	}
	return 0;
}


/************************************************************************/
/*	Debugger CMDs							*/
/************************************************************************/

static void 
list_breakpoints( void ) 
{
	int i;
	if(!num_bp) {
		printm("No breakpoints\n");
		return;
	}
	
	for( i=0; i<num_bp; i++ ) {
		printm("%08lx ",br_table[i].addr );
		switch( br_table[i].type ) {
		case br_rom:
			printm("ROM");
			break;
		case br_ram:
			break;
		case br_unmapped:
			printm("*UNMAPPED*");
			break;
		case br_m68k:
			printm("68K");
			break;
		default:
			printm("ERROR: Unknown type!");
		}
		if( br_table[i].flags & br_dec_flag ) {
			printm(" [%d]",br_table[i].data );
		}
		printm("\n");
	}
}


/* clear all breakpoint */
static int __dbg
cmd_brc( int numargs, char **args ) 
{
	if( numargs>1 )
		return 1;

	clear_all_breakpoints();
	printm("All breakpoints cleared\n");

	refresh_debugger();
	return 0;
}

/* remove breakpoint */
static int __dbg
cmd_brm( int numargs, char**args ) 
{
	ulong addr;
	
	if( numargs>2 )
		return 1;

	if( numargs==1 )
		addr = mregs->nip;
	else
		addr = string_to_ulong(args[1]);

	restore_breakpoints();
	if( remove_breakpoint( addr ) )
		printm("No breakpoint at 0x%08lX\n",addr);
	else
		printm("Breakpoint removed\n");
	setup_breakpoints();

	refresh_debugger();
	return 0;
}

/* list breakpoints */
static int __dbg
cmd_brl( int numargs, char**args ) 
{
	if( numargs>1 )
		return 1;

	list_breakpoints();
	return 0;
}

static int __dbg
cmd_bea( int numargs, char**args ) 
{
	if( numargs!=2 )
		return 1;
	mregs->mdbg_ea_break = string_to_ulong(args[1]) &~ 0xfff;
	printm("ea-break at page %08lX\n", mregs->mdbg_ea_break );
	printm("You might want to issue a tlbia...\n");
	set_break_flag( BREAK_EA_PAGE );
	return 0;
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

void 
breakpoints_init( void )
{
	br_table = calloc( TABLE_SIZE, sizeof(breakpoint_t) );
	table_size = TABLE_SIZE;
	num_bp = 0;
	
	add_cmd( "brc", "brc\nclear all breakpoints\n", -1, cmd_brc );
	add_cmd( "brm", "brm [addr] \n remove breakpoint\n", -1, cmd_brm );
	add_cmd( "brl", "brl \nlist breakpoints\n", -1, cmd_brl );
	add_cmd( "bea", "brl \nbreak when PTE matching ea is seen\n", -1, cmd_bea );
}


void 
breakpoints_cleanup( void ) 
{
	clear_all_breakpoints();
	free( br_table );
}

