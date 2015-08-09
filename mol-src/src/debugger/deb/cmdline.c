/* 
 *   Creation Date: <97/06/26 21:18:28 samuel>
 *   Time-stamp: <2004/01/16 20:40:44 samuel>
 *   
 *	<cmdline.c>
 *	
 *	Debugger cmdline and info area
 *   
 *   Copyright (C) 1997-2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <curses.h>
#include <stdarg.h>
#include <stddef.h>

#include "res_manager.h"
#include "cmdline.h"
#include "monitor.h"
#include "mac_registers.h"
#include "extralib.h"
#include "symbols.h"
#include "memory.h"
#include "deb.h"
#include "processor.h"

#define	BUF_SIZE   	80

/* ---- HISTORY ------ */

#define NUM_HIST	30
static char	*history[ NUM_HIST ];
static int	hist_bot;
static int	hist_disp;

#define	HIST_M1(n)	( ((n)-1+NUM_HIST) % NUM_HIST )
#define HIST_P1(n)	( ((n)+1) % NUM_HIST )


/* ---- COMMAND LINE ------*/

static char	cmdline[ BUF_SIZE +1];
static char	*yank;
static int	pos;			/* cursor pos, starting at 0 */

static void	add_to_history( int incflag );
static void 	insert_str( char *str );

static int 	cmdline_inited = FALSE;

/* ----- COMMAND PROCESSING ----- */

#define MAX_NUM_ARGS	40

/* for now, we use a simple linked list */
typedef struct cmdentry {
	struct cmdentry *next;
	char	*cmdname;
	char	*help;
	void	*func;
} cmdentry;

static cmdentry	*root;

static cmdentry *lookup_cmd( char *cmdname, int numargs );
static void 	tab_completion( void );
static void 	cmdline_parse( char *cmdline );

static char 	*reg_expand( char *rawstr );
static char 	*split_str( char *rawstr );
static int	do_algebra( int *numargs, char **args );
static int 	do_operation( int *numargs, char **args, char *op );
static int 	assign_reg( char *str, ulong value );

typedef int (*argnfunc)( int argv, char **argc );


/* ------ MESSAGE BUFFER ------ */

#define MES_NUM_LINES	256
static char	*mes_line[ MES_NUM_LINES ];
static int	mes_bot_line;

/**************************************************************
*  cmdline_do_key
*    
**************************************************************/

void 
cmdline_do_key( int c ) 
{
	switch( c ) {
	case KEY_ENTER:
	case 13: /* return */
		hist_disp = hist_bot;
		add_to_history( TRUE );
		printm( ">%s\n",cmdline);
		cmdline_parse( cmdline );
		cmdline[0]=0;
		pos = 0;
		break;
		
	case 4: /* c-D */
		if( cmdline[pos] ) {
			memmove( cmdline+pos, cmdline+pos+1,
				strlen(cmdline+pos+1) +1);
			break;
		}
		/* fall through to tab-completion */
	case 9:	/* tab */
		/* tab-completion */
		tab_completion();
		break;

	case 1: /* c-A */
		pos = 0;
		break;
	case 5: /* c-E */
		pos = strlen( cmdline );
		break;
	case 2: /* c-B */
	case KEY_LEFT:
		if( pos )
			pos--;
		break;
	case 6: /* c-F */
	case KEY_RIGHT:
		if( pos< strlen(cmdline) )
			pos++;
		break;
	case 25: /* c-Y */
		if( yank )
			insert_str( yank );
		break;
	case 128+'f': /* M-f */
		if( pos<strlen(cmdline))
			pos++;
		while( pos<strlen(cmdline)
		       && !(cmdline[pos]==' ' && cmdline[pos-1]!=' '))
			pos++;
		break;
	case 128+'b': /* M-b */
		if( pos>0 )
			pos--;
		while( pos>0 
		       && !(cmdline[pos-1]==' ' && cmdline[pos]!=' '))
			pos--;
		break;		
	case 16: /* c-p  history up */
		add_to_history( FALSE );
		hist_disp = HIST_M1( hist_disp );
		if( history[hist_disp]==0 || hist_disp==hist_bot ) {
			hist_disp = HIST_P1( hist_disp );
			break;
		}
		strcpy( cmdline, history[hist_disp] );
		pos = strlen( cmdline );
		break;	
	case 14: /* c-n  history down*/
		hist_disp = HIST_P1( hist_disp );
		if( history[hist_disp]==0 || hist_disp==HIST_P1(hist_bot)) {
			hist_disp = HIST_M1( hist_disp );
			break;
		}
		strcpy( cmdline, history[hist_disp] );
		pos = strlen( cmdline );
		break;
	case 'p'+128: /* M-p */
		mes_bot_line++;
		if( mes_bot_line >= MES_NUM_LINES )
			mes_bot_line = MES_NUM_LINES-1;
		break;
	case 'n'+128: /* M-n */
		mes_bot_line--;
		if( mes_bot_line <= 0 )
			mes_bot_line = 1;
		break;
	case 11: /* c-k */
		if( yank )
			free(yank );
		if( pos < BUF_SIZE-1 ) {
			yank = (char*) malloc( strlen(cmdline+pos)+1);
			strcpy( yank, cmdline+pos );
			cmdline[pos]=0;
		} else {
			yank = 0;
		}
		break;
	case 8: /*del*/
	case 127: /* backspace */
	case KEY_BACKSPACE:
		if( !pos )
			return;
		memmove( cmdline+pos-1, cmdline+pos, strlen(cmdline+pos)+1 );
		pos--;
		break;

	default: /* key to be inserted */
		if( !isprint(c) )
			break;
		
		if( strlen(cmdline) < BUF_SIZE-1 ) {
			memmove( cmdline+pos+1, cmdline+pos,
				 strlen(cmdline+pos)+1 );
			cmdline[pos]=c;
			pos++;
		} else {
			char buf[2];
			buf[0]=c;
			buf[1]=0;
			insert_str( buf );
		}
	}

	cmdline_draw();
	refresh();
}


/**************************************************************
*  add_to_history
*    
**************************************************************/

static void 
add_to_history( int incflag ) 
{
	if( hist_disp != hist_bot || ( !strlen(cmdline) && incflag)  )
		return;
	
	free( history[hist_bot] );
	history[hist_bot] = (char*)malloc( strlen(cmdline)+1 );
	strcpy( history[hist_bot], cmdline );
	
	if( incflag )
		hist_disp = hist_bot = (hist_bot +1)% NUM_HIST;
}


/**************************************************************
*  insert_str
*    insert string into cmd-string at pos
**************************************************************/

static void 
insert_str( char *str ) 
{
	char	buf[2*BUF_SIZE];

	strcpy( buf, cmdline );
	buf[pos]=0;
	strcat( buf, str );
	strcat( buf, cmdline+pos );

	buf[BUF_SIZE-1] = 0;	/* make sure it isn't too long */

	strcpy( cmdline, buf );

	pos += strlen(str);
	if( pos>BUF_SIZE-1 )
		pos=BUF_SIZE-1;
}


/**************************************************************
*  cmdline_print
*    
**************************************************************/

static int 
cmdline_print( char *mes ) 
{
	size_t size;
	char *p, *tmp;
	int i, len=0, broken=0;

	p = mes;
	while( *p ) {
		if( *p == '\n' ) {
			broken=1;
			break;
		}
		p++;
		len++;
		mes_bot_line = 1;
	}

	size = len + 1;
	if( mes_line[0] )
		size += strlen( mes_line[0] );
	
	tmp = (char*)malloc( size );
	tmp[0] = 0;
	if( mes_line[0] ) {
		strcpy( tmp, mes_line[0] );
		free( mes_line[0] );
	}
	mes_line[0] = tmp;

	i=strlen(tmp)+len;
	memmove( tmp+strlen(tmp), mes, len );
	tmp[i]=0;
	
	if( broken ) {
		free( mes_line[MES_NUM_LINES-1] );
		for( i=MES_NUM_LINES-1; i>0; i-- )
			mes_line[i] = mes_line[i-1];
		mes_line[0] = 0;

		*p = 0;

		cmdline_draw();
		refresh();
		cmdline_print( p+1 );
	}
	return 1;
}


/**************************************************************
*  cmdline_draw
*    
**************************************************************/

void 
cmdline_draw( void ) 
{
	int	i, y, v,h;

	getmaxyx( stdscr, v,h );

	/* message lines */
	y=v-2;
	i=mes_bot_line;
	while( y>=0 ) {
		move( y,0 );
		clrtoeol();
		if( i<MES_NUM_LINES && mes_line[i] ) {
			printw( "%s",mes_line[i] );
		}
		i++;
		y--;
	}
	
	/* cmdline */
	move(v-1,0);
       	clrtoeol();
	printw(">%s", cmdline);
	cmdline_setcursor();
}


/**************************************************************
*  cmdline_setcursor
*    moves the cursor to its correct position
**************************************************************/

void 
cmdline_setcursor( void )
{
	int	v,h;

#if 0
	/* This code seems useless, the address will always be true */
	if( !cmdline_setcursor )
		return;
#endif

	getmaxyx( stdscr, v,h );
	move( v-1,1+pos );
}


/**************************************************************
*  expand
*    expands symbols and register values in string
*    the returned string must be freed with free()
**************************************************************/

#define ENTRY( key, field )  { key, offsetof(mac_regs_t, field ) },
static struct {
	char	*key;
	int	offs;
} regtable[] = {
	ENTRY( "lr",		link 		) 
	ENTRY( "ctr",		ctr 		)
	ENTRY( "cr",		cr 		)
	ENTRY( "xer",		xer		)
	ENTRY( "fpscr",		fpscr		)
	ENTRY( "vscr",		vscr		)
	ENTRY( "vrsave",	spr[S_VRSAVE]	)
	ENTRY( "sprg0", 	spr[S_SPRG0]	)
	ENTRY( "sprg1", 	spr[S_SPRG1]	)
	ENTRY( "sprg2", 	spr[S_SPRG2]	)
	ENTRY( "sprg3", 	spr[S_SPRG3]	)
	ENTRY( "srr0",		spr[S_SRR0]	)
	ENTRY( "srr1",		spr[S_SRR1]	)
	ENTRY( "hid0",		spr[S_HID0]	)
	ENTRY( "hid1",		spr[S_HID1]	)
	ENTRY( "hid2",		spr[S_HID2]	)
	ENTRY( "hid5",		spr[S_HID5]	)
	ENTRY( "hid15",		spr[S_HID15]	)
	ENTRY( "dsisr", 	spr[S_DSISR]	)
	ENTRY( "dar",		spr[S_DAR]	)
	ENTRY( "dec",		spr[S_DEC]	)
	ENTRY( "ear",		spr[S_EAR]	)
	ENTRY( "pvr",		spr[S_PVR]	)
	ENTRY( "rtcu",		spr[S_RTCU_R]	)
	ENTRY( "rtcl",		spr[S_RTCL_R]	)
	ENTRY( "sdr1",		spr[S_SDR1]	)
	ENTRY( "ibat0l",	spr[S_IBAT0L]	)
	ENTRY( "ibat1l",	spr[S_IBAT1L]	)
	ENTRY( "ibat2l",	spr[S_IBAT2L]	)
	ENTRY( "ibat3l",	spr[S_IBAT3L]	)
	ENTRY( "ibat0u",	spr[S_IBAT0U]	)
	ENTRY( "ibat1u",	spr[S_IBAT1U]	)
	ENTRY( "ibat2u",	spr[S_IBAT2U]	)
	ENTRY( "ibat3u",	spr[S_IBAT3U]	)
	ENTRY( "dbat0l",	spr[S_DBAT0L]	)
	ENTRY( "dbat1l",	spr[S_DBAT1L]	)
	ENTRY( "dbat2l",	spr[S_DBAT2L]	)
	ENTRY( "dbat3l",	spr[S_DBAT3L]	)
	ENTRY( "dbat0u",	spr[S_DBAT0U]	)
	ENTRY( "dbat1u",	spr[S_DBAT1U]	)
	ENTRY( "dbat2u",	spr[S_DBAT2U]	)
	ENTRY( "dbat3u",	spr[S_DBAT3U]	)
	ENTRY( "msr",		msr		)
	ENTRY( "debug0",	debug[0]	)
	ENTRY( "debug1",	debug[1]	)
	ENTRY( "debug2",	debug[2]	)
	ENTRY( "debug3",	debug[3]	)
	ENTRY( "debug4",	debug[4]	)
	ENTRY( "debug5",	debug[5]	)
	ENTRY( "debug6",	debug[6]	)
	ENTRY( "debug7",	debug[7]	)
	ENTRY( "debug8",	debug[8]	)
	ENTRY( "debug9",	debug[9]	)
	{ NULL, 0 }
};

static int
get_mregs_offs( char *rawstr, int *offs )
{
	int i;
	for( i=0; regtable[i].key && strcasecmp(regtable[i].key,rawstr) ; i++  )
		;
	if( !regtable[i].key )
		return 0;
	*offs = regtable[i].offs;
	return 1;
}


#define COMP( reg, v )  		\
if( !strcasecmp(reg,rawstr) ) {		\
	value = v;			\
	break;				\
}

static char *
reg_expand( char *rawstr )
{
	int 	notfound = FALSE;
	int 	value, len, offs;
	char	*str;

	/* No locking is needed (called from locked function) */

	/* Remove citates */
	len = strlen(rawstr );
	if( len>=2 && rawstr[0]=='"' && rawstr[ len-1 ] =='"' ) {
		str = strdup( rawstr+1);
		str[ len-2 ] = 0;
		return str;
	}

	/* Remove lisp-like quotes */
	if( len && rawstr[0]=='\'' )
		return strdup( rawstr+1 );
	
	for(;;) {
		/* symbols */
		if( (value=addr_from_symbol( rawstr )) != 0 )
			break;

		/* mregs */
		if( get_mregs_offs( rawstr, &offs ) ) {
			value = *(int*)((char*)mregs + offs );
			break;
		}

		/* named regs */
		COMP( "pc", get_nip() );

		/* general purpose registers */
		if( (rawstr[0]=='r' || rawstr[0]=='R') && is_number_str(rawstr+1) ) {
			sscanf( rawstr+1,"%d",&value );
			if( value>=0 && value<32 ) {
				value = mregs->gpr[ value ];
				break;
			}
		}
		if( !strncasecmp("gpr",rawstr,3) && is_number_str(rawstr+3) ) {
			sscanf(rawstr+3, "%d",&value );
			if( value>=0 && value<32 ) {
				value = mregs->gpr[value];
				break;
			}
		}
		/* segment registers */
		if( !strncasecmp("segr",rawstr,4) && is_number_str(rawstr+4) ) {
			sscanf(rawstr+4, "%d",&value );
			if( value>=0 && value<16 ) {
				value = mregs->segr[value];
				break;
			}
		}

		/* 68k registers... d0-d7 = r8-r15, a0-a6 = r16-r22, a7=r1 */
		if( is_68k_mode() && (toupper(rawstr[0])=='D' || toupper(rawstr[0])=='A') && is_number_str(rawstr+1) ) {
			sscanf( rawstr+1,"%d",&value );
			if( value>=0 && value<8 ) {
				value += (toupper(rawstr[0]) =='A') ? 16 :8;
				if( value == 23 )	/* fix a7 */
					value = 1;
				value = mregs->gpr[ value ];
				break;
			}
		}
		notfound = TRUE;
		break;
	}

	if( notfound ) {
		str = strdup( rawstr );
	} else {
		str = (char*)malloc( 16 );
		sprintf(str,"%X",value );
	}
	return str;
}


/**************************************************************
*  split_str
*    Ex: "r26=r5+20" -> "r26 = r5 + r20"
**************************************************************/

static char *
split_str( char *rawstr ) 
{
	char	*p,*str;
	char	c;
	int	inword = TRUE;

	p = str = (char*)malloc( strlen(rawstr)*3 + 10 );
	
	/* extract arguments */
	while( *rawstr ) {
		c=*(rawstr++);
		if( c=='=' || c=='+' || c=='-' || c=='*' || c=='@' || c=='#') {
			if(inword)
				*(p++)=' ';
			*(p++)=c;
			*(p++)=' ';
			inword = FALSE;
			continue;
		}

		if( isblank(c) && inword ) {
			*(p++)=' ';
			inword = FALSE;
		}
		if( !isblank(c) ) {
			inword = TRUE;
			*(p++)=c;
		}
	}
	*(p++)=0;

	return str;
}


/**************************************************************
*  do_algebra
*    ret: 1  if no further action is needed (no command)
*    	 -1  if error
*	  0  if a regular command
**************************************************************/

static int 
do_algebra( int *numargs, char **args ) 
{
	int err;

	while( !(err=do_operation( numargs, args, "#" )) )
		if( err==-1 )
			return -1;
	while( !(err=do_operation( numargs, args, "*" )) )
		if( err==-1 )
			return -1;
	while( !(err=do_operation( numargs, args, "-" )) )
		if( err==-1 )
			return -1;
	while( !(err=do_operation( numargs, args, "+" )) )
		if( err==-1 )
			return -1;
	while( !(err=do_operation( numargs, args, "@" )) )
		if( err==-1 )
			return -1;

	/* is there an assignment? */
	if( *numargs == 3 && !strcmp(args[1],"=") && is_number_str(args[2]) ) {
		ulong num = string_to_ulong( args[2] );

		if( !assign_reg( args[0], num ) )
			return 1;
       		return -1;
	}

	/* is the first argument numeric and the only arg? */
	if( *numargs == 1 && is_number_str(args[0])) {
		ulong num = string_to_ulong( args[0] );
		printm("0x%08lX #%ld ", num, num );
		printm("BIN: %08lX %08lX %08lX %08lX\n", hexbin(num>>24), hexbin(num>>16),
		       hexbin(num>>8), hexbin(num) );
		return 1;
	}

	/* do assignment */
	return 0;
}


/**************************************************************
*  assign_reg
*    
**************************************************************/

static int 
assign_reg( char *regname, ulong value )
{
	ulong	*laddr = NULL;
	int	found = TRUE;
	int	num, offs;

	/* pc requires special attention (68k mode...) */
	if( !strcasecmp( "pc", regname ) ) {
		set_nip( value );
		refresh_debugger_window();
		return 0;
	}

	refetch_mregs();
	for(;;) {
		/* mregs */
		if( get_mregs_offs( regname, &offs ) ) {
			laddr = (ulong*)((char*)mregs + offs);
			break;
		}

		/* general purpose registers */
		if( (regname[0]=='r' || regname[0]=='R') && is_number_str(regname+1) ) {
			sscanf( regname+1,"%d",&num );
			if( num>=0 && num<32 ) {
				laddr = &mregs->gpr[ num ];
				break;
			}
		}
		if( !strncasecmp("gpr",regname,3) && is_number_str(regname+3) ) {
			int num;
			sscanf(regname+3, "%d",&num );
			if( num>=0 && num<32 ) {
				laddr = &mregs->gpr[num];
				break;
			}
		}
		/* segment registers */
		if( !strncasecmp("segr",regname,4) && is_number_str(regname+4) ) {
			sscanf(regname+4, "%d",&num );
			if( num>=0 && num<16 ) {
				laddr = &mregs->segr[num];
				break;
			}
		} 

		/* 68k registers */
		if( is_68k_mode() ){
			int ch = toupper( regname[0] );
			if( (ch == 'D' || ch == 'A') && is_number_str(regname+1) ) {
				sscanf(regname+1, "%d",&num );
				if( num>=0 && num <8 ){
					/* d0-d7 = r8-r15,  a0-a6 = r16-r22, a7=r1 */
					num += (ch == 'D') ? 8 : 16;
					if( num == 23 )
						num = 1;
					laddr = &mregs->gpr[num];
					break;
				}
			}
		}

		found = FALSE;
		break;
	}
	if( !found )
		return 1;

	if( laddr )
		*laddr = value;

	send_mregs_to_mol();
	refresh_debugger_window();
	
	return 0;
}


/**************************************************************
*  do_operation
*    ret: 0  call again
*    	  1  done
*	 -1  error
**************************************************************/

static int
do_operation( int *numargs, char **args, char *op ) 
{
	int 	i,j;
	int 	hasl, hasr;
	int	remove=-1, num_remove=1;
	ulong	left=0, right;
	
	for(i=0; i<*numargs; i++ ) {
		hasl = (i>0)? is_number_str( args[i-1] ) : 0;
		hasr = (i<*numargs-1)? is_number_str( args[i+1] ) : 0;
		if( hasl )
			left = string_to_ulong( args[i-1] );
		if( hasr )
			right = string_to_ulong( args[i+1] );

		if( strcmp( args[i], op ) )
			continue;
		
		switch( op[0] ) {
		case '#': /* #arg (decimal value) */
			if( !hasr )
				return -1;
			for(j=0; j<strlen(args[i+1]); j++)
				if( !isdigit( args[i+1][j] ))
					return -1;
			sscanf( args[i+1], "%ld",&right );
			free( args[i+1] );
			args[i+1] = num_to_string( right );
			remove = i;
			break;

		case '-': /* - arg */
			if( !hasr )
				return -1;
			free( args[i+1] );
			args[i+1] = num_to_string( -right );
			args[i][0] = '+';
			break;

		case '+': /* +arg | arg+arg */
			if( !hasl && hasr ) {
				remove = i;
				break;
			}
			if( hasl && hasr ) {
				free( args[i+1] );
				args[i+1] = num_to_string( left+right );
				remove = i-1;
				num_remove = 2;
				break;
			}
			return -1;	/* parse error */

		case '*': /* arg*arg */
			if( hasl && hasr ) {
				free( args[i+1] );
				args[i+1] = num_to_string( left*right );
				remove = i-1;
				num_remove = 2;
				break;
			}
			return -1;

		case '@': /* @ arg */
			if( !hasr )
				return -1;
			free( args[i+1] );
			args[i+1] = num_to_string( readc_ea( right, get_data_context(), kDataTrans ));
			remove = i;
			break;
		}

		/* delete argument(s) */
		if( remove != -1 ) {
			for(j=remove; j<remove+num_remove; j++ )
				free( args[j] );
			for(j=remove+num_remove; j<*numargs; j++ )
				args[j-num_remove]=args[j];
			*numargs -= num_remove;

			return 0;	/* more parsing might be needed */
		}
	}
	return 1;	/* (?) */
}


/**************************************************************
*  cmdline_parse
*   
**************************************************************/

static void 
cmdline_parse( char *rawstr ) 
{
	char	*args[MAX_NUM_ARGS];
	int	i,numargs=0, flag=1, doneflag;
	cmdentry *cmd;
	char	*str;
	int	is_assignment;
	
	/* split string */
	str = split_str( rawstr );

	/* extract arguments */
	i=0;
	while( str[i] ) {
		if( !isblank(str[i]) && flag ) {
			if( numargs<MAX_NUM_ARGS ) {
				args[numargs] = &str[i];
				numargs++;
			}
			flag = 0;
		}
		if( isblank(str[i]) ) {
			str[i] = 0;
			flag = 1;
		}
		i++;
	} 

	/* an assignment? */
	is_assignment = numargs>=3 && !strcmp(args[1],"=");

	/* expand registers */
	for(i=1; i<numargs; i++ )
		args[i] = reg_expand( args[i] );
	if( numargs && is_assignment )
		args[0] = strdup( args[0] );
	if( numargs && !is_assignment)
		args[0] = reg_expand( args[0] );

	/* do algebraic operations */
	doneflag = do_algebra( &numargs, args );
       	if( doneflag == -1 ) {
		printm("Parse error\n");
	}	

	if( numargs && doneflag==0 ) {
		/* one argument is the command name itself */
	
		cmd = lookup_cmd( args[0], numargs );
		if( cmd && cmd->func ) {
			if( ((argnfunc)cmd->func)( numargs, args ) )
				printm("usage: %s",cmd->help);
		} else
			printm("%s: Command not found.\n",args[0] );
	}
	/* free strings from reg_expand */
	for(i=0; i<numargs; i++ )
		free( args[i] );

	free(str);
}


/**************************************************************
*  tab_completion
*    
**************************************************************/

static void 
tab_completion( void ) 
{
	char 	  save;
	cmdentry  *cur = root, *first=0;
	int	  match, len, numfound=0;
	int	  v,h;

	getmaxyx( stdscr, v,h );
	
	save = cmdline[pos];
	cmdline[pos] = 0;

	while( cur ) {
		match = !strncmp( cur->cmdname, cmdline, strlen(cmdline) );
		if( first && match ) {
			numfound++;
		}
		if( first && !match )
			break;

		if( !first  && match ) {
			first = cur;
			numfound++;
		}		
		cur=cur->next;
	}

	if( numfound > 1 ) {
		int slen=0;
		while( numfound-- ) {
			if( !(slen % 10) && slen!=0 )
				slen++;
			while( slen < h && (slen % 10) ) {
				printm(" ");
				slen++;
			}
			slen = slen + strlen( first->cmdname );
			if( slen < h ) {
				printm("%s",first->cmdname );
			} else {
				printm("\n");
				slen = 0;
				continue;
			}
			first=first->next;
		}
		printm("\n");
	}
	
	len = strlen( cmdline );
	cmdline[pos] = save;

	if( numfound == 1 ) {  /*cmd-completion possible */
		insert_str( first->cmdname+len  );
	}
}


/**************************************************************
*  lookup_cmd
*    
**************************************************************/

static cmdentry *
lookup_cmd( char *cmdname, int numargs ) 
{
	cmdentry *cmd = root;
	
	while( cmd ) {
		if( !strcmp( cmd->cmdname, cmdname ))
			return cmd;
		cmd = cmd->next;
	}	
	return 0;
}


/**************************************************************
*  add_cmd
*    
**************************************************************/

void 
add_cmd( const char *cmdname, const char *help, int dummy, dbg_cmd_fp func )
{
	cmdentry **prev, *cur, *cmd;
	
	cmd = (cmdentry*) malloc( sizeof(cmdentry)+strlen(cmdname)+1+strlen(help)+1 );
	strcpy( (char*)cmd + sizeof(cmdentry), cmdname );

	cmd->cmdname = (char*)cmd + sizeof(cmdentry);
	strcpy( cmd->cmdname + strlen(cmdname)+1, help );
	cmd->help = cmd->cmdname + strlen(cmdname)+1;
	cmd->func = func;

	/* we maintain a sorted linked list */
	for( prev=&root, cur=root ;; ) {
		if( !cur ) {
			*prev = cmd;
			cmd->next = 0;
			break;
		}
		if( strcmp(cur->cmdname, cmd->cmdname) >= 0 ) {
			*prev = cmd;
			cmd->next = cur;
			break;
		}
		prev = &cur->next;
		cur = cur->next;
	}
}

void
add_remote_cmd( char *cmdname, char *help, int numargs )
{
	add_cmd( cmdname, help, numargs, do_remote_dbg_cmd );
}


/**************************************************************
*  yn_question
*    defanswer
**************************************************************/

int 
yn_question( char *prompt, int defanswer ) 
{
	int v,h, c, ret=defanswer;

	getmaxyx( stdscr, v,h );
	move( v-1,0 );
	printw("%s [%c/%c] ",prompt, defanswer?'Y':'y', defanswer? 'n':'N');
	c = deb_getch();

	printm("%s [%c/%c] %c\n",prompt, defanswer?'Y':'y', defanswer? 'n':'N', c);
	switch(c) {
	case 'Y':
	case 'y':
		ret = 1;
		break;
	case 'N':
	case 'n':
		ret = 0;
		break;
	}

	return ret;
}

/**************************************************************
*  C O M M A N D S
*    
**************************************************************/

static int 
cmd_help( int argc, char **argv )
{
	struct cmdentry *cur;

	if( argc >2 )
		return 1;

	if( argc==1 ) {
		for( cur=root; cur; cur=cur->next ) {
			printm("%s\n",cur->help );
		}
	} else {
		cur = lookup_cmd( argv[1], -1 );
		if( cur )
			printm("%s\n",cur->help );
		else 
			printm("Command '%s' not found\n",argv[1]);
	}
	return 0;
}

static int 
cmd_keyhelp( int argc, char **argv )
{
	printm("SORRY! NOT IMPLEMENTED\n");
	return 0;
}

/**************************************************************
*  init / cleanup
*    
**************************************************************/

void 
cmdline_init( void ) 
{
	int i;

	open_logfile("/tmp/moldbg-log");
	set_print_hook( cmdline_print );
	
	/* cmd-line */
	pos = 0;
	cmdline[0] = 0;	
	yank = 0;
	
	/* history */
	for(i=0; i<NUM_HIST; i++)
		history[i] = 0;
	hist_disp = 0;
	hist_bot = 0;

	cmdline_inited = TRUE;

	add_cmd( "help", "help [cmd] \nprint help text\n", -1, cmd_help );
	add_cmd( "keyhelp", "keyhelp \nprint keys\n", -1, cmd_keyhelp );

	/* message lines */
	mes_bot_line=1;		/* line-0 is never displayed */
	for( i=0; i<MES_NUM_LINES; i++ )
		mes_line[i] = 0;
}

void 
cmdline_cleanup( void ) 
{
	int 		i;
	cmdentry	*cmd, *tmp;

	if( !cmdline_inited )
		return;

	cmdline_inited = FALSE;

	free( yank );

	for(i=0; i<NUM_HIST; i++)
		free( history[i] );

	/* cmd-processing */
	cmd = root;
	while( cmd ) {
		tmp = cmd;
		cmd = cmd->next;
		free( tmp );
		/* the cmd/help-string is allocated in the same block  */
		/* as the struct */
	}

	for(i=0; i<MES_NUM_LINES; i++)
		free( mes_line[i] );
}
