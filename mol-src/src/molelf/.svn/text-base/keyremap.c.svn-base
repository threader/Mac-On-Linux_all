/* 
 *   Creation Date: <2002/04/14 13:48:54 samuel>
 *   Time-stamp: <2004/02/07 17:55:44 samuel>
 *   
 *	<keyremap.c>
 *	
 *	
 *   
 *   Copyright (C) 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <stdarg.h>
#include "os_interface.h"
#include "osi_calls.h"
#include "kbd_sh.h"
#include "video_sh.h"

#include "font_8x8.c"
#define FONT_ADJ_HEIGHT	 (FONT_HEIGHT + 2)
#define NCOLS	80
#define NROWS	48

#define NUM_COLORS 8
static int colortable[NUM_COLORS] = {
	0x000000,
	0x80dddd,
	0x00aaaa,
	0xccddff,
	0xdd8080,
	0x505050,
	0x808080,
	0xffffff
};

#define STYLES_MASK	0x7

typedef struct {
	int	bg_cind, fg_cind;
} style_t;

static style_t styles[] = {
	{ 0, 1 },	/* Style 0 */
	{ 0, 2 },
	{ 0, 3 },
	{ 0, 4 },
	{ 5, 4 },
	{ 6, 7 },
	{ 0, 7 },	/* Style 6 */
};

typedef struct {
	osi_get_vmode_info_t vmode;

	int	width, height;
	int	hadd, vadd;

	int 	h, v;
	
	char	buf[NROWS][NCOLS];
	char	attr[NROWS][NCOLS];

	int	bg_cind, fg_cind;
} console_t;

static console_t tc;

extern int entry( void );

/************************************************************************/
/*	Keymapping stuff						*/
/************************************************************************/

enum {
/*0*/	K_A=0, K_S, K_D, K_F, K_H, K_G, K_Z, K_X, 
	K_C, K_V, K_PARAGRAPH, K_B, K_Q, K_W, K_E, K_R, 
/*10*/	K_Y, K_T, K_1, K_2, K_3, K_4, K_6, K_5, 
	K_EQUAL, K_9, K_7, K_MINUS, K_8, K_0, K_BR_RIGHT, K_O,
/*20*/	K_U, K_BR_LEFT, K_I, K_P, K_RET, K_L, K_J, K_QUOTE, 
	K_K, K_SCOLON, K_BSLASH, K_COMMA, K_SLASH, K_N, K_M, K_POINT,
/*30*/	K_TAB, K_SPACE, K_GRAVE, K_BACKSPACE, K_34, K_ESC, K_CTRL_L, K_CMD_L,
	K_SHIFT_L, K_CAPSL, K_ALT_L, K_LEFT, K_RIGHT, K_DOWN, K_UP, K_3F,
/*40*/	K_40, K_KP_COMMA, K_42, K_KP_MULT, K_44, K_KP_PLUS, K_46, K_KP_CLEAR,
	K_48, K_49, K_4A, K_KP_SLASH, K_ENTER, K_4D, K_KP_MINUS, K_4F,
/*50*/	K_50, K_KP_EQUAL, K_KP_0, K_KP_1, K_KP_2, K_KP_3, K_KP_4, K_KP_5, 
	K_KP_6, K_KP_7, K_5A, K_KP_8, K_KP_9, K_5D, K_5E, K_5F, 
/*60*/	K_F5, K_F6, K_F7, K_F3, K_F8, K_F9, K_66, K_F11,
	K_68, K_F13, K_6A, K_F14, K_6C, K_F10, K_6E, K_F12, 
/*70*/	K_70, K_F15, K_HELP, K_HOME, K_PAGE_UP, K_DEL, K_F4, K_END, 
	K_F2, K_PAGE_DOWN, K_F1, K_SHIFT_R, K_7C, K_7D, K_7E, K_START,
/*80 - ALIAS - */
	K_CTRL_R, K_CMD_R, K_ALT_R,
	NUM_ADB_KEYCODES
};

typedef struct {
	int	alias;
	int	real_adb;
} alias_t;

static alias_t key_aliases[] = {
	{ K_CTRL_R, K_CTRL_L },
	{ K_ALT_R, K_ALT_L },
	{ K_CMD_R, K_CMD_L },
	{ 0, 0 }
};

typedef struct {
	int	key;
	char	*text;
} mdef_t;

typedef struct {
	int	key;
	mdef_t	*entries;
	int	cur;
} mtab_t;

static mdef_t menu_mod[] = {
	{K_SHIFT_L,	"Left Shift Key" },
	{K_SHIFT_R,	"Right Shfit Key" },
	{K_CTRL_L,	"Left Control Key" },
	{K_CTRL_R,	"Right Control Key" },
	{K_ALT_L,	"Left Alt Key" },
	{K_ALT_R,	"Right Alt Key" },
	{K_CMD_L,	"Left Apple Key" },
	{K_CMD_R,	"Right Apple Key" },
	{0,		NULL }
};
static mdef_t menu_fkeys[] = {
	{K_F1,		"F1" },
	{K_F2,		"F2" },
	{K_F3, 		"F3" },
	{K_F4,	 	"F4" },
	{K_F5,		"F5" },
	{K_F6,		"F6" },
	{K_F7, 		"F7" },
	{K_F8,	 	"F8" },
	{K_F9,		"F9" },
	{K_F10,		"F10" },
	{K_F11,		"F11" },
	{K_F12,	 	"F12" },
	{K_F13,	 	"F13" },
	{K_F14,	 	"F14" },
	{K_F15,	 	"F15" },
	{0,		NULL }
};
static mdef_t menu_row1[] = {
	{K_PARAGRAPH,	"Paragraph"},
	{K_1,		"1" },
	{K_2,		"2" },
	{K_3,		"3" },
	{K_4,		"4" },
	{K_5,		"5" },
	{K_6,		"6" },
	{K_7,		"7" },
	{K_8,		"8" },
	{K_9,		"9" },
	{K_0,		"0" },
	{K_MINUS,	"-" },
	{K_EQUAL,	"=" },
	{0,		NULL }
};
static mdef_t menu_row2[] = {
	{K_Q,		"Q" },
	{K_W,		"W" },
	{K_E,		"E" },
	{K_R,		"R" },
	{K_T,		"T" },
	{K_Y,		"Y" },
	{K_U,		"U" },
	{K_I,		"I" },
	{K_O,		"O" },
	{K_P,		"P" },
	{K_BR_LEFT,	"[" },
	{K_BR_RIGHT,	"]" },
	{0,		NULL }
};
static mdef_t menu_row3[] = {
	{K_A,		"A" },
	{K_S,		"S" },
	{K_D,		"D" },
	{K_F,		"F" },
	{K_G,		"G" },
	{K_H,		"H" },
	{K_J,		"J" },
	{K_K,		"K" },
	{K_L,		"L" },
	{K_SCOLON,	";" },
	{K_QUOTE,	"'" },
	{K_BSLASH,	"\\" },
	{0,		NULL }
};
static mdef_t menu_row4[] = {
	{K_GRAVE,	"`" },
	{K_Z,		"Z" },
	{K_X,		"X" },
	{K_C,		"C" },
	{K_V,		"V" },
	{K_B,		"B" },
	{K_N,		"N" },
	{K_M,		"M" },
	{K_COMMA,	"," },
	{K_POINT,	"." },
	{K_SLASH,	"/" },
	{0,		NULL }
};
static mdef_t menu_keypad[] = {
	{K_KP_1,	"1" },
	{K_KP_2,	"2" },
	{K_KP_3,	"3" },
	{K_KP_4,	"4" },
	{K_KP_5,	"5" },
	{K_KP_6,	"6" },
	{K_KP_7,	"7" },
	{K_KP_8,	"8" },
	{K_KP_9,	"9" },
	{K_KP_PLUS,	"+" },
	{K_KP_MINUS,	"-" },
	{K_KP_MULT,	"*" },
	{K_KP_SLASH,	"/" },
	{K_ENTER,	"Enter" },
	{K_KP_CLEAR,	"NumLock/Clear" },
	{0,		NULL }
};
static mdef_t menu_nav[] = {
	{K_BACKSPACE,	"Backspace" },
	{K_HOME,	"Home" },
	{K_END,		"End" },
	{K_PAGE_UP,	"Page Up" },
	{K_PAGE_DOWN,	"Page Down" },
	{K_HELP,	"Help/Insert" },
	{K_DEL,		"Delete" },
	{0,		NULL }
};
static mdef_t menu_misc[] = {
	{K_SPACE,	"Space" },
	{K_CAPSL,	"Caps Lock" },
	{K_TAB,		"Tab" },
	{K_START,	"Start/Power" },
	{0,		NULL }
};

enum { KM_MOD_KEYS=-100, KM_FKEYS, KM_ROW1, KM_ROW2, KM_ROW3, KM_ROW4, KM_KEYPAD, KM_NAV, KM_MISC,
	KM_SAVE, KM_EXIT, KM_MAIN_MENU, KM_EMPTY };

static mdef_t menu_main[] = {
	{ KM_MOD_KEYS,		"Modifier Keys" },
	{ KM_FKEYS,		"Function Keys" },
	{ KM_ROW1,		"Row 1 (123456...)" },
	{ KM_ROW2,		"Row 2 (qwerty...)" },
	{ KM_ROW3,		"Row 3 (asdfgh...)" },
	{ KM_ROW4,		"Row 4 (zxcvbn...)" },
	{ KM_KEYPAD,		"Keypad" },
	{ KM_NAV,		"PageUp etc." },
	{ KM_MISC,		"Misc" },
	{ KM_EMPTY,		"" },
	{ KM_SAVE,		"Save and Exit" },
	{ KM_EMPTY,		"" },
	{ KM_EXIT,		"Exit" },
	{ 0, NULL }
};
	
static mtab_t mtab[] = {
	{ KM_MAIN_MENU,	menu_main, 0 },
	{ KM_MOD_KEYS,	menu_mod, 0 },
	{ KM_FKEYS,	menu_fkeys, 0 },
	{ KM_ROW1,	menu_row1, 0 },
	{ KM_ROW2,	menu_row2, 0 },
	{ KM_ROW3,	menu_row3, 0 },
	{ KM_ROW4,	menu_row4, 0 },
	{ KM_KEYPAD,	menu_keypad, 0 },
	{ KM_NAV,	menu_nav, 0 },
	{ KM_MISC,	menu_misc, 0 },
	{ 0, NULL, 0 }
};

static int cur_menu = 0;

#define KEYTABLE_SIZE	256
typedef struct {
	int initialized;	/* has user configure key_xxx */

	int key_up, key_down, key_left, key_right, key_return, key_esc;
	int table[ KEYTABLE_SIZE ];
	int adb_to_keycode[ NUM_ADB_KEYCODES ];
} ktab_t;

static ktab_t keytables[ NUM_KEYCODE_TABLES ];
static ktab_t *ktab;

static int wait_key( int is_init );

/************************************************************************/
/*	Drawing functions						*/
/************************************************************************/

static void 
draw_pixel( int x, int y, int col )
{
	char *p = (char*)0x81000000 + tc.vmode.offset + y*tc.vmode.row_bytes;

	if( col < 0 || col >= NUM_COLORS )
		return;

	if( tc.vmode.depth != 8 )
		col = colortable[col];

	switch( tc.vmode.depth ) {
	case 8:
		*(p + x) = col;
		break;
	case 16: /* unused */
	case 15:
		*((short*)p + x)= ((col & 0xff) >> 3) | ((col & 0xff00) >> 6) 
				  | ((col & 0xff0000) >> 9);
		break;
	case 32:
	case 24:
		*((ulong*)p + x) = col;
		break;
	default:
		printm("Unsupported depth\n");
		break;
	}
}

static void
draw_char( char ch, int h, int v )
{
	char *c = (char *)fontdata;
	int x, y, xx, rskip, m;

	while( h >= tc.vmode.w/FONT_WIDTH || v >= tc.vmode.h/FONT_ADJ_HEIGHT )
		return;
	
	h *= FONT_WIDTH;
	v *= FONT_ADJ_HEIGHT;

	rskip = (FONT_WIDTH > 8)? 2 : 1;
	c += rskip * (unsigned int)ch * FONT_HEIGHT;

	for( x=0; x<FONT_WIDTH; x++ ){
		xx = x % 8;
		if( x && !xx )
			c++;
		m = (1<<(7-xx));
		for( y=0; y<FONT_HEIGHT; y++ ){
			int col;

			col = (c[rskip*y] & m) ? tc.fg_cind : tc.bg_cind;
			draw_pixel( h+x, v+y+1, col );
		}
		draw_pixel( h+x, v, tc.bg_cind );
		draw_pixel( h+x, v+FONT_HEIGHT+1, tc.bg_cind );
	}
}

static void
rec_char( char ch, unsigned char attrib )
{
	if( tc.h >= NCOLS || tc.v >= NROWS )
		return;

	tc.buf[tc.v][tc.h] = ch;
	tc.attr[tc.v][tc.h] = attrib;
}

static void
draw_line( int n )
{
	style_t *st = &styles[0];
	int i, attr=0;

	if( n >= NROWS )
		return;

	for( i=0; i<NCOLS; i++ ) {
		int f = tc.attr[n][i];

		if( (f & STYLES_MASK) )
			st = &styles[ (f & STYLES_MASK)-1 ];

		attr |= f;
		
		if( st ) {
			tc.fg_cind = st->fg_cind;
			tc.bg_cind = st->bg_cind;
			st = NULL;
		}
		draw_char( tc.buf[n][i], i+tc.hadd, n+tc.vadd );
	}
}


static void 
draw_str( char *s )
{
	int attr=0, i, attr_read=0;

	s--;
	while( *++s ) {
		if( attr_read ) {
			switch( *s ) {
			case '>':
				attr_read = 0;
				continue;
			case '<':
				rec_char( *s, attr );
				tc.h++;
				attr = 0;
				break;
			default:
				if( *s >= '0' && *s <= '6' )
					attr = (*s-'0') + 1;
				continue;
			}
		}
		switch( *s ) {
		case '<':
			attr_read = !attr_read;
			break;
		case '\b':
			while( tc.h < NCOLS-1 ) {
				rec_char( ' ', attr );
				attr = 0;
				tc.h++;
			}
			break;
		case '\t':
			for(i=0; i<4; i++ ) {
				rec_char( ' ', attr );
				attr = 0;
				tc.h++;
			}
			break;
		case '\r':
			attr = 0;
			tc.v++, tc.h=0;
			break;
		case '\n':
			while( tc.h < NCOLS ) {
				rec_char( ' ', attr );
				attr = 0;
				tc.h++;
			}
			tc.v++, tc.h=0;
			break;
		default:
			rec_char( *s, attr );
			attr = 0;
			tc.h++;
			break;
		}

	}
}

static void
clear_scr( void )
{
	int i;
	tc.h=tc.v=0;
	for( i=0; i<NCOLS; i++ )
		draw_str("\n");
	tc.h=tc.v=0;
}

static void
refresh( void )
{
	int i;
	
	/* draw border */
	for(i=0; i<NROWS; i++ ) {
		tc.buf[i][0] = tc.buf[i][NCOLS-1] = '*';
		tc.attr[i][0] = tc.attr[i][NCOLS-1] = 1;
	}
	for(i=0; i<NCOLS; i++ ) {
		tc.buf[0][i] = tc.buf[NROWS-1][i] = '*';
		tc.attr[0][i] = tc.attr[NROWS-1][i] = 1;
	}
	for(i=0; i<NROWS; i++ )
		draw_line(i);
}

static void
print_menu_( mtab_t *t, int offs, int active )
{
	char buf[200];
	mdef_t *m;
	int i;

	tc.v = 16;
	for( i=0, m=t->entries ; m->text; i++, m++ ) {
		int k, modified=0;

		k = t->entries[i].key;
		if( k>=0 && k<NUM_ADB_KEYCODES ) {
			int kcode = ktab->adb_to_keycode[k];
			if( kcode >=0 && k != ktab->table[kcode] )
				modified = 1;
		}
		tc.h = offs;
		sprintf( buf, "%s  %-20s <0>%s \n", 
			 (t->cur == i)? active ? "<5>" : "<4>"  : "<3>", m->text, 
			 modified ? "<0>  Modified" : "" );
		draw_str( buf );
	}
}

static void
print_menu( void )
{
	print_menu_( &mtab[0], 8, !cur_menu );
	draw_str("\n\n\n\n\n\n\n\n");
	if( cur_menu )
		print_menu_( &mtab[cur_menu], 34, 1 );

	tc.v=42;
	draw_str("   <1>The keys in the menus are ordered as they appear on a US keyboard.\n");
	draw_str("\n   <3>Note:<1> Fullscreen mode and X11 mode use different key mappings.\n");
	draw_str("   <1>Configure each mode individually.\n");
}


/************************************************************************/
/*	Initialization							*/
/************************************************************************/

static void
init_video( void )
{
	OSI_GetVModeInfo_( 0, 0, &tc.vmode );
	printm("VideoMode: %d x %d, Depth: %d\n", tc.vmode.w, tc.vmode.h,  tc.vmode.depth );
	
	tc.width = tc.vmode.w / FONT_HEIGHT;
	tc.height = tc.vmode.h / FONT_ADJ_HEIGHT;

	tc.hadd = (tc.width - NCOLS) / 2;
	tc.vadd = (tc.height - NROWS) / 2;

	tc.fg_cind = 1;
	tc.bg_cind = 0;

	if( tc.vmode.depth == 8 ){
		int i;
		for(i=0; i<NUM_COLORS; i++ )
			OSI_SetColor( i, colortable[i] );
		OSI_SetColor( -1, 0);
	}
}

static void
init_keytable( void )
{
	int i,j;
	for( j=0; j<NUM_KEYCODE_TABLES; j++ ) {
		int b = keycode_index_to_bit(j);
		for( i=0; i<KEYTABLE_SIZE; i++ )
			keytables[j].table[i] = OSI_KeycodeToAdb( i | b );
		for( i=0; i<NUM_ADB_KEYCODES; i++ )
			keytables[j].adb_to_keycode[i] = -1;
	}
	ktab = NULL;
}

static void
head( void )
{
	draw_str("\n");
	draw_str("\n   Mac-on-Linux Keyboard Configurator\n" );
	draw_str("\n");
}

static void
initialize_kbd( void )
{
	int key;
	clear_scr();
	head();
	draw_str("   <1>The purpose of this tool is making sure the keyboard layout is\n" );
	draw_str("   <1>correct. Simply follow the instructions.\n\n" );	

	draw_str("   <1>First, certain essential keys will be configured:\n\n\n" );	
	
	draw_str("   <2>\tPress [Esc] " );
	if( (key=wait_key(1)) < 0 )
		return;
	ktab->key_esc = key;

	draw_str("   <2>\tPress [Return] " );
	if( (key=wait_key(1)) < 0 )
		return;
	ktab->key_return = key;
	
	draw_str("   <2>\tPress [Arrow Up] " );
	if( (key=wait_key(1)) < 0 )
		return;
	ktab->key_up = key;

	draw_str("   <2>\tPress [Arrow Down] " );
	if( (key=wait_key(1)) < 0 )
		return;
	ktab->key_down = key;
	
	draw_str("   <2>\tPress [Arrow Left] " );
	if( (key=wait_key(1)) < 0 )
		return;
	ktab->key_left = key;

	draw_str("   <2>\tPress [Arrow Right] " );
	if( (key=wait_key(1)) < 0 )
		return;
	ktab->key_right = key;

	draw_str("\n   <2>Press [Return] to continue or [ESC] to start over ");
	if( (key=wait_key(1)) < 0 )
		return;
	if( key != ktab->key_return ) {
		initialize_kbd();
		return;
	}
	ktab->adb_to_keycode[ K_ESC ] = ktab->key_esc;
	ktab->adb_to_keycode[ K_LEFT ] = ktab->key_left;
	ktab->adb_to_keycode[ K_RIGHT ] = ktab->key_right;
	ktab->adb_to_keycode[ K_UP ] = ktab->key_up;
	ktab->adb_to_keycode[ K_DOWN ] = ktab->key_down;
	ktab->adb_to_keycode[ K_RET ] = ktab->key_return;

	ktab->initialized = 1;
}

static int
wait_key( int is_init ) {
	int ind, key, raw;
	ktab_t *old_ktab = ktab;
	
	if( is_init ) {
		draw_str("<3>\1");
		refresh();
	}

	do {
		while( ((key=OSI_GetAdbKey2( &raw )) < 0) || (key & 0x80) )
			OSI_USleep(10000);
		ind = keycode_to_type_index( raw );
		if( raw < 0 || (raw & ~KEYCODE_TYPE_MASK) >= KEYTABLE_SIZE ) {
			printm("keycode out of range (%d)\n", raw & ~KEYCODE_TYPE_MASK );
			ind = -1;
		}
	} while( ind < 0 );
	ktab = &keytables[ind];

	if( !old_ktab )
		old_ktab = ktab;

	if( is_init ) {
		tc.h--; 
		draw_str(" \n");
	}
	if( !old_ktab->initialized && ktab->initialized==1 )
		return -1;
	if( ktab->initialized != 1 ) {
		/* handle console switch during initialization */
		if( old_ktab != ktab || !is_init ) {
			initialize_kbd();
			return -1;
		}
	}
	return raw & ~KEYCODE_TYPE_MASK;
}

static int
do_exit( void )
{
	int i, j, dirty=0;

	for(i=0; i<NUM_KEYCODE_TABLES; i++ ) {
		if( !keytables[i].initialized )
			continue;
		for(j=0; j<NUM_ADB_KEYCODES; j++) {
			int k = keytables[i].adb_to_keycode[j];
			dirty |= (k != -1 && keytables[i].table[k] != j);
		}
	}

	if( !dirty )
		return 1;
	tc.v = 30;
	tc.h = 0;
	draw_str("  <2>Warning: Changes have not been saved.\n\n");
	draw_str("  <2>Press [<1>RETURN<2>] to exit or [<1>Esc<2>] to abort. \1\n\n\n\n\n\n");
	refresh();
	return (wait_key(0) == ktab->key_return) ? 1:0;
}

static void
do_save( void )
{
	int i,j, m;
	for( i=0; i<NUM_KEYCODE_TABLES; i++ ){
		ktab_t *k = &keytables[i];
		if( !k->initialized )
			continue;
		for( j=0; j<NUM_ADB_KEYCODES; j++ ) {
			int adbcode = j;

			for( m=0; key_aliases[m].alias ; m++ )
				if( key_aliases[m].alias == j )
					adbcode = key_aliases[m].real_adb;

			if( k->adb_to_keycode[j] != -1 )
				OSI_MapAdbKey( k->adb_to_keycode[j] | keycode_index_to_bit(i), adbcode );
		}
	}
	OSI_SaveKeymapping();
}

static int
select_item( int key )
{
	int i;

	for( i=0; mtab[i].key; i++ ){
		if( mtab[i].key == key ){
			cur_menu = i;
			return 0;
		}
	}
	switch( key ) {
	case KM_SAVE:
		do_save();
		return 1;
	case KM_EXIT:
		return do_exit();
	}
	return 0;
}

static int
mainloop( void )
{
	mtab_t *t;
	int k, ret=0;

	for(;;) {
		t = &mtab[cur_menu];
		k = wait_key(0);

		/* new keysource has been configured need to redraw screen */
		if( k < 0 ) {
			ret = 2;
		} else if( k == ktab->key_esc ) {
			if( !cur_menu )
				ret = do_exit();
			cur_menu = 0;
		} else if( k == ktab->key_left ) {
			cur_menu = 0;
		} else if( k == ktab->key_up ) {
			while( t->cur && t->entries[--t->cur].key == KM_EMPTY )
				;
		} else if( k == ktab->key_down ) {
			while( t->entries[++t->cur].key == KM_EMPTY )
				;
			if( t->entries[t->cur].text == NULL )
				t->cur--;
		} else if( k == ktab->key_right ) {
			if( !cur_menu )
				ret = select_item( t->entries[t->cur].key );
		} else if( k == ktab->key_return ) {
			ret = select_item( t->entries[t->cur].key );
		} else if( cur_menu && t->entries[t->cur].key >=0 ) {
			int j;
			/* keycode configuration */
			for( j=0; j<NUM_ADB_KEYCODES; j++ )
				if( ktab->adb_to_keycode[j] == k )
					ktab->adb_to_keycode[j] = -1;
			ktab->adb_to_keycode[t->entries[t->cur].key] = k;

			/* next item */
			while( t->entries[++t->cur].key == KM_EMPTY )
				;
			if( t->entries[t->cur].text == NULL )
				t->cur--;
		}
		if( ret )
			break;

		print_menu();
		refresh();
	}
	return ret;
}

int
entry( void )
{
	init_video();
	init_keytable();

	initialize_kbd();
	do {
		clear_scr();
		head();
	
		draw_str("   <3>Instructions:<1> Select the key which is not correctly mapped using the\n");
		draw_str("   <1>arrows and then press the correct key (arrow right selects a menuentry).\n\n");
		draw_str("   <3>Note:<1> If a non-US keyboard is used then select the us key which\n");
		draw_str("   <1>shares the same physical position. For instance, the Swedish '\02' key\n");
		draw_str("   <1>is located where US keyboards have the semicolon. Thus, to configure '\02',\n");
		draw_str("   <1>one selects ';' in the menu before typing '\02'.\n");
		draw_str("\n\n");

		print_menu();
		refresh();
	} while( mainloop() != 1 );
	
	clear_scr();
	refresh();
	return 0;
}
