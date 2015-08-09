/* 
 *   Creation Date: <97/06/18 13:58:23 samuel>
 *   Time-stamp: <2004/02/07 23:41:35 samuel>
 *   
 *	<monitor.c>
 *	
 *	The debugger interface
 *   
 *   Copyright (C) 1997, 1999, 2000, 2001, 2002, 2003, 2004 Samuel Rydh
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include <curses.h>
#include <signal.h>
#include <termios.h>
#include <stdarg.h>
#include <sys/ioctl.h>

#include "monitor.h"
#include "memory.h"
#include "cmdline.h"
#include "symbols.h"
#include "extralib.h"
#include "mmu_contexts.h"
#include "deb.h"
#include "mac_registers.h"
#include "processor.h"
#include "breakpoints.h"

#include "dis-asm.h"

#define		INST_WIN_X		17
#define 	INST_WIN_WIDTH		48
#define 	REG_WIN_WIDTH		INST_WIN_X
#define 	DATA_WIN_X		( INST_WIN_X + INST_WIN_WIDTH )
#define 	DATA_WIN_WIDTH		43
#define 	INP_WIN_HEIGHT_SMALL	6
#define 	INP_WIN_HEIGHT_NEG	10

static void	convert_tab( char *src, char *dest );
static void	draw_inst_win( int scroll );
static void	draw_reg_win( void );
static void	draw_inp_win( void );
static void	draw_data_win( int scroll );
static void	adjust_signal( int sig_num );
static int 	is_endoffunc_inst( ulong prev );
static int 	check_cur_inst( ulong cur );
static int	inst_addr_visible( ulong addr );
static void 	mvwprintb( WINDOW *win, int y, int x, char *form, ... );
static void	print_target_bar( WINDOW *win, int selflag );
static void	update_debugger( int drawflag );
static void 	draw_mem_mode( int scroll );
static void	draw_fpr_mode( void );
static void 	draw_spr_mode( void );
static void	draw_altivec_mode( void );
static void 	draw_unusual_spr_mode( void );
static int 	insn_fprintf_func( PTR dummy, const char *fmt, ... );
static void 	insn_print_addr_func( bfd_vma addr, struct disassemble_info *info );
static int 	print_insn( char *obuf, size_t size, ulong addr, int context, int ppc );
static int 	get_inst_len( ulong pc );
static int 	insn_read_mem_func( bfd_vma memaddr, bfd_byte *dest, unsigned int length, 
				    struct disassemble_info *info );


/* --- static variables --- */

static int	debugger_inited = FALSE;

static WINDOW	*reg_win=0, *inst_win=0, *data_win=0, *inp_win=0;
static int	signal_pending=FALSE;
static int	big_inp_mode;
static int	target;
static int	display_dbats = 0;

enum { target_inst_win=0, target_data_win };

/* --- inst window --- */

typedef struct {
	ulong	addr;
	int	flags;
} inst_win_rec;

enum { inst_symbol=1 };

static char	*inst_top_ptr;
static inst_win_rec *inst_win_cont;
static int 	il_forced_context;	/* context to use, if non-zero */
static int	ppc_mode;
static int	draw_op_hex;

/* --- data window  --- */

enum { mem_mode=0, spr_mode, fpr_mode, unusual_spr_mode, altivec_mode };

/* ---- spr mode  ----*/
static int	data_win_mode;

/* --- data mode ----*/
static char	*mem_top_ptr;
static int	dm_forced_context;	/* context to use, if non-zeor */


/* --- disassembler --- */
static disassemble_info dis_info;



/**************************************************************
*  monitor_init
*    
**************************************************************/

void 
monitor_init( void )
{
	int height, width;

	/* NOTE: initscr() zaps the SIGINT handler (not mention in the man page...) */
	initscr();      	/* initialize the curses library */
	
	keypad(stdscr, TRUE); 	/* keyboard mapping */
	nonl();		    	/* tell curses not to do NL->CR/NL on output */
	cbreak();	       	/* no wait for \n */
	noecho();       	/* don't echo input */
	idlok(stdscr, TRUE);   	/* allow use of insert/delete line */
	timeout(0);		/* nonblocking input */

	/* open windows */
	big_inp_mode = FALSE;
	
	getmaxyx( stdscr, height, width );
	if( width < REG_WIN_WIDTH + INST_WIN_WIDTH + DATA_WIN_WIDTH +1) {
		endwin();
		fprintf(stderr,"Sorry, terminal is too narrow to start debugger\n");
		exit(1);
	}

	reg_win = newwin( height-INP_WIN_HEIGHT_SMALL, REG_WIN_WIDTH,
			  0, 0 );
	inst_win = newwin( height-INP_WIN_HEIGHT_SMALL, INST_WIN_WIDTH,
			   0, INST_WIN_X );
	data_win = newwin( height-INP_WIN_HEIGHT_SMALL,
			   DATA_WIN_WIDTH, 0, DATA_WIN_X );

	wresize( stdscr, INP_WIN_HEIGHT_SMALL,
		 REG_WIN_WIDTH+INST_WIN_WIDTH+DATA_WIN_WIDTH );
	mvwin( stdscr, height-INP_WIN_HEIGHT_SMALL, 0 );
	inp_win = stdscr;

	scrollok( inst_win, TRUE );
	scrollok( data_win, FALSE );

	/* --- inst window ---- */
	inst_top_ptr = (char*)0;
	getmaxyx( inst_win, height, width );
	inst_win_cont = (inst_win_rec*)malloc( height * sizeof(inst_win_rec) );
	il_forced_context = 0;

	/* --- mem window --- */
	mem_top_ptr = (char*)0;
	dm_forced_context = 0;

	/* --- data window --- */
	data_win_mode = spr_mode;
	target = target_inst_win;

	debugger_inited = TRUE;
	
	signal(SIGWINCH, adjust_signal);	/* resize interrupts */

	/* Disassembler. If we upgrade the version of 
	 * ppc-dis.c / m68k-dis.c, we might need to fill in 
	 * some other fields too...
	 */
	memset( &dis_info, 0, sizeof( struct disassemble_info ) );
	dis_info.read_memory_func = insn_read_mem_func;
	dis_info.fprintf_func = insn_fprintf_func;
	dis_info.print_address_func = insn_print_addr_func;

	ppc_mode = 1;
	set_ppc_mode( ppc_mode );
	draw_op_hex = 0;
}


/**************************************************************
*  monitor_cleanup
*    
**************************************************************/

void 
monitor_cleanup( void ) 
{
	if( !debugger_inited )
		return;

	debugger_inited = FALSE;
		
	/* inp_win = stdsrc */
	if( inst_win )
		delwin( inst_win );
	if( data_win )
		delwin( data_win );
	if( reg_win) 
		delwin( reg_win );
	reg_win = data_win = inst_win = 0;
	
	endwin();
}


/**************************************************************
*  deb_getch
*	
**************************************************************/

int
deb_getch( void ) 
{
	int v;
	
	for( ;; ) {
		if( (v = getch()) != ERR )
			break;
		poll_socket();
	}
	return v;
}


/**************************************************************
*  adjust
*	
**************************************************************/

static void
adjust( void ) 
{
	int v,h;
	struct winsize size;

	if (ioctl(fileno(stdout), TIOCGWINSZ, &size) == 0) {
#if NCURSES_VERSION_MAJOR >= 4
		resizeterm(size.ws_row, size.ws_col);
#endif
		if( !big_inp_mode ) {
			wresize( reg_win, size.ws_row-INP_WIN_HEIGHT_SMALL,
				 REG_WIN_WIDTH );
			wresize( inst_win, size.ws_row-INP_WIN_HEIGHT_SMALL,
				 INST_WIN_WIDTH);
			wresize( data_win, size.ws_row-INP_WIN_HEIGHT_SMALL,
				 DATA_WIN_WIDTH);
			wresize( inp_win, INP_WIN_HEIGHT_SMALL,
				 REG_WIN_WIDTH+INST_WIN_WIDTH+DATA_WIN_WIDTH );
			mvwin( inp_win, size.ws_row-INP_WIN_HEIGHT_SMALL, 0 );
		} else {
			wresize( reg_win, INP_WIN_HEIGHT_NEG, REG_WIN_WIDTH );
			wresize( inst_win, INP_WIN_HEIGHT_NEG, INST_WIN_WIDTH);
			wresize( data_win, INP_WIN_HEIGHT_NEG, DATA_WIN_WIDTH);
			mvwin( inp_win, INP_WIN_HEIGHT_NEG, 0 );
			wresize( inp_win,  size.ws_row-INP_WIN_HEIGHT_NEG,
				 REG_WIN_WIDTH+INST_WIN_WIDTH+DATA_WIN_WIDTH );
			}
		free( inst_win_cont );
		getmaxyx( inst_win, v, h );
		inst_win_cont = (inst_win_rec*)malloc( v * sizeof(inst_win_rec) );
		update_debugger(0 /* clear */);
		update_debugger(1 /* update */);
	}
}


static void 
adjust_signal( int sig_num ) 
{
	signal_pending = 1;
}


/**************************************************************
*  debugger
*    This function is the "heart" of the debugger,
*    called after breakpoints etc
**************************************************************/

static void
set_data_win_mode( int new_mode )
{
	data_win_mode = new_mode;
	scrollok( data_win, new_mode == mem_mode );
	werase( data_win );
	draw_data_win(0);
	cmdline_setcursor();
	wrefresh( data_win );
}

int 
mon_debugger( void )
{
	int f, c, ret=-1;
	
	if( !debugger_inited )
		return kDbgGo;

	while( ret<0) {
		c = deb_getch();

		/* handle ESC sequences */
		if( c==27 )
			c = deb_getch() + 128;

		if( signal_pending ) {
			adjust();
			signal_pending = 0;
		}
		switch ( c ) {
		case 12: 	/* ctrl-l */
			refetch_mregs();
			update_debugger(0);	/* clear */
			refresh_debugger();
			//update_debugger(1);	/* update */
			break;
		case 15:	/* ctrl-o */
			refetch_mregs();
			update_debugger(0);
			update_debugger(1);
			break;
		case 24:	/* ctrl-x */
			ret = kDbgStop;
			break;
		case KEY_DOWN:
			if( target == target_data_win && data_win_mode == mem_mode) {
				mem_top_ptr += 8;
				draw_data_win( 1 );
				cmdline_setcursor();
				wrefresh( data_win );
			} else {
				draw_inst_win( 1 );
				cmdline_setcursor();
				wrefresh( inst_win );
			}
			break;
		case KEY_UP:
			if( target == target_data_win && data_win_mode == mem_mode) {
				mem_top_ptr -= 8;
				draw_data_win( -1 );
				cmdline_setcursor();
				wrefresh( data_win );
			} else {
				draw_inst_win( -1 );
				cmdline_setcursor();
				wrefresh( inst_win );
			}
			break;
		case '1'+128:
		case '9'+128:
			set_data_win_mode( spr_mode );
			break;
		case '8'+128:
			set_data_win_mode( fpr_mode );
			break;
		case '7'+128:
			set_data_win_mode( unusual_spr_mode );
			break;
		case '6'+128:
			set_data_win_mode( altivec_mode );
			break;
		case '2'+128:
		case '0'+128:
			set_data_win_mode( mem_mode );
			break;
		case 13+128: /* M-return */
			target = (target == target_data_win)? target_inst_win : target_data_win;
			update_debugger(1);
			break;
		case 32+128: /* M-SPACE */
			big_inp_mode = !big_inp_mode;
			adjust();
			break;
		case 'q'+128: /* M-q */
			if( yn_question("Do you really want to quit? ",1) )
				ret = kDbgExit;
			break;

		case 'd'+128: /* M-d */
			f = (mregs->spr[S_SRR1] & MSR_IR) ? br_transl_ea : br_phys_ea;
			dbg_add_breakpoint( mregs->spr[S_SRR0], br_dec_flag | f, 1 );
			ret = kDbgGo;
			break;

		case 's'+128:
			if( ppc_mode )
				ret = kDbgStep;
			else {
				dbg_add_breakpoint( 0, br_m68k | br_dec_flag, 1 );
				ret = kDbgGo;
			}
			break;
		case 't'+128:
			if( ppc_mode )
				dbg_add_breakpoint( mregs->nip+4, br_dec_flag, 1 );
			else
				dbg_add_breakpoint( get_nip() + get_inst_len(get_nip()), br_m68k | br_dec_flag, 1 );
			ret = kDbgGo;
			break;

		case 'g'+128:
			ret = kDbgGo;
			break;

		case 'l'+128:
			if( ppc_mode ) {
				dbg_add_breakpoint( mregs->link, br_dec_flag, 1 );
				ret = kDbgGo;
			} else {
				printm("Not supported in 68k mode\n");
			}
			break;

		case 'u'+128: /* M-u */
			ret = kDbgGoUser;
			break;

		case 'm'+128:
			if( ppc_mode ) {
				ret = kDbgGoRFI;
			} else {
				dbg_add_breakpoint( 0, br_m68k | br_rfi_flag | br_dec_flag, 1 );
				ret = kDbgGo;
			}
			break;
		case 'y'+128: /* skip */
			set_nip( get_nip() + (ppc_mode? 4 : get_inst_len(get_nip())) );
			update_debugger(1);
			break;
		case 'j'+128:
			ppc_mode = !ppc_mode;
			set_ppc_mode( ppc_mode );
			if( ppc_mode )
				inst_top_ptr = (char*)((ulong) inst_top_ptr & ~0x3);
			update_debugger(1);
			break;
		case 'h'+128:
			draw_op_hex = !draw_op_hex;
			update_debugger(1);
			break;
		case 'z'+128:
			if( ppc_mode )
				break;
			inst_top_ptr += 2;
			update_debugger(1);
			break;
		case 'x'+128:
			if( ppc_mode )
				break;
			inst_top_ptr -= 2;
			update_debugger(1);
			break;
		default:
			cmdline_do_key( c );
		}
	}

	return ret;
}


/**************************************************************
*  update_debugger
*	1 update contents
*	2 clear
**************************************************************/

void
refresh_debugger( void )
{
	if( !inst_addr_visible( get_nip() ) )
		inst_top_ptr = (char*)get_nip();

	update_debugger(1);
}


void 
refresh_debugger_window( void )
{
	if( !debugger_inited )
		return;

	update_debugger( 1 );
}

static void 
update_debugger( int drawflag ) 
{
	
	if( drawflag == 1 ) {
		draw_data_win( 0 );
		draw_reg_win();
		draw_inst_win( 0 );
		draw_inp_win();
	} else {
		werase( data_win );
		werase( reg_win );
		werase( inst_win );
		werase( inp_win );
	}

       	wnoutrefresh( data_win );
	wnoutrefresh( reg_win );
	wnoutrefresh( inst_win );
	wnoutrefresh( inp_win );
	doupdate();

}


/**************************************************************
*  draw_reg_win
*    
**************************************************************/

static void
draw_reg_win( void ) 
{
	int i;

	werase( reg_win );

	if( ppc_mode ){
		mvwprintb(reg_win, 1,2,"LR:**  %08lx",mregs->link );
		mvwprintb(reg_win, 2,2,"CTR:** %08lx",mregs->ctr );
	
		for(i=0; i<32; i++) {
			wattron( reg_win, A_BOLD );
			mvwprintw(reg_win, i+4,2,"R%02d:",i );
			wattroff( reg_win, A_BOLD );
			wprintw(reg_win, " %08lx",mregs->gpr[i] );
		}
		mvwprintb( reg_win, 37, 2, "XER:** %08lx",mregs->xer );
		mvwprintb( reg_win, 38, 4, "%2s %2s %2s", 
			   (mregs->xer & MOL_BIT(0)) ? "SO" : "",
			   (mregs->xer & MOL_BIT(1)) ? "SV" : "",
			   (mregs->xer & MOL_BIT(2)) ? "CA" : "" );
	} else {
		ulong sr;
		/* m68k. NIP = r24-2 */
		mvwprintb(reg_win, 1,2, "NIP:** %08x", mregs->gpr[24]-2 );

		/* 68k, D0-D7 = r8-r15 */
		for(i=0; i<8; i++) {
			wattron( reg_win, A_BOLD );
			mvwprintw(reg_win, i+3,2,"D%d: ",i );
			wattroff( reg_win, A_BOLD );
			wprintw(reg_win, " %08lx",mregs->gpr[i+8] );
		}
		/* A0-A6 = r16-r22, A7=r1 (user sp) */
		for(i=0; i<8; i++) {
			int ind;
			wattron( reg_win, A_BOLD );
			mvwprintw(reg_win, i+12,2,"A%d: ",i );
			wattroff( reg_win, A_BOLD );
			ind = (i==7)? 1 : i+16;
			wprintw(reg_win, " %08lx",mregs->gpr[ind] );
		}
		/* Status register... cr0, xer hold flag bits */
		/* low byte of r25 holds high byte of status register */
		/* T1 T0 S M 0 I3 I2 I1 .... ? */
		sr = (mregs->gpr[25] << 8) & 0xff00;
		sr |= (mregs->cr & MOL_BIT(0) ) ? 0x8 : 0;	/* LT  (N, Negative) */
		sr |= (mregs->cr & MOL_BIT(2) ) ? 0x4 : 0;	/* EQ  (Z, Zero) */
		sr |= (mregs->xer & MOL_BIT(2) ) ? 0x1 : 0;	/* CA  (C, Carry) */
		/* shold we get the SO bit from cr or XER? */
		sr |= (mregs->cr & MOL_BIT(3) ) ? 0x2 : 0 ; /* SO  (V, Overflow) */
		/* XXX: Extended bit X (0x10...) lookup in motorolas manual...*/

		mvwprintb(reg_win, 21,2,"SR:** %04X",sr );
		mvwprintb(reg_win, 23,4,"%s%s%s%s%s",
			  (sr & 0x10) ? "X" : "x",
			  (sr & 0x8)  ? "N" : "n",
			  (sr & 0x4)  ? "Z" : "z",
			  (sr & 0x2)  ? "V" : "v",
			  (sr & 0x1)  ? "C" : "c" );
		mvwprintb(reg_win, 24,4,"            ");
		mvwprintb(reg_win, 24,4,"%s%s%s%s :%d", 
			 (sr & 0x8000) ? "T1 " : "",
			 (sr & 0x4000) ? "T2 " : "",
			 (sr & 0x2000) ? "S" : "s",
			 (sr & 0x1000) ? "M" : "m",
			 (sr & 0x0700) >> 8 );
	}

       	box( reg_win, 0,0 );
}


/**************************************************************
*  redraw_inst_win
*
**************************************************************/

void 
redraw_inst_win( void ) 
{
	/* Should perhaps be made static since 
	 * refresh_debugger_window() does the trick.
	 */

	draw_inst_win(0);
	wrefresh( inst_win );
}


/**************************************************************
*  draw_inst_win
*    
**************************************************************/

/*
 * XXX: scrolling doesn't work very well when disassembling
 * in 68k mode (due to the non-constant instruction len)
 */
static void 
draw_inst_win( int scroll )
{
	char	buf1[50], buf2[80];
       	char	*ptr = inst_top_ptr;
	int	h,v;
	int 	inc, y, i;
	int	symbol_look;
	ulong	cur;
	int	start_y, end_y;
	char	*symbol;
	char	break_char;
	int	attr_inst;
	int	context;

	/* clear_cache(); */

	/* MMU-context to use */
	context = il_forced_context ? il_forced_context : get_inst_context();

	getmaxyx( inst_win, v,h );
	start_y = 1;
	end_y = v-2;

	/* do scrolling preparation stuff */
	inc = 1;
	start_y=1;
	ptr = inst_top_ptr;
	symbol_look = TRUE;

	if( scroll > 0 ) {
		for( y=1+scroll; y<=v-2; y++ )
			inst_win_cont[y-scroll] = inst_win_cont[y];
		start_y = v-1-scroll;
		ptr = (char*) inst_win_cont[v-2-scroll].addr;
		if( inst_win_cont[v-2-scroll].flags & inst_symbol )
			symbol_look = FALSE;
		else
			ptr+=4;
	}
	if( scroll < 0 ) {
		scroll = -scroll;
		for( y=v-2-scroll; y>=1; y-- )
			inst_win_cont[y+scroll] = inst_win_cont[y];
		start_y = scroll;
		inc = -1;
		ptr = (char*) inst_win_cont[1+scroll].addr;
		ptr -= 4;
		if( inst_win_cont[1+scroll].flags & inst_symbol )
			symbol_look = FALSE;
	}
	if( scroll )
		wscrl( inst_win, scroll*inc );

	/* do the line-loop */
	y=start_y;
	while( y>0 && y<v-1 ) {
		int inst_len;

		if( symbol_look ) {
			/* print symbols/labels */
			ulong	symaddr = (inc<0)? (ulong)ptr+4 : (ulong)ptr;
			int	function_start = FALSE;
			
    			symbol = symbol_from_addr( symaddr );
			if( !symbol && ppc_mode) {
				ulong prev;
				prev = readc_ea( (ulong)symaddr-4,context, kInstTrans );

				if( is_endoffunc_inst(prev) )
					function_start = TRUE;
			}
			if( symbol || function_start ) {
				wmove( inst_win, y, 1 );
				wclrtoeol( inst_win );
				wattron( inst_win, A_BOLD );
				if( function_start )
					mvwprintw( inst_win, y,1,"func_%08x:", symaddr );
				else
					mvwprintw( inst_win, y, 1, "%s:", symbol );
				wattroff( inst_win, A_BOLD );

				symbol_look = FALSE;
				inst_win_cont[y].flags = inst_symbol;
				inst_win_cont[y].addr = symaddr;
				y+=inc;
				continue;
			}
			symbol_look = FALSE;
		}

		inst_win_cont[y].flags = 0;
		inst_win_cont[y].addr = (ulong)ptr;
		
		/* no symbol, disassemble */
		cur = readc_ea( (ulong)ptr,context, kInstTrans );

		/* is breakpoint? */
		if( !dbg_is_breakpoint( (ulong)ptr, &break_char ) )
			break_char = 0;

		/* styling stuff */
		attr_inst = 0;
		switch( check_cur_inst( cur ) ) {
		case 1:
			attr_inst = A_UNDERLINE;
			break;
		}
		inst_len = print_insn( buf1, 50, (ulong)ptr, context, ppc_mode );
		convert_tab( buf1,buf2 );

		/* do the printing of an instruction */
		wmove( inst_win, y,1 );
		wclrtoeol( inst_win );
		if( ptr == (char*)get_nip() ) {
			attr_inst = 0;
			wattron( inst_win, A_REVERSE );
			wprintw( inst_win, "                                 ");
		}
		mvwprintw( inst_win, y,1, " %08lx ", ptr );
		wattron( inst_win, attr_inst );
		wprintw( inst_win, "%s", buf2 );
		if( break_char )
			mvwprintw( inst_win, y,1,"%c",break_char );

		wattrset( inst_win, 0 );

		for( i=0; draw_op_hex && i<inst_len && i<4; i++  )
			mvwprintw( inst_win, y,h-13+3*i, "%02lx", (cur>>((3 - i)*8)) & 0xff );
		
		y+=inc;
		ptr += inc * inst_len;
		symbol_look = TRUE;
	}
	box( inst_win,0,0 );
	print_target_bar( inst_win, (target==target_inst_win) );

	inst_top_ptr = (char*) inst_win_cont[1].addr;
}


/**************************************************************
*  inst_addr_visible
*	returns TRUE if instruction with address addr is    
*	visible in inst_win
**************************************************************/

static int 
inst_addr_visible( ulong addr ) 
{
	int v,h;

	getmaxyx( inst_win, v,h );

	if( addr>=inst_win_cont[1].addr && addr<inst_win_cont[v-2].addr )
		return TRUE;
	if( addr == inst_win_cont[v-2].addr
	    && !(inst_win_cont[v-2].flags & inst_symbol) )
		return TRUE;

	return FALSE;
}


/**************************************************************
*  draw_inp_win
*    
**************************************************************/

static void 
draw_inp_win( void )
{
	cmdline_draw();
}


/**************************************************************
*  draw_data_win
*    
**************************************************************/

static void 
draw_data_win( int scroll ) 
{
	switch( data_win_mode ) {
	case mem_mode:
		draw_mem_mode( scroll );
		break;
	case spr_mode:
		draw_spr_mode();
		break;
	case unusual_spr_mode:
		draw_unusual_spr_mode();
		break;
	case fpr_mode:
		draw_fpr_mode();
		break;
	case altivec_mode:
		draw_altivec_mode();
		break;
	}
	
	box( data_win,0,0 );
	print_target_bar( data_win, (target==target_data_win) );
}



/**************************************************************
*  print_target_bar
*    
**************************************************************/

static void
print_target_bar( WINDOW *win, int selflag ) 
{
	int v,h;

	getmaxyx( win, v, h );
	if( selflag) {
		mvwprintw( win, 0,h-12,"[=======]");
	} else {
		mvwprintw( win, 0,h-12,"[       ]");
	}
}



/**************************************************************
*  draw_spr_mode / draw_fpr_mode
*    
**************************************************************/

static void 
draw_fpr_mode( void ) 
{
	int y=1, i;
	mvwprintb( data_win, y++,2,"FPSCR:** %08x", mregs->fpscr );
	y++;
	for(i=0; i<32; i++ ) {
		wattron( data_win, A_BOLD );
 		mvwprintw( data_win, y++, 2, "FP%02d:", i);
		wattroff( data_win, A_BOLD );
		wprintw( data_win, "  %08lX %08lX", mregs->fpr[i].h, mregs->fpr[i].l );
	}
}


#define BIN4(n,s)	hexbin(((n)>>s) & 0xf)
static void 
draw_spr_mode( void ) 
{
	int 	y,i;
	ulong	tmp;

	y=1;
	mvwprintb( data_win, y,2,"MSR:**  %08x",mregs->msr );
	mvwprintb( data_win, y++,20,"PC:**   %08x",mregs->nip);
	wmove( data_win, y, 2 );
	wclrtoeol( data_win );
	wprintw( data_win, " %2s %2s %2s %2s %2s %2s %2s %2s %2s %2s",
		 (mregs->msr & MSR_EE )? "EE" : "",
		 (mregs->msr & MSR_PR )? "PR" : "",
		 (mregs->msr & MSR_BE )? "BE" : "",
		 (mregs->msr & MSR_FP )? "FP" : "",
		 (mregs->msr & MSR_SE )? "SE" : "",
		 (mregs->msr & MSR_ME )? "ME" : "",
		 (mregs->msr & MSR_IP )? "IP" : "",
		 (mregs->msr & MSR_IR )? "IR" : "",
		 (mregs->msr & MSR_DR )? "DR" : "",
		 (mregs->msr & MSR_RI )? "RI" : "");
	wmove( data_win, ++y, 2 );
	wclrtoeol( data_win );
	wprintw( data_win, " %3s %3s %3s %2s %3s %3s",
		 (mregs->msr & MSR_VEC )?"VEC": "",
		 (mregs->msr & MSR_POW )?"POW": "",
		 (mregs->msr & MSR_ILE )?"ILE": "",
		 (mregs->msr & MSR_LE )? "LE" : "",
		 (mregs->msr & MSR_FE0)? "FE0": "",
		 (mregs->msr & MSR_FE1)? "FE1": "");
	y++;

	tmp = mregs->cr;
	mvwprintb( data_win, y++,2,"CR (high):**  %04X %04X %04X %04X",
		BIN4(tmp,28), BIN4(tmp,24), BIN4(tmp,20), BIN4(tmp,16)  );
	mvwprintb( data_win, y++,2,"CR (low):**   %04X %04X %04X %04X",
		BIN4(tmp,12), BIN4(tmp,8), BIN4(tmp,4), BIN4(tmp,0)  );
	wclrtoeol( data_win );
	mvwprintb( data_win, y,2, " %2s %2s %2s %2s     %2s %3s %2s %2s",
		   ( tmp & MOL_BIT(0) )? "LT" : "",
		   ( tmp & MOL_BIT(1) )? "GT" : "",
		   ( tmp & MOL_BIT(2) )? "EQ" : "",
		   ( tmp & MOL_BIT(3) )? "SO" : "",
		   ( tmp & MOL_BIT(4) )? "FX" : "",
		   ( tmp & MOL_BIT(5) )? "FEX": "",
		   ( tmp & MOL_BIT(6) )? "VX" : "",
		   ( tmp & MOL_BIT(7) )? "OV" : "");
	y++;
	mvwprintb( data_win, y, 2,  "XER:**   %08lx", mregs->xer);
	tmp = mregs->xer;
	mvwprintb( data_win, y, 20, "%-3d %2s %2s %2s", tmp & 0x7f,
		   ( tmp & MOL_BIT(0) )? "SO" : "",
		   ( tmp & MOL_BIT(1) )? "OV" : "",
		   ( tmp & MOL_BIT(2) )? "CA" : "" );
	y++;
	mvwprintb( data_win, y, 2, "FPSCR:** %08lx", mregs->fpscr);
	y++;

	y++;
	mvwprintb( data_win, y++, 2, "SRR0:**  %08lx",mregs->spr[S_SRR0] );
	mvwprintb( data_win, y++, 2, "SRR1:**  %08lx",mregs->spr[S_SRR1] );
	mvwprintb( data_win, y++, 2, "DSISR:** %08lx",mregs->spr[S_DSISR] );
	mvwprintb( data_win, y++, 2, "DAR:**   %08lx",mregs->spr[S_DAR] );
	y++;
	mvwprintb( data_win, y++, 2, "SPRG0:** %08lx",mregs->spr[S_SPRG0] );
	mvwprintb( data_win, y++, 2, "SPRG1:** %08lx",mregs->spr[S_SPRG1] );
	mvwprintb( data_win, y++, 2, "SPRG2:** %08lx",mregs->spr[S_SPRG2] );
	mvwprintb( data_win, y++, 2, "SPRG3:** %08lx",mregs->spr[S_SPRG3] );
	mvwprintb( data_win, y++, 2, "EAR:**   %08lx",mregs->spr[S_EAR] );
	mvwprintb( data_win, y++, 2, "PVR:**   %08lx",mregs->spr[S_PVR] );
	mvwprintb( data_win, y++, 2, "DEC:**   %08lx",mregs->spr[S_DEC] );
	mvwprintb( data_win, y++, 2, "TBU:**   %08lx",0xDEADBEEF/*mregs->tb[0]*/ );
	mvwprintb( data_win, y++, 2, "TBL:**   %08lx",0xDEADBEEF/*mregs->tb[1]*/ );
	y++;
	mvwprintb( data_win, y++, 2, "SDR1:**  %08lx",mregs->spr[S_SDR1] );

	y=10;
	mvwprintb( data_win, y++, 20, "HID0:**   %08lx",mregs->spr[S_HID0] );
	mvwprintb( data_win, y++, 20, "IABR:**   %08lx",mregs->spr[S_IABR] );
	mvwprintb( data_win, y++, 20, "ICTRL:**  %08lx",mregs->spr[S_ICTRL] );
	mvwprintb( data_win, y++, 20, "DABR:**   %08lx",mregs->spr[S_DABR] );
	mvwprintb( data_win, y++, 20, "MSSCR0:** %08lx",mregs->spr[S_MSSCR0] );
	mvwprintb( data_win, y++, 20, "MSSCR1:** %08lx",mregs->spr[S_MSSCR1] );
	mvwprintb( data_win, y++, 20, "LDSTCR:** %08lx",mregs->spr[S_LDSTCR] );
	mvwprintb( data_win, y++, 20, "L2CR:**   %08lx",mregs->spr[S_L2CR] );
	mvwprintb( data_win, y++, 20, "L3CR:**   %08lx",mregs->spr[S_L3CR] );
	mvwprintb( data_win, y++, 20, "ICTC:**   %08lx",mregs->spr[S_ICTC] );
	mvwprintb( data_win, y++, 20, "THRM1:**  %08lx",mregs->spr[S_THRM1] );
	mvwprintb( data_win, y++, 20, "THRM2:**  %08lx",mregs->spr[S_THRM2] );
	mvwprintb( data_win, y++, 20, "THRM3:**  %08lx",mregs->spr[S_THRM3] );
	mvwprintb( data_win, y++, 20, "PIR:**    %08lx",mregs->spr[S_PIR] );
	y++;

	y=27;
	if (display_dbats) {
		mvwprintb( data_win, y, 2, "DBAT    upper**");
		mvwprintb( data_win, y++, 20," lower**");
		mvwprintb( data_win, y, 3, "0:**    %08lx",mregs->spr[S_DBAT0U] );
		mvwprintw( data_win, y++, 20, "%08lx",mregs->spr[S_DBAT0L] );
		mvwprintb( data_win, y, 3, "1:**    %08lx",mregs->spr[S_DBAT1U] );
		mvwprintw( data_win, y++, 20, "%08lx",mregs->spr[S_DBAT1L] );
		mvwprintb( data_win, y, 3, "2:**    %08lx",mregs->spr[S_DBAT2U] );
		mvwprintw( data_win, y++, 20, "%08lx",mregs->spr[S_DBAT2L] );
		mvwprintb( data_win, y, 3, "3:**    %08lx",mregs->spr[S_DBAT3U] );
		mvwprintw( data_win, y++, 20, "%08lx",mregs->spr[S_DBAT3L] );
	} else {
		mvwprintb( data_win, y, 2, "IBAT    upper**");
		mvwprintb( data_win, y++, 20," lower**");
		mvwprintb( data_win, y, 3, "0:**    %08lx",mregs->spr[S_IBAT0U] );
		mvwprintw( data_win, y++, 20, "%08lx",mregs->spr[S_IBAT0L] );
		mvwprintb( data_win, y, 3, "1:**    %08lx",mregs->spr[S_IBAT1U] );
		mvwprintw( data_win, y++, 20, "%08lx",mregs->spr[S_IBAT1L] );
		mvwprintb( data_win, y, 3, "2:**    %08lx",mregs->spr[S_IBAT2U] );
		mvwprintw( data_win, y++, 20, "%08lx",mregs->spr[S_IBAT2L] );
		mvwprintb( data_win, y, 3, "3:**    %08lx",mregs->spr[S_IBAT3U] );
		mvwprintw( data_win, y++, 20, "%08lx",mregs->spr[S_IBAT3L] );
	}
	y++;

	mvwprintb( data_win, y, 2, "SEGR:**" );
	mvwprintb( data_win, y++, 17, "SEGR:**" );
	for( i=0; i<8; i++ ) {
		wattron( data_win, A_BOLD );
		mvwprintw( data_win, y, 3, "%X:", i);
		wattroff( data_win, A_BOLD );
		wprintw( data_win, "  %08lx",mregs->segr[i] );
		wattron( data_win, A_BOLD );
		mvwprintw( data_win, y++,18,"%X:", 8+i);
		wattroff( data_win, A_BOLD );
		wprintw( data_win, "  %08lx",mregs->segr[8+i]);
	}
	y++;
}

static void 
draw_unusual_spr_mode( void ) 
{
	int y=1;
	mvwprintb( data_win, y++,2,"MMCR0:**  %08x",mregs->spr[S_MMCR0] );
	mvwprintb( data_win, y++,2,"MMCR1:**  %08x",mregs->spr[S_MMCR1] );
	mvwprintb( data_win, y++,2,"PMC1:**   %08x",mregs->spr[S_PMC1] );
	mvwprintb( data_win, y++,2,"PMC2:**   %08x",mregs->spr[S_PMC2] );
	mvwprintb( data_win, y++,2,"PMC3:**   %08x",mregs->spr[S_PMC3] );
	mvwprintb( data_win, y++,2,"PMC4:**   %08x",mregs->spr[S_PMC4] );
	mvwprintb( data_win, y++,2,"SIAR:**   %08x",mregs->spr[S_SIAR] );
	mvwprintb( data_win, y++,2,"SDAR:**   %08x",mregs->spr[S_SDAR] );
	y++;
	mvwprintb( data_win, y++,2,"THRM1:**  %08x",mregs->spr[S_THRM1] );
	mvwprintb( data_win, y++,2,"THRM2:**  %08x",mregs->spr[S_THRM2] );
	mvwprintb( data_win, y++,2,"THRM3:**  %08x",mregs->spr[S_THRM3] );
	y++;
	mvwprintb( data_win, y++,2,"L2CR:**   %08x",mregs->spr[S_L2CR] );
	y++;
	mvwprintb( data_win, y++,2,"HID0:**   %08x",mregs->spr[S_HID0] );
	mvwprintb( data_win, y++,2,"HID1:**   %08x",mregs->spr[S_HID1] );
	mvwprintb( data_win, y++,2,"HID15:**  %08x",mregs->spr[S_HID15] );
	y++;
	mvwprintb( data_win, y++,2,"IABR:**   %08x (HID2)",mregs->spr[S_HID2] );
	mvwprintb( data_win, y++,2,"DABR:**   %08x (HID5)",mregs->spr[S_HID5] );
	y++;
	mvwprintb( data_win, y++,2,"HASH1:**  %08x (603)",mregs->spr[S_HASH1] );
	mvwprintb( data_win, y++,2,"HASH2:**  %08x (603)",mregs->spr[S_HASH2] );
	mvwprintb( data_win, y++,2,"DMISS:**  %08x (603)",mregs->spr[S_DMISS] );
	mvwprintb( data_win, y++,2,"IMISS:**  %08x (603)",mregs->spr[S_IMISS] );
	mvwprintb( data_win, y++,2,"DCMP:**   %08x (603)",mregs->spr[S_DCMP] );
	mvwprintb( data_win, y++,2,"ICMP:**   %08x (603)",mregs->spr[S_ICMP] );
	mvwprintb( data_win, y++,2,"RPA:**    %08x (603)",mregs->spr[S_RPA] );
}

static void
draw_altivec_mode( void )
{
	int y=1,i;
	
	mvwprintb( data_win, y++,2,"VRSAVE:**  %08x",mregs->spr[S_VRSAVE] );
	mvwprintb( data_win, y++,2,"VSCR:**    %08x  %s %s",mregs->vscr,
		   (mregs->vscr & VSCR_NJ)? "NJ" : "  ",
		   (mregs->vscr & VSCR_SAT)? "SAT" : "   " );
	y++;
	for(i=0;i<32;i++) {
		altivec_reg_t *v = &mregs->vec[i];
		if( MOL_BIT(i) & mregs->spr[S_VRSAVE] )
			wattron( data_win, A_BOLD );
		mvwprintw(data_win, i+4,2,"V%02d", i );
		wattroff( data_win, A_BOLD );
		mvwprintb( data_win, y++,6,"%08x %08x %08lx %08lx", 
			   v->words[0], v->words[1], v->words[2], v->words[3] );
	}
}


/**************************************************************
*  mvprintb
*    
**************************************************************/

static void
mvwprintb( WINDOW *win, int y, int x, char *form, ... ) 
{
	char buf[80];
	char *p;
	int i=0;
	va_list	va;

	va_start( va, form );
	wmove( win, y, x );
	p = form;
	while( *p && i<79 ) {
		if( *p == '*' && *(p+1)=='*' ) {
			memmove( buf, form, i );
			buf[i] = 0;
			wattron( win, A_BOLD );
			wprintw( win, "%s", buf );
			wattroff( win, A_BOLD );
			form = p+2;
			break;
		}
		i++;
		p++;
	}	
	vwprintw( win, form, va );
	va_end( va );
}

	       

/**************************************************************
*  draw_mem_mode( void )
*    
**************************************************************/

static void 
draw_mem_mode( int scroll ) 
{
	int 	i, y,v,h, start_y, end_y;
	ulong	m1,m2;
	int	context;

	/* clear_cache(); */

	/* MMU-context to use for displaying data */
	context = dm_forced_context ? dm_forced_context : get_data_context();

	getmaxyx( data_win, v, h );
	start_y = 1;
	end_y = v-2;
	
	if( scroll ) {
		wscrl( data_win, scroll );
		if( scroll > 0 ) {
			start_y = v-1-scroll;
			end_y = v-2;
		} else {
			start_y = 1;
			end_y = -scroll;
		}
	}

	for( y=start_y; y <= end_y; y++ ) {
		char str[9];

		wmove( data_win, y, 0 );
		wclrtoeol( data_win );
		
		m1 = readc_ea( (ulong)mem_top_ptr + (y-1)*8, context, kDataTrans  );
		m2 = readc_ea( (ulong)mem_top_ptr + (y-1)*8+4, context, kDataTrans );

		mvwprintw( data_win, y,2, "%08x: %04X %04X %04X %04X ",
			   mem_top_ptr+(y-1)*8, m1>>16, m1&0xffff, m2>>16, m2&0xffff  );
		str[8] = 0;
		*(long*)(&str[0]) = m1;
		*(long*)(&str[4]) = m2;
		for( i=0; i<8; i++ ) {
			if( !isprint(str[i] ))
			    str[i]='.';
		}
		wprintw( data_win, "%s", str );
	}
}


/**************************************************************
*  is_endoffunc_inst
*    
**************************************************************/

static int 
is_endoffunc_inst( ulong inst ) 
{
	ulong opc = OPCODE_PRIM(inst);

	if( ppc_mode ) {
		switch( inst ) {
		case 0x4C000064: /* rfe */
		case 0x4E800020: /* blr */
			return 1;
		}

		switch( opc ) {
		case 18: /* b (without link) */
			if( inst & MOL_BIT(31) )
				return FALSE;
			return TRUE;
		}
	} else {
		if( ((inst >> 16) & 0xffc0) == 0x4ec0 )
			return 1;	/* jmp */
		if( ((inst >> 16) & 0xffc0) == 0x4e40 )
			return 1;	/* jsr */
		switch( inst >> 24 ){
		case 0x60:	/* bra */
		case 0x61:	/* bsr */
			return 1;
		}
	}
	return FALSE;
}


/**************************************************************
*  check_cur_inst
*    
**************************************************************/

static int 
check_cur_inst( ulong inst ) 
{
	int opc, op_ext;

	if( ppc_mode ) {
		opc = OPCODE_PRIM(inst);
		op_ext = OPCODE_EXT(inst);
	
		switch( opc ) {
		case 18: /* b */
		case 16: /* bc */
			return 1;		
		}
		switch( OPCODE(opc, op_ext) ) {
		case OPCODE(19,528): /* bcctr */
		case OPCODE(19,16): /* bclr */
			return 1;
		}
	} else {
		/* 68k */
		if( ((inst >> 16) & 0xffff) == 0x4e75 )
			return 1;	/* rts */
		if( ((inst >> 16) & 0xffff) == 0x4e73 )
			return 1;	/* rte */
		if( ((inst >> 16) & 0xf0f8) == 0x50c8 )
			return 1;	/* db[cc] */
		if( ((inst >> 16) & 0xffc0) == 0x4ec0 )
			return 1;	/* jmp */
		if( ((inst >> 16) & 0xffc0) == 0x4e80 )
			return 1;	/* jsr */
		if( (inst >> 28) == 0x6 )
			return 1;	/* b[cc] */
		if( ((inst >> 16) & 0xfff0) == 0x4e40 )
			return 1;	/* TRAP */
		if( ((inst >> 16) & 0xf0f8) == 0x50f8 )
			return 1;	/* TRAP[cc] */		
	}
	return 0;
}


ulong 
get_inst_win_addr( void ) 
{
	return (ulong)inst_top_ptr;
}

int 
is_68k_mode( void )
{
	return !ppc_mode;
}

ulong 
get_nip( void )
{
	if( ppc_mode )
		return mregs->nip;

	/* We silently assume that mregs->nip is at the beginning
	 * of the jump table.
	 */
	return mregs->gpr[24]-2;
}

void
set_nip( ulong new_nip )
{
	refetch_mregs();
	if( ppc_mode )
		mregs->nip = new_nip;
	else
		set_68k_nip( new_nip );
	send_mregs_to_mol();
}


void
set_68k_nip( ulong mac_nip )
{
	ulong inst;
	
	/* m68k_nip can be set by loading mregs->nip by setting 
	 *   	nip = MACOS_68K_JUMP_TABLE + (first opcode word<<3)
	 * r27 = opcode word 2
	 * r2  = mac_nip + 2
	 */
	if( (mregs->nip & ~0x7ffff) != MACOS_68K_JUMP_TABLE ){
		printm("Can't set 68k-nip. Step once to 'align' nip\n");
		return;
	}

	inst = readc_ea( mac_nip, get_inst_context(), kInstTrans );
	mregs->gpr[27] = inst & 0xffff;
	mregs->gpr[24] = mac_nip +2;
	mregs->nip = MACOS_68K_JUMP_TABLE | (inst>>(16-3));
}


static int
get_inst_len( ulong pc )
{
	char buf[2];	/* dummy buffer */

	if( ppc_mode )
		return 4;
	return print_insn( buf, 1, pc, get_inst_context(), ppc_mode );	
}



/**************************************************************
*  convert_tab
*    
**************************************************************/

static void 
convert_tab( char *src, char *dest ) 
{
	int	i=0;
	
	while( *src ) {
		*dest = *src;
		if( *src == '\t' ) {
			i=8-i%8;
			while(i-->0)
				*(dest++)=' ';
			dest--;
		}
		src++;
		dest++;
		i++;
	}
	*dest = 0;
}

struct dis_pb {
	char 	*obuf;
	size_t	obuf_size;
	int 	context;
};

static int
print_insn( char *obuf, size_t size, ulong addr, int context, int ppc )
{
	struct dis_pb dpb;
	int ret;

	dpb.obuf = obuf;
	dpb.obuf_size = size;
	dpb.context = context;

	dis_info.application_data = &dpb;
	obuf[0] = 0;

	if( ppc ) {
		dis_info.mach = 0;
		ret = print_insn_big_powerpc( addr, &dis_info );
	} else {
		dis_info.mach = bfd_mach_m68020;
		ret = print_insn_m68k( addr, &dis_info );
	}
	
	dis_info.application_data = NULL;

	return ret;
}

static int
insn_read_mem_func( bfd_vma memaddr, bfd_byte *dest, unsigned int length,
		    struct disassemble_info *info )
{
	ulong buf[2];
	struct dis_pb *dpb = info->application_data;

	buf[0] = readc_ea( (ulong)memaddr,dpb->context, kInstTrans );
	if( length > 4 )
		buf[1] = readc_ea( (ulong)memaddr+4,dpb->context, kInstTrans );
	if( length > 8 ) {
		length = 8;
		printm("Warning: insn_read_mem_func %d bytes requested\n", length );
	}
	memmove( dest, buf, length );
	return 0;
}

static int
insn_fprintf_func( PTR dummy, const char *fmt, ... )
{
	struct dis_pb *dpb = dis_info.application_data;
	va_list	args;
	int len, space;

	len = strlen( dpb->obuf );
	space = dpb->obuf_size - len - 1;
	if( space < 0 )
		return 0;	
	va_start( args, fmt );
	len = vsnprintf( dpb->obuf + len, space, fmt, args );
	va_end(args );

	return len;
}

static void
insn_print_addr_func( bfd_vma addr, struct disassemble_info *info )
{
	char *sym;

	sym = symbol_from_addr(addr);
	if( sym )
		(info->fprintf_func)( info->stream, "%s", sym );
	else
		(info->fprintf_func)( info->stream, "0x%x", addr );
}


/************************************************************************/
/*	CMDs								*/
/************************************************************************/

/* Disassembly from. Syn. il [addr] [U|M] */
static int 
cmd_il( int numargs, char **args ) 
{
	if( numargs>3)
		return 1;

	/* U - force unmapped translation, M - force MMU-translation */
	il_forced_context = 0;
	if( numargs== 3 ) {
		il_forced_context = kContextMapped_S;
		if( toupper(*args[2]) == 'X' )
			il_forced_context = kContextUnmapped;
	}

	if( numargs==1 )
		inst_top_ptr = (char*)get_nip();
	else
		inst_top_ptr = (char*)string_to_ulong(args[1]);

	inst_top_ptr -= ((ulong)inst_top_ptr)% (ppc_mode? 4 : 2);

	redraw_inst_win();
	return 0;
}

/* Display memory. Syn. dm addr [U|M] */
static int 
cmd_dm( int numargs, char **args ) 
{
	if( numargs<2 || numargs>3)
		return 1;

	/* U - force unmapped translation, M - force MMU-translation */
	dm_forced_context = 0;
	if( numargs== 3 ) {
		dm_forced_context = kContextMapped_S;
		if( toupper(*args[2]) == 'X' )
			dm_forced_context = kContextUnmapped;
	}

	mem_top_ptr = (char*)string_to_ulong(args[1]);

	draw_data_win(0);
	wrefresh( data_win );
	return 0;
}

/* Display debug registers. Syn. sx  */
static int 
cmd_sx( int numargs, char **args ) 
{
	int i;
	if( numargs!=1)
		return 1;
	for(i=0; i<NUM_DEBUG_REGS; i++)
		printm("%08lx ", mregs->debug[i] );
	printm("\n");
	return 0;
}

/* Display registers */
static int 
cmd_regs( int numargs, char **args )
{
	int i;

	printm("NIP: %08lx   LR: %08lx   CTR: %08lx   MSR: %08lx\n",
	       mregs->nip,mregs->link,mregs->ctr, mregs->msr );
	for(i=0; i<32; i++ ){
		printm("%08lx ",mregs->gpr[i] );
		if( i%8==7 )
			printm("\n");
	}
	return 0;
}
/* Display fp registers */
static int 
cmd_fregs( int numargs, char **args )
{
	int i;

	printm("XXX: flush fregs....\n");
#if 0
	flush_fregs();
#endif
	printm("FPU-registers: \n");
	for(i=0; i<32; i++ ){
		if( i%4==0 )
			printm("  %2d   ", i );
		
		printm("%08lx %08lx ;  ",mregs->fpr[i].h, mregs->fpr[i].l );
		if( i%4==3 )
			printm("\n");
	}
	return 0;
}

/* Disasseble */
static int
cmd_dil( int numargs, char **args )
{
	ulong ea;
	int len;
	char buf[50], buf2[100];
	if( numargs < 2 )
		return -1;
	
	ea = string_to_ulong(args[1]);
	len = 0x80;
	if( numargs == 3 )
		len = string_to_ulong(args[2]);

	while( len >0 ){
		int i, inc, val;
		char *sym;

		sym = symbol_from_addr( ea );
		if( sym )
			printm("%s:\n",sym );

		printm("  %08lx  ", ea );
		inc = print_insn( buf, 50, ea, get_inst_context(), ppc_mode );
		convert_tab( buf, buf2 );
		printm("  %-38s ", buf2);
		for( i=0; i<inc; i++ ) {
			val = readc_ea( ea+i, get_inst_context(), kInstTrans ) >> 24;
			printm( "%02X ", val );
		}
		printm("\n");
		ea += inc;
		len -= inc;
	}
	return 0;
}

/* 
 * This works for gcc generated code. 
 * Linux: sp_offs = 4, Darwin: sp_offs = 8.
 */
static int 
print_stack_frame( ulong sp, int depth, int sp_offs )
{
	ulong sp2, lr;
	char *sym;
	int i;
	
	/* stack should always be aligned (preferably, quad word) */
	if( sp & 0x3 || depth>100 )
		return 1;

	sp2 = readc_ea( sp, get_data_context(), kDataTrans );
	lr = readc_ea( sp2+sp_offs, get_data_context(), kDataTrans );

	/* XXX: A search tree for the symbols would be nice. */
       	for( sym=NULL, i=0; i<400 && !sym; i++ )
		sym = symbol_from_addr(lr - i*4);

	if( sp2 > sp )
		print_stack_frame( sp2, depth+1, sp_offs );

	printm("%08lx:  LR = %08lx  ", sp, lr );
	if( sym )
		printm("%s + 0x%x",sym, (i-1)*4 );
	printm("\n");
	return 0;	
}

/* Display stack chain */
static int
cmd_sf( int numargs, char **args )
{
	ulong sp;
	char *sym;
	int i;
	
	if( numargs > 2 )
		return -1;

	sp = mregs->gpr[1];
	if( numargs == 2 )
		sp = string_to_ulong(args[1]);

	if( ppc_mode ) {
		int sp_offs = (!strcmp(args[0],"sfd")) ? 8 /* darwin */ : 4 /* linux */;
		print_stack_frame( sp, 0, sp_offs );

		/* XXX: A search tree for the symbols would be nice. */
		for( sym=NULL, i=0; i<400 && !sym; i++ )
			sym = symbol_from_addr(mregs->link - i*4);
		printm("           LR:  %08lx  ", mregs->link);
		if( sym )
			printm("%s + 0x%x", sym, (i-1)*4 );
		printm("\n");

		for( sym=NULL, i=0; i<400 && !sym; i++ )
			sym = symbol_from_addr(mregs->nip - i*4);
		printm("           NIP: %08lx  ", mregs->nip);
		if( sym )
			printm("%s + 0x%x", sym, (i-1)*4 );

		printm("\n");
	} else {
		printm("does not support 68k-mode\n");
	}
	return 0;
}

/* toggle i/d bat display */
static int
cmd_bats( int numargs, char **args )
{
	if( numargs !=1 )
		return 1;
	printm("Toggling DBAT/IBAT display\n");
	display_dbats = !display_dbats;
	refresh_debugger_window();
	return 0;
}



/************************************************************************/
/*	breakpoints							*/
/************************************************************************/

/* set breakpoint breakpoint */
static int
cmd_br( int numargs, char**args ) 
{
	ulong addr;
	int flags;

	if( numargs >= 4 )
		return 1;

	if( numargs == 1 )
		addr = mregs->nip + 4;
	else
		addr = string_to_ulong(args[1]);

	flags = ppc_mode ? 0 : br_m68k;
	if( ppc_mode && numargs == 3 )
		flags |= (toupper(args[2][0])=='X') ? br_phys_ea : br_transl_ea;

	printm("Setting breakpoint at %08lX\n", addr );
	dbg_add_breakpoint( addr, flags, 0 );

	clear_cache();
	refresh_debugger();
	return 0;
}

/* set decrementer breakpoint */
static int
cmd_brd( int numargs, char**args ) 
{
	ulong addr;
	int dec=0;
	
	if( numargs > 3 )
		return 1;

	if( numargs >= 2 )
		addr = string_to_ulong(args[1]);
	else
		addr = mregs->nip+4;

	if( numargs == 3 )
		dec = string_to_ulong(args[2] );
	if( dec <= 0 )
		dec = 1;

	printm("Adding decrementer breakpoint at 0x%08lX\n",addr);
	dbg_add_breakpoint( addr, br_dec_flag, dec );

	clear_cache();
	refresh_debugger();
	return 0;
}


void 
install_monitor_cmds( void ) 
{
	add_cmd( "il", "il [addr] [X|M]\ndisassemble from addr\n", -1, cmd_il );
	add_cmd( "dm", "dm addr [X|M]\ndisplay memory from addr\n", -1, cmd_dm );
	add_cmd( "sx", "sx \nshow DEBUG registers\n", -1, cmd_sx );
	add_cmd( "regs", "regs \nshow registers\n", -1, cmd_regs );
	add_cmd( "fregs", "fregs \nshow FPU registers\n", -1, cmd_fregs );
	add_cmd( "sf", "sf [stack addr] \ndisplay stack chain (Linux)\n", -1, cmd_sf );
	add_cmd( "sfd", "sfd [stack addr] \ndisplay stack chain (Darwin)\n", -1, cmd_sf );
	add_cmd( "dil", "dil addr [len] \ndisassembly from addr\n", -1, cmd_dil );
	add_cmd( "bats", "bats \ntoggle display between I/D BATS\n",-1,cmd_bats );

	add_cmd( "br", "br [addr] [X|M]\nadd breakpoint\n", -1, cmd_br );
	add_cmd( "brd", "brd [addr] [dec] \nadd decrementer breakpoint\n", -1, cmd_brd );
}

