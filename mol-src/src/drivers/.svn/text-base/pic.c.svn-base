/* 
 *   Creation Date: <97/07/02 19:51:35 samuel>
 *   Time-stamp: <2003/12/15 22:16:27 samuel>
 *   
 *	<pic.c>
 *	
 *	Interrupt Controller
 *	(the one found in the Grand Central (GC) chip)
 *   
 *   Copyright (C) 1997, 1999-2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <pthread.h>

#include "pic.h"
#include "molcpu.h"
#include "ioports.h"
#include "promif.h"
#include "debugger.h"
#include "driver_mgr.h"
#include "gc.h"
#include "session.h"
#include "byteorder.h"
#include "booter.h"
#include "timer.h"

/* #define PIC_DBG_CMDS */

#define PIC_GC_OFFSET		0x10

#define GC_LEVEL_MASK		0x3ff00000
#define OHARE_LEVEL_MASK	0x1ff00000
#define HEATHROW_LEVEL_MASK	0x1ff00000

#define NUM_REGS		8
enum{
	r_event=0, r_enable, r_ack, r_level
};
static char *reg_names[] = {
	"EVENT_2", "ENABLE_2", "ACK_2", "LEVEL_2", "EVENT", "ENABLE", "ACK", "LEVEL"
};

/* This pmac-interrupt controller generates an interrupt on 
 * the positive *AND* negative edge of the signal from the device. 
 * (Update: Is this TRUE?)
 */

/* XXX: How should writes to LEVEL register be handled? 
 * Is LEVEL latched if ENABLE is cleared? Then a bit written to LEVEL
 * should probably release the latch.
 */
/* G3-machines have 64 interrupts, older machines have 32 */

/* The PIC has two cells; the first handles irq 32-64 
 * (G3 and later) and the second handles irq 0-31.
 */
typedef struct {
	/* hw regs */
	int		event;
	int		enable;
	int		ack;
	int		level;

	/* all irqs can be shared by at most two sources */
	int		src[2];
	int		in_use[2];
} pic_regs_t;

static struct {
	pthread_mutex_t lock_mutex;
	gc_range_t	*pic_range;
	pic_regs_t	r[2];

	int		cpu_irq;
} pic;

static int 		oldworld_hack;
irq_controller_t	g_irq_controller;

#define LOCK		pthread_mutex_lock( &pic.lock_mutex )
#define UNLOCK		pthread_mutex_unlock( &pic.lock_mutex )

#define R1		pic.r[1]
#define R2		pic.r[0]



static int
save_pic_regs( void )
{
	return write_session_data( "PIC", 0, (char*)pic.r, sizeof(pic.r) );
}

static void 
intline_upd( void )
{
	int raise = (R1.event & R1.enable) || (R2.event & R2.enable);
	set_cpu_irq( raise, &pic.cpu_irq );
}

static void
pic_io_write( ulong addr, ulong data_le, int len, void *usr )
{
	ulong mbase = get_mbase( pic.pic_range );
	ulong data = ld_le32( &data_le );
	int reg = (addr - mbase)>>2;
	pic_regs_t *r = (reg>3) ? &R1 : &R2;

	LOCK;
	switch( reg & 3 ) {
	case r_enable:
		r->enable = data;
		/* we should NOT clear event bits! */
		intline_upd();
		break;
	case r_ack:
		/* ACK of the highest bit (at least in R1) clears both event register. */
		if( data & MOL_BIT(0) ) {
			R1.event = 0;
			R2.event = 0;
		} else {
			r->event &= ~data;
		}
		intline_upd();
		break;
	case r_level:
		/* XXX: This actually happens in the oldworld setting. */
		/* printm("PIC: write to level register (%08lx)\n", data); */
		break;
	case r_event:
		printm("PIC: write to event register (%08lx)\n", data);
		break;
	}
	UNLOCK;
}

static ulong
pic_io_read( ulong addr, int len, void* usr )
{
	ulong data, mbase = get_mbase( pic.pic_range );
	int reg = (addr - mbase)>>2;
	pic_regs_t *r = (reg>3) ? &R1 : &R2;
	
	LOCK;
	switch( reg & 3 ) {
	case r_enable:
		data = r->enable;
		break;
	case r_event:
		data = r->event;
		break;
	case r_level:
		data = r->level;
		break;
	case r_ack:
		data = 0;
		break;
	}
	UNLOCK;
	return ld_le32(&data);
}


static void 
pic_io_print( int isread, ulong addr, ulong data, int len, void *usr )
{
	int i, reg = (addr - get_mbase(pic.pic_range))>>2;
	ulong val = ld_le32( &data );

	printm( "PIC %s %s : [ ", isread? "read " : "write", reg_names[reg] );

	i = reg>3 ? 0 : 32;
	for( ; val ; val=val>>1, i++ )
		if( val & 1 )
			printm("%d ", i );
	printm("]\n");
}

static int
source_register( int *irq )
{
	pic_regs_t *r = (*irq & 32) ? &R2 : &R1;
	ulong b = (1UL << (*irq & 31));
	int ind = (r->in_use[0] & b) ? 1:0;

	if( r->in_use[ind] & b ) {
		printm("---> IRQ %d is already shared!\n", *irq );
		return 1;
	}
	r->in_use[ind] |= b;
	if( ind )
		*irq += 64;

	/* printm("IRQ %d registered\n", *irq ); */
	return 0;
}

static void
source_hi( int irq )
{
	pic_regs_t *r = (irq & 32) ? &R2 : &R1;
	ulong b = (1UL << (irq & 31));
	int ind = (irq & 64) ? 1:0;

	if( irq < 0 )
		return;
	LOCK;
	r->src[ind] |= b;
	if( !(r->level & b) ) {
		r->level |= b;
		r->event |= b;
		intline_upd();
	}
	UNLOCK;
}

static int 
source_low( int irq )
{
	pic_regs_t *r = (irq & 32) ? &R2 : &R1;
	ulong b = (1UL << (irq & 31));
	int old, ind = (irq & 64) ? 1:0;

	if( irq < 0 )
		return 0;
	LOCK;
	/* XXX: This is clearly the wrong thing to do. */
	if( oldworld_hack && (b & r->level) ) {
		r->event |= (b & r->enable);
		intline_upd();
	}
	old = r->src[ind] & b;
	r->src[ind] &= ~b;
	r->level &= ~b | r->src[!ind];
	UNLOCK;

	return old;
}


/************************************************************************/
/*	CMDs & DEBUG							*/
/************************************************************************/

static void __dcmd
print_irq_nums( ulong mask, int add )
{
	for( ; mask ; add++, mask=mask>>1 )
		if( mask & 1 )
			printm("%d ", add );
}

static int __dcmd
cmd_pic_regs( int argc, char **argv )
{
	if( argc != 1 )
		return -1;

	printm( "}\nEnable:  { ");
	print_irq_nums( R1.enable, 0 );
	print_irq_nums( R2.enable, 32 );
	printm( "}\nEvent:   { ");
	print_irq_nums( R1.event, 0 );
	print_irq_nums( R2.event, 32 );
	printm( "}\nLevel:   { ");
	print_irq_nums( R1.level, 0 );
	print_irq_nums( R2.level, 32 );
	printm( "}\n");
	return 0;
}

#ifdef PIC_DBG_CMDS
static int __dcmd
cmd_rirq( int argc, char **argv )
{
	printm("level %d, enable %d\n", R1.level, R1.enable );
	R1.event = R1.level & R1.enable;
	R2.event = R2.level & R2.enable;	
	intline_upd();
	return 0;
}

static int __dcmd
cmd_mask0( int argc, char **argv )
{
	R1.enable = 0;
	return 0;
}

static int __dcmd
cmd_raise( int argc, char **argv )
{
	int r;
	if( argc != 2 )
		return -1;
	r = atoi(argv[1]);
	source_hi( r );
	R1.enable |= (1 << r);
	printm("Raising IRQ %d\n", r );
	return 0;
}

static int __dcmd
cmd_level( int argc, char **argv )
{
	int r;
	if( argc != 2 )
		return -1;
	r = atoi(argv[1]);
	R1.level |= (1 << r);
	R1.enable |= (1 << r);
	printm("Setting level %d\n", r );
	return 0;
}

static int __dcmd
cmd_event( int argc, char **argv )
{
	int r;
	if( argc != 2 )
		return -1;
	r = atoi(argv[1]);
	R1.event |= (1 << r);
	R1.enable |= (1 << r);
	printm("Setting IRQ event %d\n", r );
	intline_upd();
	return 0;
}

static int __dcmd
cmd_nmi( int argc, char **argv )
{
	ulong bit = 1UL << 20;
	int v = R1.level & bit;
	
	if( !v )
		irq_line_hi( 20 /* NMI */);
	else
		irq_line_low( 20 /* NMI */);

	printm("NMI: %d\n", !v );
	return 0;
}
#endif /* PIC_DBG_CMDS */

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static irq_controller_t gc_pic = {
	.irq_hi		= source_hi,
	.irq_low        = source_low,
	.register_irq	= source_register
};
static io_ops_t ops = {
	.read	      	= pic_io_read,
	.write		= pic_io_write,
	.print		= pic_io_print
};

static int
pic_init( void )
{
	mol_device_node_t *dn = prom_find_type("interrupt-controller");

	if( !is_oldworld_boot() && !is_gc_child(dn, NULL) )
		return 0;

	pthread_mutex_init( &pic.lock_mutex, NULL );
	pic.pic_range = add_gc_range( PIC_GC_OFFSET, NUM_REGS * 4, "PIC", 0, &ops, NULL );

	session_save_proc( save_pic_regs, NULL, kDynamicChunk );
	if( loading_session() ) {
		if( read_session_data( "PIC", 0, (char*)pic.r, sizeof(pic.r) ) )
			session_failure("Could not read PIC registers\n");
		intline_upd();
	}
	oldworld_hack = is_oldworld_boot();

	add_cmd( "pic_regs", "pic_regs \ndisplay PIC regs\n", -1, cmd_pic_regs );
#ifdef PIC_DBG_CMDS
	add_cmd( "nmi", "dtc\nRaise NMI\n", -1, cmd_nmi );
	add_cmd( "rirq", "rirq\nReissue irq\n", -1, cmd_rirq );
	add_cmd( "mask0", "rirq\nMask all irqs\n", -1, cmd_mask0 );
	add_cmd( "raise", "rirq\nRaise irq\n", -1, cmd_raise );
	add_cmd( "level", "rirq\nLevel\n", -1, cmd_level );
	add_cmd( "event", "rirq\nEvent\n", -1, cmd_event );
#endif
	/* register our controller */
	g_irq_controller = gc_pic;
	return 1;
}

static void 
pic_cleanup( void )
{
	pthread_mutex_destroy( &pic.lock_mutex );
}

driver_interface_t pic_driver = {
	"pic", pic_init, pic_cleanup
};
