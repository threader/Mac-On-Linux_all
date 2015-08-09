/**************************************************************
*   
*   Creation Date: <97/07/26 18:23:02 samuel>
*   Time-stamp: <2003/06/02 12:37:06 samuel>
*   
*	<escc.c>
*	
*	Emulation of the Z8530 serial controller chip
*
*   Copyright (C) 1997, 1999, 2000, 2002, 2003  Samuel Rydh
*
*   This program is free software; you can redistribute it and/or
*   modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation;
*
**************************************************************/

#include "mol_config.h"

#include "ioports.h"
#include "promif.h"
#include "debugger.h"
#include "verbose.h"
#include "gc.h"
#include "driver_mgr.h"
#include "booter.h"
#include "dbdma.h"

/*
 * It appears as if the escc chip is accessible from a second
 * hardware address, 0xf3012000 (OF states 0xf3013000)
 * Not, the same register layout though
 *
 * port B:
 *   0xf3012000 / 0xf130000	read/write REGs
 *   0xf3012004 / 0xf130010	read/write DATA
 *
 * port A:
 *   0xf3012010 / 0xf3013020	read/write REGs
 *   0xf3012014 / 0xf3013030	read/write DATA
 */

#define SCC_GC_OFFS		0x13000
#define SCC_SIZE		0x40

#define SCC_2_GC_OFFS		0x12000
#define SCC_2_SIZE		0x40

/* register bit definitions */
enum {
	WR0_CRC_CMD_MASK	= 0xc0, 
	WR0_CMD_MASK 		= 0x38,
	WR0_REG_MASK		= 0x7,
	
	WR0_CMD_POINT_HIGH	= 0x8
};

/* We must allow second range hardware access,
 * or MacOS will get stuck in the booting.
 */

static struct {
	gc_range_t  	*range;
	gc_range_t	*range2;

	int		r;
} scc[1];


/************************************************************************/
/*	I/O								*/
/************************************************************************/

static void 
wreg( int r, int ch, char val )
{
	if( r==0 && !(val & 0xf0) )
		return;
	/* printm( "[%c]: Write %-2d: %02X\n", ch? 'B':'A', r, val ); */
}

static char 
rreg( int r, int ch )
{
	char val = 0;
	
	switch( r ){
	case 0:
		val = 0x54;
		break;
	case 1:
		val = 7;
		break;
	case 10:
		val = 0;
		break;
	}
	/* printm( "[%c]: Read %-2d: %02X\n", ch? 'B':'A', r, real_val ); */
	return val;
}

static ulong 
io_read( ulong ptr, int len, void *usr )
{
	return 0;
}

static void 
io_write( ulong ptr, ulong data, int len, void *usr )
{

}

static ulong 
io_read2( ulong ptr, int len, void *usr ) 
{
	int ch = (ptr & 0x2) >> 1;
	char v=0;
	
	/* v2 = ioread( 0x80812000 + (ptr&0xf), 1 ); */
	
	switch( (ptr>>2)&3 ) {
	case 0:
		v = rreg( scc->r, ch );
		break;
	case 1:
		v = 0xff;
		break;
	case 2:
		v = 0;
		break;
	}
	scc->r = 0;
	return v;
}

static void 
io_write2( ulong ptr, ulong d, int len, void *usr ) 
{
	int ch = (ptr & 0x2) >> 1;

	/* iowrite( 0x80812000 + (ptr&0xf), d, 1 ); */
	switch( (ptr>>2)&3  ){
	case 0:
		if( !scc->r ) {
			scc->r = d & WR0_REG_MASK;
			if( (d & WR0_CMD_MASK) == WR0_CMD_POINT_HIGH )
				scc->r += 8;
			wreg( 0, ch, d );
			return;
		}
		wreg( scc->r, ch, d );
		break;
	case 1:
	case 2:
		break;
	}
	scc->r = 0;	/* or? */
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static io_ops_t ops = {
	.read	= io_read,
	.write	= io_write
};
static io_ops_t ops2 = {
	.read	= io_read2,
        .write	= io_write2
};

static int
escc_init( void )
{
	mol_device_node_t *dn;
	gc_bars_t bars;
	
	if( !gc_is_present() )
		return 0;

	scc->range = add_gc_range( SCC_GC_OFFS, SCC_SIZE, "scc", 0, &ops, NULL );
	scc->range2 = add_gc_range( SCC_2_GC_OFFS, SCC_2_SIZE, "scc-2", 0, &ops2, NULL );

	/* this hack keeps MacOS 9 happy */
	if( (dn=prom_find_devices("ch-b")) && is_gc_child(dn, &bars) && bars.nbars ) {
		int offs = bars.offs[bars.nbars-1];
		allocate_dbdma( offs, "escc-dma", (offs>>8) & 0xf, NULL, 0 );
	}
	return 1;
}

static void
escc_cleanup( void )
{
}

driver_interface_t escc_driver = {
    "escc", escc_init, escc_cleanup
};
