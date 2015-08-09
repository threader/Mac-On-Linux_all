/**************************************************************
*   
*   Creation Date: <97/07/04 21:06:20 samuel>
*   Time-stamp: <2003/06/02 12:30:10 samuel>
*   
*	<awacs.c>
*	
*	AWACS driver (Audio Waveform Amplifier and Converter)
*   
*   Copyright (C) 1997-01, 03 Samuel Rydh
*
*   This program is free software; you can redistribute it and/or
*   modify it under the terms of the GNU General Public License
*   as published by the Free Software Foundation;
*
*   This is a very crude driver, mostly written for
*   debugging the dbdma code. The startup boing works though.
*
**************************************************************/

#include "mol_config.h"
#include "driver_mgr.h"

/* #define ENABLE_AWACS */

#ifdef ENABLE_AWACS
#include <linux/soundcard.h>
#include "promif.h"
#include "ioports.h"
#include "promif.h"
#include "dbdma.h"
#include "thread.h"
#include "debugger.h"
#include <sys/ioctl.h>

#include "gc.h"

/* #define DUMP_ROM_BELL */

/* constants which allow us to hear the startup boing */
#define SND_SPEED	44100
#define SND_FORMAT	AFMT_S16_BE


struct awacs_state {
	int	inited;
	int	irq, txdma_irq, rxdma_irq;

	int	dsp_fd;
	int	rombell_fd;
};

static struct awacs_state as[1];

static int awacs_init( void );
static void awacs_cleanup( void );
static void player_entry( void *dummy );


driver_interface_t awacs_driver = {
    "awacs", awacs_init, awacs_cleanup
};


/************************************************************************/
/*	FUNCTIONS							*/
/************************************************************************/

int
awacs_init( void )
{
	mol_device_node_t *dn;
	gc_bars_t bars;
	
	printm("******** WARNING: AWACS enabled ***********\n");

	/* --- Extract information from PROM --- */
	if( !(dn=prom_find_devices("awacs")) )
		dn = prom_find_devices("davbus");
	if( !dn || !is_gc_child(dn, &bars) ) {
		printm("No AWACS sound controller found\n");
		return 0;
	}
	if( dn->bars.nbars != 3 || dn->n_intrs != 3 ) {
		printm("AWACS: Expecting 3 addrs and 3 intrs.\n");
		return 0;
	}
	as->inited = 1;

	as->irq = dn->intrs[0].line;
	as->txdma_irq = dn->intrs[1].line;
	as->rxdma_irq = dn->intrs[2].line;

	as->dsp_fd = open("/dev/dsp", O_WRONLY  /* | O_NONBLOCK */  ); 

	if( as->dsp_fd == -1 ) {
		printm("Failed to open /dev/dsp\n");
	} else {
		ulong speed = SND_SPEED;
		ulong format = SND_FORMAT;

       		ioctl( as->dsp_fd, SNDCTL_DSP_SPEED, &speed );
		ioctl( as->dsp_fd, SNDCTL_DSP_SETFMT, &format );
	}

	/* dn->addrs[i]: 0 = regs, 1 = tx_dma, 2 = rx_dma */
	allocate_dbdma( bars.offs[1], "AWACS-txdma", as->txdma_irq, NULL, 0 );
	create_thread( player_entry, NULL, "AWACS-thread" );
	
#ifdef DUMP_ROM_BELL
	printm("Dumping sound to /tmp/rombell\n");
	as->rombell_fd = open("/tmp/rombell", O_WRONLY | O_CREAT, 0644 );
#endif
	printm("AWACS sound driver installed\n");
	return 1;
}

static void
awacs_cleanup( void )
{
	if( !as->inited )
		return;

	release_dbdma( as->txdma_irq );

	if( as->dsp_fd != -1)
		close( as->dsp_fd );
}


/* Play sound. This is mostly for testing. A real driver should
 * double buffered transfer.
 */
static void
player_entry( void *dummy )
{
	struct dma_read_pb pb;
	ulong speed = SND_SPEED;
	ulong format = SND_FORMAT;
	size_t len;
	char *src;

	static int is_working=0;

	/* No output device? */
	for( ;; ) {
		while( dma_read_req( as->txdma_irq, &pb) )
			dma_wait( as->txdma_irq, DMA_READ, NULL );

		if( as->dsp_fd == -1 ) {
			dma_read_ack( as->txdma_irq, pb.req_count, &pb );
			continue;
		}

		if( !is_working ) {
			ioctl( as->dsp_fd, SNDCTL_DSP_SPEED, &speed );
			ioctl( as->dsp_fd, SNDCTL_DSP_SETFMT, &format );
		}

		len = pb.req_count - pb.res_count;
		src = pb.buf + pb.res_count;

		while( len>0 ) {
			int ret;
			
			ret = write( as->dsp_fd, src, len );
#ifdef DUMP_ROM_BELL
			if( as->rombell_fd != -1 && ret>0 )
				write( as->rombell_fd, src, ret );
#endif
			if( ret< 0 && (errno == EAGAIN || errno == EINTR ))
				continue;
			if( ret< 0 ){
				perrorm("AWACS: write");
				return;
			}
			src += ret;
			len -= ret;
		}
		dma_read_ack( as->txdma_irq, pb.req_count, &pb );
		is_working = !pb.is_last;
	}

	/* NEVER comes here */
}

#else /* ENABLE_AWACS */

driver_interface_t awacs_driver = {
    "awacs-disabled", NULL, NULL
};

#endif 
