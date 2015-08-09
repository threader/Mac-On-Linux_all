/* 
 *   Creation Date: <2000/12/09 16:16:40 samuel>
 *   Time-stamp: <2003/10/23 02:31:27 samuel>
 *   
 *	<keycodes.c>
 *	
 *	Handles different keyboard layouts and user customization
 *   
 *   Copyright (C) 2000, 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   Based upon initial work by Ian Reinhart Geiser <geiseri@yahoo.com>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/param.h>
#include "keycodes.h"
#include "debugger.h"
#include "res_manager.h"
#include "driver_mgr.h"
#include "mol_assert.h"
#include "os_interface.h"
#include "booter.h"
#include "mac_registers.h"

int _uses_linux_keycodes;

#define USE_LINUX_KEYCODES_RES		"use_linux_keycodes"
#define SHOW_KEY_TRANS_RES		"show_key_trans"


static char *key_res_names[ NUM_KEYCODE_TABLES ] = {
	"remap_key",
	"remap_xkey",
	"remap_vnckey"
};

static char *file_res_names[ NUM_KEYCODE_TABLES ] = {
	"kbd_prefs",
	"xkbd_prefs",
	"vnckbd_prefs"
};

#define UNK		0xff

/* 127 == Start Button */
static const unsigned char linux_to_adb_keycodes[128] = {
	/* 0 */	UNK,  53,  18,  19,  20,  21,  23,  22,  26,  28,
		 25,  29,  27,  24,  51,  48,  12,  13,  14,  15,
	/*20*/	 17,  16,  32,  34,  31,  35,  33,  30,  36,  54,
	 	  0,   1,   2,   3,   5,   4,  38,  40,  37,  41,
	/*40*/	 39,  50,  56,  42,   6,   7,   8,   9,  11,  45,
		 46,  43,  47,  44, 123,  67,  58,  49,  57, 122,
	/*60*/  120,  99, 118,  96,  97,  98, 100, 101, 109,  71,
		107,  89,  91,  92,  78,  86,  87,  88,  69,  83,
	/*80*/	 84,  85,  82,  65, UNK, 105,  50, 103, 111, UNK,
		UNK, UNK, UNK, UNK, UNK, UNK,  76,  54,  75, 105,
	/*100*/	 58, UNK, 115,  62, 116,  59,  60, 119,  61, 121,
		114, 117, UNK, UNK, UNK, 107, UNK,  81, UNK, 113,
	/*120*/	UNK, UNK, UNK, UNK, UNK,  55,  55, UNK
};

typedef struct {
	char	*table;
	int 	max;		/* num keycodes in table */
	int	min;		/* lowest keycode in use */
} keytable_t;

static keytable_t ktab[ NUM_KEYCODE_TABLES ];

static int	initialized;
static int	verbose_key_trans;


int
register_key_table( int ktype, int min, int max )
{
	keytable_t *kt = &ktab[ktype];
	char *p;
	int i, size;
	
	assert( initialized && !kt->table && min <= max );

	size = max-min+1;
	p = kt->table = malloc( size );
	kt->max = max;
	kt->min = min;

	memset( p, 0xff, size );
	for( i=0; i<size; i++, p++ ) {
		if( _uses_linux_keycodes && i<sizeof(linux_to_adb_keycodes) )
			*p = linux_to_adb_keycodes[i];
		if( !_uses_linux_keycodes && i<128 )
			*p = i;
	}
	return 0;
}

static int
keycode_to_adb( int ktype, int keycode )
{
	keytable_t *kt = &ktab[ktype];
	int ret;

	assert(kt->table);
	if( keycode > kt->max || keycode < kt->min ) {
		/* FIXME This is noisy now due to the usb/keycode fix */
//		printm("Keycode %d out of range\n", keycode );
		keycode = kt->min;
	}

	ret = kt->table[ keycode - kt->min ];
	ret = (ret == 0xff)? -1 : (ret & 0x7f);

	if( verbose_key_trans )
		printm("keycode %3d (0x%02x) --> ADB-code %3d (0x%02x)\n",
		       keycode, keycode, ret, ret);
	return ret;
}

void
set_keycode( int ktype, int keycode, int adbcode )
{
	keytable_t *kt = &ktab[ktype];

	assert(kt->table);

	if( keycode > kt->max || keycode < kt->min ) {
		printm("keycode %d (table %d) out of range (%d...%d)\n", keycode, ktype, kt->min, kt->max );
		return;
	}
	if( adbcode >= 128 || adbcode < 0 ) {
		printm("Adbcode %d out of range (max=127)\n", adbcode);
		return;
	}
	kt->table[keycode-kt->min] = adbcode;
}

static void
parse_kbd_files( int ktype, char *res_name )
{
	char *name;
	unsigned int a,b;
        int n, r, ind=0;
	FILE *f;

	while( (name=get_filename_res_ind( res_name, ind++, 0 )) ) {
		if( !(f = fopen( name, "r" ) )) {
			printm("Could not open '%s'\n", name );
			continue;
		}
		n=0;
		while( (r=fscanf(f, "0x%x: 0x%x ", &a, &b )) != EOF) {
			if( r != 2 ) {
				printm("Parse error in '%s' (%d pairs read)\n", name, n );
				break;
			}
			n++;
			set_keycode( ktype, a, b );
			/* printm("KEYREMAP: %d --> %d\n", a, b ); */
		}
		fclose(f);
	}
}

static void
parse_kbd_resources( int ktype, char *res_name )
{
	unsigned int a,b, ind=0;
	char *s1,*s2;
	int err;

	while( (s1=get_str_res_ind( res_name, ind, 0 )) ) {
		err = !(s2=get_str_res_ind( res_name, ind++, 1 ));

		if( err || !(sscanf(s1,"0x%x", &a) || sscanf(s1,"%d",&a)) 
		    || !(sscanf(s2,"0x%x",&b) || sscanf(s2,"%d", &b)) )
			err++;

		if( err ) {
			printm("Bad keyremap resource.\n");
			printm("Correct syntax: 'keyremap: old_keycode new_keycode'\n");
		} else {
			set_keycode( ktype, a, b );
			printm("KEYREMAP: %d --> %d\n", a, b );
		}
	}
}

int 
user_kbd_customize( int ktype ) 
{
	parse_kbd_files( ktype, file_res_names[ktype] );
	parse_kbd_resources( ktype, key_res_names[ktype] );
	return 0;
}


/************************************************************************/
/*	key event handling						*/
/************************************************************************/

static struct {
	uint		keydowns[4];		/* 128 bits */
	uint		mol_keydowns[4];	/* MOL keydown mask */
	key_action_t	*actions;
} kev;

static void
remove_key_action( key_action_t *action )
{
	key_action_t **pp;
	for( pp=&kev.actions; *pp && *pp != action ; pp=&(**pp).next )
		;
	if( *pp == action )
		*pp = action->next;
}

static void
add_key_action( key_action_t *action )
{
	remove_key_action( action );
	action->next = kev.actions;
	kev.actions = action;
}

void
add_key_actions( key_action_t *t, int size )
{
	int i;
	for( i=0; i<size/sizeof(key_action_t); i++ )
		add_key_action( &t[i] );
}

void
remove_key_actions( key_action_t *t, int size )
{
	int i;
	for( i=0; i<size/sizeof(key_action_t); i++ )
		remove_key_action( &t[i] );
}

static int
key_is_down( int code )
{
	if( code < 0 )
		return 1;
	for( ; code > 0 ; code=code>>8 )
		if( kev.mol_keydowns[code & 3] & (1 << ((code >> 2) & 0x1f)) )
			return 1;
	return 0;
}

static int
handle_mol_keycomb( int code, int is_keypress ) 
{
	key_action_t *p;
	int drop=0;
	
	for( p=kev.actions; p; p=p->next ) {
		int m1 = key_is_down(p->mod1), m2 = key_is_down(p->mod2);
		if( m1 && m2 && is_keypress && code == p->key ) {
			p->fire = 1;
			drop = 1;
			continue;
		}
		if( p->fire && !m1 && !m2 && p->action(p->key, p->usr_val) )
			break;
	}
	for( ; p ; p=p->next )
		p->fire = 0;

	return drop;
}

void
key_event( int ktype, int keycode, int is_keypress )
{
	int code = (keycode & ~KEYCODE_TYPE_MASK);
	uint *p, b;

	if( (code=keycode_to_adb(ktype, code)) < 0 )
		return;
	
	p = &kev.keydowns[ code & 3 ];
	b = 1 << (code >> 2);

	/* keep track of keydowns without for mol key-shortcuts */
	if( is_keypress )
		kev.mol_keydowns[ code & 3 ] |= b;
	else
		kev.mol_keydowns[ code & 3 ] &= ~b;

	/* handle MOL key combinations */
	if( handle_mol_keycomb(code, is_keypress) )
		return;

	/* simulate old-style caps lock */
	if( !is_osx_boot() && code == KEY_CAPSLOCK ) {
		if( !is_keypress )
			return;
		is_keypress = !(b & *p);
	}

	if( !gPE.adb_key_event )
		return;
	
	if( is_keypress && !(b & *p) ) {
		if( !gPE.adb_key_event(code, keycode) )
			*p |= b;
	} else if( !is_keypress && (b & *p) ) {
		if( !gPE.adb_key_event(code | 0x80, keycode) )
			*p &= ~b;
	}
}

void
zap_keyboard( void )
{
	uint *p, b;
	int i;

	for( i=0; i<128; i++ ) {
		p = &kev.keydowns[ i & 3 ];
		b = 1 << ((i & 0x7f) >> 2);
		if( (*p & b) )
			gPE.adb_key_event( i | 0x80, 0 );
		*p &= ~b;
	}
}


/************************************************************************/
/*	OSI interface for layout configuration				*/
/************************************************************************/

/* int keycode_to_adb( int raw_keycode ) 
 * Ret values: -1 == key not configured, -2 == invalid keycode, -3 == bad table
 */
static int
osip_keycode_to_adb( int sel, int *params )
{
	int table=-1;
	int keycode = params[0] &~KEYCODE_TYPE_MASK;
	
	table = keycode_to_type_index( params[0] );
	if( table < 0 )
		return keycode;
	assert( table < NUM_KEYCODE_TABLES );

	if( !ktab[table].table )
		return -3;
	if( keycode < ktab[table].min || keycode > ktab[table].max )
		return -2;
	return keycode_to_adb( table, keycode );
}

/* void osip_map_adb_key( keycode, adbkey ) */
static int
osip_map_adb_key( int sel, int *params )
{
	int table, keycode = params[0];
	int adbcode = params[1];

	table = keycode_to_type_index( params[0] );
	if( table < 0 )
		return 0;

	set_keycode( table, keycode &~ KEYCODE_TYPE_MASK, adbcode );
	return 0;
}

static int
osip_save_keymapping( int sel, int *params )
{
	int i,j;
	
	for( i=0; i<NUM_KEYCODE_TABLES; i++ ) {
		char *name;
		FILE *file;
		
		if( !ktab[i].table )
			continue;

		if( !(name=get_filename_res(file_res_names[i])) ) {
			printm("Missing resource: %s. File not saved\n",
			       file_res_names[i]);
			continue;
		}
		if( !(file=fopen(name, "w")) ) {
			printm("Error saving keymap to %s. Save aborted.\n", name);
			continue;
		}
		for( j=ktab[i].min; j<=ktab[i].max; j++ ){
			int adb = ktab[i].table[j-ktab[i].min];
			if( adb != 0xff )
				fprintf(file, "0x%02x: 0x%02x\n", j, adb );
		}
		fclose( file );
		printm("Keymapping sucessfully saved to '%s'\n", name );
	}
	return 0;
}

static const osi_iface_t osi_iface[] = {
	{ OSI_KEYCODE_TO_ADB,	osip_keycode_to_adb	},
	{ OSI_MAP_ADB_KEY,	osip_map_adb_key	},
	{ OSI_SAVE_KEYMAPPING,	osip_save_keymapping	},
};

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static int
keycodes_init( void )
{
	FILE *f;

	_uses_linux_keycodes = 1;
	verbose_key_trans = get_bool_res(SHOW_KEY_TRANS_RES) == 1;
	
	if( !get_bool_res(USE_LINUX_KEYCODES_RES) )
		_uses_linux_keycodes = 0;
	else if( (f=fopen("/proc/sys/dev/mac_hid/keyboard_sends_linux_keycodes", "r")) ) {
		if( fgetc(f) == '0' )
			_uses_linux_keycodes = 0;
		fclose(f);
	}
	register_osi_iface( osi_iface, sizeof(osi_iface) );
	initialized = 1;
	return 1;
}

static void
keycodes_cleanup( void )
{
	int i;
	for( i=0; i<NUM_KEYCODE_TABLES; i++ )
		if( ktab[i].table )
			free( ktab[i].table );
}

driver_interface_t keycodes_driver = {
    "keycodes", keycodes_init, keycodes_cleanup
};
