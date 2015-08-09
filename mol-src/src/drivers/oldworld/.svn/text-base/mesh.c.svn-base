/* 
 *   Creation Date: <1999-03-12 harry>
 *   
 *	<mesh.c>
 *	
 *	Driver for the  MESH SCSI controller
 *	(internal SCSI chain on machines with two controllers)
 *   
 *   Copyright (C) 1999, harry eaton <haceaton@jhuapl.edu>
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/param.h>
#include <pthread.h>

#include "molcpu.h"
#include "extralib.h"
#include "ioports.h"
#include "promif.h"
#include "dbdma.h"
#include "pic.h"
#include "debugger.h"
#include "gc.h"
#include "thread.h"
#include "driver_mgr.h"

#include "scsi-bus.h"
#include "mesh_hw.h"
#include "scsi_main.h"

/* #define VERBOSE */

/* if you define STRANGE_INT, better define BY_THE_BOOK too */
/* #define BY_THE_BOOK */
/* #define STRANGE_INT */

/* This implementation is based upon the information found in
 * "Macintosh Technology in the Common Hardware Reference Platform"
 * of course it was changed slightly to conform to what the
 * real mesh actually does...
 */

/* S8..S0 bits in DMA_STATUS */
#define DMA_SB_CMD_DONE		0x80	/* ??? From MESH  */
#define DMA_SB_EXCEPTION	0x40	/* ??? From MESH  */
#define DMA_SB_DREQ		0x20
#define DMA_SB_EXTERNAL_MASK	0x1f	/* or only bit s0 ? */

static void 	do_command( unsigned char cmd );
static char	dma_status_bits( int irq, int mask, char data );
static void	reset_chip( void );
static void	end_cmd( int );
static void	overflow_fifo( int );
static void	sphase_changed( int old_sphase, int new_sphase, void *usr );
static int	bus_data_in( int count, char *buf, int may_block, int sphase, void *usr );
static int	bus_data_req( int count, char *buf, int may_block, int sphase, void *usr );
static void	xfer_from_fifo( char *buf, int num_bytes );

static char *reg_names[NUM_REGS] = {
	"COUNT_LO", "COUNT_HI", "FIFO", "SEQUENCE", "BUS_STATUS_0", "BUS_STATUS_1",
	"FIFO_COUNT", "EXCEPTION", "ERROR", "INTR_MASK", "INTERRUPT",
	"SOURCE_ID", "DEST_ID", "SYNC_PARAMS", "MESH_ID", "SEL_TIMEOUT"
};
static char *cmd_names[128] = {"NOP",
	"Arb","Sel","Cmd","Status","DataOut","DataIn","MsgOut","MsgIn","BusFree",
	"EnParChk","DisParChk","EnReSel","DisReSel","RstMESH","FlushFIFO"
};

#define WARN_MISALIGN(len) \
	if(len!=1) { printm(": Misaligned access ignored\n"); }

/* a big fifo helps reduce threading - we lie to macos about it though */
#define FIFO_SIZE	1024
#define FIFO_MASK   	(FIFO_SIZE-1)

/* phase */
enum { 
	idling=0, reading, writing
};

static struct {
	gc_range_t     	*regs_range;

	int		irq, dma_irq;

	pthread_mutex_t	lock_mutex;

	int		deferred;
	int		phase;
	int		cmd_dma;

	int		sphase_guarded;		/* true if sphase is monitored */

	int		sphase;		/* curent sphase (scsi bus phase) */
	int		req_count;
	int		res_count;
	int		done;
	int		book;

	struct scsi_unit *unit;

		/* mesh registers */
		/* the transfer hi/lo count is combined into an int */
	unsigned short	cur_xfer_count;
	unsigned char	fifo[FIFO_SIZE];
	unsigned char	sequence;
	unsigned char	bus_status0;
	unsigned char	bus_status1;
	int		fifo_count;
	unsigned char	exception;
	unsigned char	error;
	unsigned char	intr_mask;
	unsigned char	interrupt;
	unsigned char	source_id;
	unsigned char	dest_id;
	unsigned char	sync_parms;
	unsigned char	mesh_id;
	unsigned char	sel_timeout;
	unsigned short	fifo_bot;	/* where in the fifo we are */
	unsigned char	irq_line;

	/* dma status byte (S8..S0) */
	unsigned char	dma_sb;
	unsigned char	dma_sb_ext;		/* external controlled bits */
} sc[1];

#define LOCK	pthread_mutex_lock( &sc->lock_mutex );
#define UNLOCK	pthread_mutex_unlock( &sc->lock_mutex );

static scsi_unit_pb_t unit_pb = {
	0,1,0,
	sphase_changed,
	bus_data_req,
	bus_data_in,
	bus_data_req,
	bus_data_in,
	bus_data_req,
	bus_data_in,
	NULL, NULL, NULL
};

	
int 
mesh_available( void )
{
	return prom_find_devices( "mesh" ) != NULL;
}

static void
reset_chip( void )
{
#ifdef VERBOSE
	printm("*** MESH CHIP RESET ***\n");
#endif
	sc->cur_xfer_count = 0;
	sc->sequence = 0;
	sc->bus_status0 = 0;
	sc->bus_status1 = 0;
	sc->fifo_count = 0;
	sc->exception = 0;
	sc->error = 0;
	sc->intr_mask = 0;
	sc->interrupt = INT_CMDDONE;
	sc->source_id = 7; /* should get set to 7 later too */
	sc->dest_id = 0;
	sc->sync_parms = 0;
	sc->mesh_id = 0xe2; /* the Ohare mesh id value */
	sc->sel_timeout = 0;
	sc->fifo_bot = 0;
	sc->irq_line = 0;
	sc->done = 1;
	sc->req_count = 0;
	sc->deferred = 0;
	sc->book = 0;
}


static ulong
reg_io_read( ulong addr, int len, void *usr )
{
	int 	r = ( addr - get_mbase(sc->regs_range)) >>4;
	ulong 	ret=0;

	LOCK;
	
	WARN_MISALIGN(len);
	switch(r){
	case r_count_lo: 	/* 0 */
		ret = sc->cur_xfer_count & 0xff;
		break;
	case r_count_hi: 	/* 1 */
		ret = (sc->cur_xfer_count >> 8) & 0xff;
		break;
	case r_fifo: 		/* 2 */
		if( sc->fifo_count > 16 )
			sc->cur_xfer_count--;
		if( sc->fifo_count ) {
			ret = sc->fifo[sc->fifo_bot];
			sc->fifo_bot = (sc->fifo_bot + 1) & FIFO_MASK;
			sc->fifo_count--;
		}
#ifdef VERBOSE
		else
			printm("MESH: Reading empty fifo\n");
#endif
	/* continue transfer once fifo is unloaded */
		if( !sc->fifo_count && sc->deferred ){
			sc->deferred = 0;
			UNLOCK;
			scsi_xfer_status( sc->unit, 1 );
			LOCK;
		}
	/* the docs claim both the fifo count and xfer count
	 * must go to zero for command completion, but
	 * it seems only the xfer count is necessary.
	 */
#ifdef BY_THE_BOOK
		if( (!sc->fifo_count || !sc->book) && !sc->cur_xfer_count && !sc->done ){
#else
	/* but we don't want to instantly assert the interrupt upon reading
	 * the n-16th byte - macos is confused by being so far behind the
	 * fifo
	 */
		if( !sc->cur_xfer_count && !sc->done && sc->fifo_count < 8 ){
#endif
			sc->interrupt |= INT_CMDDONE;
			end_cmd(1);
		}
		break;
	case r_sequence:		/* 3 */
		ret = sc->sequence;
		break;

	case r_bus_status_0:		/* 4 */
		ret = sc->bus_status0;
		break;

	case r_bus_status_1:		/* 5 */
		ret = sc->bus_status1;
		break;

	case r_fifo_count:		/* 6 */
		if( sc->fifo_count > 16 ) {
			ret = 16;
#ifdef VERBOSE
			printm("fifo[%d] ", sc->fifo_count);
#endif
		}
		else
			ret = sc->fifo_count;
		break;
	case r_exception:		/* 7 */
		ret = sc->exception;
		break;
	case r_error:			/* 8 */
		ret = sc->error;
		break;
	case r_intr_mask:		/* 9 */
		ret = sc->intr_mask;
		break;
	case r_interrupt:		/* 10 */

	/* Sigh, it seems that the macos sometimes makes a request
	 * then intentionally lets the fifo overflow, but still waits
	 * for the xfer count to drop to zero for the interrupt.
	 * So far, it always seems to query the interrupt register
	 * while it's waiting for this overflow. Consequently we
	 * check for this condition, dump the fifo and let the
	 * "transfer" proceed.
	 */
		if( sc->deferred )
			sc->deferred++;
	/* wait until we're really sure it wants overflow */
		if( sc->deferred > 65536 ) {
			printm("MESH: allowing fifo overflow through neglect to empty\n");
			overflow_fifo( 1 );
		}
		ret = sc->interrupt;
		if( sc->error )
			ret |= 0x4;
		if( sc->exception )
			ret |= 0x2;
		break;
	case r_source_id:		/* 11 */
		ret = sc->source_id;		/* should always be 7 */
		break;
	case r_dest_id:			/* 12 */
		ret = sc->dest_id;
		break;
	case r_sync_parms:		/* 13 */
		ret = sc->sync_parms;	/* prevent syncronous mode */
	case r_mesh_id:			/* 14 */
		ret = sc->mesh_id;
		break;
	case r_sel_timeout:		/* 15 */
		ret = sc->sel_timeout;
		break;
#ifdef VERBOSE
	default:
		printm("accessing unknown MESH register offset 0x%x\n",r);
		stop_emulation();
#endif
	}
#ifdef VERBOSE
	if( r != r_fifo )
	printm("MESH register %s returned 0x%lx\n", reg_names[r], ret);
#endif
	UNLOCK;
	return ret;
}

static void 
reg_io_write( ulong addr, ulong data, int len, void *usr )
{
	int r = (addr - get_mbase( sc->regs_range))>>4;
	WARN_MISALIGN(len);
	data &= 0xff;
#ifdef VERBOSE
	printm("0x%lx to MESH register %s\n", data, reg_names[r]);
#endif
	LOCK;	
	switch(r) {
	case r_count_lo: 	/* 0 */
		sc->cur_xfer_count = (sc->cur_xfer_count & 0xff00) | data;
		sc->req_count = sc->cur_xfer_count;
		if( !sc->req_count )
			sc->req_count += 0x10000;
		break;

	case r_count_hi: 	/* 1 */
		sc->cur_xfer_count = (sc->cur_xfer_count & 0xff) | (data<<8);
		sc->req_count = sc->cur_xfer_count;
		if( !sc->req_count )
			sc->req_count += 0x10000;
		break;

	case r_fifo:	    	/* 2 */
#ifdef VERBOSE
		if( sc->fifo_count == FIFO_SIZE )
			printm("MESH fifo overflow!\n");
#endif
		sc->fifo_bot = (sc->fifo_bot-1) & FIFO_MASK;
		sc->fifo[sc->fifo_bot] = data;
		sc->fifo_count++;
		sc->cur_xfer_count--;
			/* indicate data available when it's all there */
		if( !sc->cur_xfer_count || sc->fifo_count == FIFO_SIZE ) {
			sc->deferred = 0;
			scsi_xfer_status( sc->unit, 1 );
		}
/* this case fits by-the-book either way */
		if( !sc->cur_xfer_count ){
			sc->interrupt |= INT_CMDDONE;
			end_cmd(2);
		}
		break;

	case r_sequence:		/* 3 */
		sc->interrupt &= 0xfe;  /* clear cmd-done bit */
		if( data & SEQ_TARGET ){
			printm("MESH received unimplemented TARGET-mode command\n");
			stop_emulation();
		}
		sc->sequence = data;
		scsi_set_atn( data & SEQ_ATN );
		sc->done = 0;
		do_command( data );
		break;

	case r_bus_status_0:
		sc->bus_status0 = data;
#ifdef VERBOSE
		printm("WARNING MESH bus status register 0 being written\n");
#endif
		break;

	case r_bus_status_1:
		sc->bus_status1 = data;
			/* scsi bus reset */
		if( data & BS1_RST ){
			sc->sphase_guarded = 0;
			scsi_reset();
				/* scsi-bus should cause this to happen */
			sc->sphase = sphase_bus_free;
		}
#ifdef VERBOSE
		else if( data )
			printm("WARNING MESH bus status register 1 being written\n");
#endif
		break;

	case r_fifo_count:
#ifdef VERBOSE
		printm("WARNING MESH FIFO count being written to \n");
#endif
		sc->fifo_count = data;
		break;

	case r_exception:
		sc->exception &= ~data;
		break;

	case r_error:
		sc->error &= ~data;
		break;

	case r_intr_mask:
		sc->intr_mask = data;
		break;

	case r_interrupt:
		if( data & INT_EXCEPTION )
			sc->exception = 0;
		if( data & INT_ERROR )
			sc->error = 0;
		if( sc->irq_line & data & sc->intr_mask ){
#ifdef VERBOSE
			printm("MESH: de-asserting interrupt\n");
#endif
			irq_line_low( sc->irq );
			sc->irq_line = 0;
		}
		sc->interrupt &= ~data;
		break;
			
	case r_source_id:
#ifdef VERBOSE
		if( data != 7 )
			printm("WARNING: MESH source ID being set to %ld\n", data);
#endif
		sc->source_id = data & 0x7;

	case r_dest_id: 	/* 4 */
		sc->dest_id = data & 0x7;
		break;

	case r_sync_parms:
#ifdef VERBOSE
		if( sc->sync_parms & 0xf1 )
			printm("WARNING: MESH set for unsupported syncronous transfer\n");
#endif
		sc->sync_parms = data & 0x7;
		break;

	case r_mesh_id:
		printm("ERROR: wrote to MESH mesh_id register!\n");
		break;

	case r_sel_timeout:
		sc->sel_timeout = data;
		break;
	default:
#ifdef VERBOSE	
		printm("MESH: Write to unimplemented register %d\n",r);
		stop_emulation();
#endif
		break;
	}
	UNLOCK;
}

static void 
reg_io_print( int isread, ulong addr, ulong data, int len, void *usr )
{
	int r = (addr - get_mbase(sc->regs_range))>>4;

	r &= 0xf;
	if( isread )
		printm("MESH: %-10s read  %02lX ", reg_names[r], data );
	else
		printm("MESH: %-10s write %02lX ", reg_names[r], data );

	if( !isread && r == r_sequence ){
		char *cmdstr = cmd_names[ data & ~SEQ_DMA_MODE ];
		printm("%s <%s>", data & SEQ_DMA_MODE ? "DMA" : "   ",
		       cmdstr? cmdstr : "<__unknown__>");
	}
	printm("\n");
}



/* Implementation of the S0..S7 bits in the dbdma STATUS register.
 * (and they *ARE* used)
 *
 * FIXME: doesn't this belong in the dbdma code?
 */
static char
dma_status_bits( int irq, int mask, char data ) 
{
	unsigned char ret;
	
	mask &= DMA_SB_EXTERNAL_MASK;
	ret = (sc->dma_sb_ext & ~mask) | (data & mask);
	ret |= sc->dma_sb & ~DMA_SB_EXTERNAL_MASK;

	return ret;
}

static void
do_command( unsigned char cmd )
{
	int ret, advance;
	int dma = cmd & SEQ_DMA_MODE;
	int atn = cmd & SEQ_ATN;

	advance = 0;
	sc->sphase_guarded = 0;
	sc->phase = idling;
	sc->cmd_dma = dma;
	if( atn )
		sc->bus_status0 |= BS0_ATN;
	else
		sc->bus_status0 &= ~BS0_ATN;
	cmd &= SEQ_CMD;	/* isolate command bits */
#ifdef VERBOSE
	printm("MESH: Doing command: %s %s %s\n", cmd_names[ cmd ], dma ? "[DMA]" : "", atn ? "[ATN]" : "" );
#endif
	switch( cmd ){
	case SEQ_RESETMESH:
		reset_chip();
		return;

	case SEQ_FLUSHFIFO:
			/* no interrupt */
		sc->fifo_count = 0;
		sc->fifo_bot = 0;
		return;
	case SEQ_ARBITRATE:
		UNLOCK;
		ret = scsi_arbitration( sc->unit );
		LOCK;
		switch( ret ){
		case -1:
			printm("MESH arbitration aborted?!\n");
			sc->exception |= EXC_PHASEMM;
			break;
		case 0: /* won arbitration */
#ifdef VERBOSE
			printm("MESH won arbitration\n");
#endif
			sc->interrupt |= INT_CMDDONE;
			break;
		case 1: /* arbitration lost */
			sc->exception |= EXC_ARBLOST;
			break;
		}
		break;
	case SEQ_SELECT:
#ifdef VERBOSE
		printm("MESH: Select, TARGET_ID = %d\n", sc->dest_id );
#endif
		UNLOCK;
		ret = scsi_selection( sc->unit, sc->dest_id, atn );
		LOCK;
		switch( ret ){
		case -1:
			printm("MESH: phase mismatch during selection\n");
			sc->exception |= EXC_PHASEMM;
			break;
		case 0:
			sc->bus_status1 |= BS1_BSY;
			sc->interrupt |= INT_CMDDONE;
			break;
		case 1:
			sc->exception |= EXC_SELTO;
			break;
		}
		break;
	case SEQ_COMMAND:
			sc->sphase_guarded = sphase_command;
			sc->res_count = 0;
			sc->phase = writing;
			sc->book = 1;
		break;
	case SEQ_STATUS:
			sc->sphase_guarded = sphase_status;
			sc->phase = reading;
			sc->res_count = 0;
			advance = 1;
			sc->book = 0;
		break;
	case SEQ_DATAOUT:
			sc->sphase_guarded = sphase_data_out;
			sc->res_count = 0;
			sc->phase = writing;
			if( dma )
				advance = 1;
			sc->book = 1;
		break;
	case SEQ_DATAIN:
			sc->sphase_guarded = sphase_data_in;
			sc->phase = reading;
			sc->res_count = 0;
			advance = 1;
			sc->book = 1;
		break;
	case SEQ_MSGOUT:
			sc->sphase_guarded = sphase_mes_out;
			sc->res_count = 0;
			sc->phase = writing;
			if( dma )
				advance = 1;
			sc->book = 0;
		break;
	case SEQ_MSGIN:
			sc->sphase_guarded = sphase_mes_in;
			sc->phase = reading;
			sc->res_count = 0;
			advance = 1;
			sc->book = 0;
		break;
	case SEQ_BUSFREE:
		if( sc->sphase != sphase_bus_free )
			sc->sphase_guarded = sphase_bus_free;
		else
			sc->interrupt |= INT_CMDDONE;
		/* make sure bus engine is running */
		advance = 1;
		break;
	default:
#ifdef VERBOSE
		printm("MESH WARNING: Unimplemented command [%s]\n", cmd_names[ cmd ] );
#endif
		break;
	}
		/* release ACK */
#ifdef VERBOSE
		printm("MESH: dropping ack\n");
#endif
		sc->bus_status0 &= ~BS0_ACK;
	if( advance ) {
#ifdef VERBOSE
		printm("MESH: allowing phase advance\n");
#endif
		sc->deferred = 0;
		UNLOCK;
		scsi_xfer_status( sc->unit, 1 );
		LOCK;
	}
	end_cmd(0);
}

static void
end_cmd( int unguard )
{
#ifdef STRANGE_INT
	int cmd;
#endif

	if( unguard ){
#ifdef VERBOSE
		printm( "MESH: Unguarding sphase %d\n", sc->sphase_guarded );
#endif
		if( sc->cur_xfer_count == 0 ) {
			sc->sphase_guarded = 0;
				/* advance the phase if not MsgOut
				 * the temporary "restoring" of phase
				 * can cause mismatch exceptions
				 * otherwise */
			if( sc->sequence != 8 )
				scsi_xfer_status( sc->unit, 1 );
		}
		/* we also switch to idling to defer reads/writes
		 * until we know what to expect.
		 */
	}
		/* The mesh documentation says that all error and exception
		 * conditions set cmd_done, but it's not clear about if
		 * cmd_done being set this way causes an interrupt.
		 * It would seem that you wouldn't need three mask bits
		 * if that were the case...
		 *
		 * It also says that if the xfer count is zero before a phase
		 * change that there is no "exception" interrupt. It goes on
		 * to imply that there will be no interrupts during "normal" 
		 * sequencing.  I suppose that could be easily achieved by
		 * masking the cmd_done interrupt, but they don't say anything
		 * about that being required.
		 *
		 * it's all very unclear, but I'm about to try masking the
		 * the cmd_done interrupt for data transfer commands
		 * even though the mask register does not mask them.  It is
		 * a bizarre interpretation, but maybe that's what it wants
		 */
	sc->irq_line = (sc->error && (sc->intr_mask & INT_ERROR)) ? INT_ERROR : 0;
	sc->irq_line |= (sc->exception && (sc->intr_mask & INT_EXCEPTION)) ? INT_EXCEPTION : 0;
#ifdef STRANGE_INT
	cmd = sc->sequence & SEQ_CMD;
	if( cmd != SEQ_COMMAND && cmd != SEQ_STATUS && cmd != SEQ_DATAOUT
	   && cmd != SEQ_DATAIN && cmd != SEQ_MSGOUT && cmd != SEQ_MSGIN )
#endif
	sc->irq_line |= (sc->interrupt & sc->intr_mask & INT_CMDDONE);
	if( sc->error | sc->exception )
		sc->interrupt |= INT_CMDDONE;
	if( !sc->done && sc->interrupt ){
#if 0
if( sc->irq_line != 1 )
	printm("MESH: cmd %s ending with error 0x%x\n",cmd_names[sc->sequence & 0xf],sc->irq_line);
#endif
		sc->done = 1;
			/* if xfer done, go to idle mode - necessary to stop other threads */
		if( sc->req_count == sc->res_count)
			sc->phase = idling;
		if( sc->irq_line ){
				irq_line_hi( sc->irq );
#ifdef VERBOSE
				printm("MESH: asserting interrupt %s\n", sc->irq_line != 1 ? "[ERR]" : "[DONE]" );
		} else if( sc->interrupt )
			printm("MESH: cmddone, but interrupts disabled\n");
#else
		}
#endif
	}
}
	
/* 
 * SCSI Bus interface
 * 
 */

static void 
sphase_changed( int old_sphase, int new_sphase, void *usr )
{
	LOCK;
#ifdef VERBOSE
	printm("MESH: phase change: %d -> %d\n", old_sphase, new_sphase );
#endif
	sc->sphase = new_sphase;
	sc->bus_status0 &= ~BS0_PHASE;
	sc->bus_status0 |= (new_sphase & BS0_PHASE);
	if( new_sphase == sphase_bus_free ){
#ifdef VERBOSE
		printm("MESH: bus freeing\n");
#endif
		if( sc->sequence == SEQ_BUSFREE ){
			sc->interrupt |= INT_CMDDONE;
			end_cmd(3);
		} else if( old_sphase != sphase_arbitration && (sc->bus_status1 & BS1_BSY) ){
			sc->phase = idling;
			sc->error |= ERR_UNEXPDISC;
			end_cmd(3);
		}
		sc->bus_status1 &= ~BS1_BSY;
		sc->bus_status0 = 0;
	} else if( sc->fifo_count > 16 ){
		/* if not looking for a bus-free (i.e. disconect
		 * reselect) then dump the fifo contents on
		 * phase change, otherwise the macos sometimes gets
		 * confused as to why more data fills
		 * the (overflowed) fifo after the phase changes
		 */
#ifdef VERBOSE
		printm("MESH: fifo overflow due to phase mismatch\n");
#endif
		overflow_fifo( 0 );
	}
	if( sc->sphase_guarded && sc->sphase_guarded != new_sphase &&
	    sc->sphase_guarded != sphase_bus_free ){
		if( sc->cur_xfer_count ) {
			sc->exception |= EXC_PHASEMM;
		} else {
#ifdef BY_THE_BOOK
			if( !sc->book )
#endif
			sc->interrupt |= INT_CMDDONE;
		}
		end_cmd(4);
	}
		/* inhibit multiple phase changes */
	if( old_sphase == 7 )
		scsi_xfer_status( sc->unit, 0 );
	if( !sc->sphase_guarded )
		sc->phase = idling;
	sc->bus_status0 &= ~BS0_REQ;
	UNLOCK;
}

static void overflow_fifo( int affect_count )
{
	int lose;

	sc->deferred = 0;
	sc->res_count = 0;
	if( sc->fifo_count > 16 ) {
			/* bitch!  the roll-over of the fifo count is
			 * very important to the macos when overflowing
			 * but I don't know how it genuinely behaves
			 */
		/* lose = sc->fifo_count - (sc->fifo_count % 17); */
		lose = sc->fifo_count - 16;
		if( affect_count )
			sc->cur_xfer_count -= lose;
		sc->fifo_bot = (sc->fifo_bot + lose) & FIFO_MASK;
		sc->fifo_count = sc->fifo_count - lose;;
	}
	UNLOCK;
	scsi_xfer_status( sc->unit, 1 );
	LOCK;
}


static int
bus_data_in( int count, char *buf, int may_block, int sphase, void *usr )
{
	struct dma_write_pb pb;
	int avail, requested;
	int end_it;
	
	LOCK;
#ifdef VERBOSE
	printm("MESH: pid[%d] scsi bus_data_in: %d",getpid(), count );
#endif

	if( !( sc->bus_status0 & BS0_ACK ) )
		sc->bus_status0 |= BS0_REQ;
	/* We detect phase changes here */
	if( sc->sphase != sphase  ){
		scsi_xfer_status( sc->unit, 0 /* halt */ );
		sphase_changed(sc->sphase, sphase, NULL);
	}

	if( sc->phase != reading ) {
#ifdef VERBOSE
		printm(" - deferring\n");
#endif
		scsi_xfer_status( sc->unit, 0 /*no data*/ );
		if( sc->sphase_guarded == sphase_bus_free ){
			sc->exception |= EXC_PHASEMM;
			end_cmd(8);
		}
		UNLOCK;
		return ERR_NO_DATA;
	}
#ifdef VERBOSE
	else printm(" - PROCESSING\n");
#endif
	if( sc->sphase_guarded && sc->sphase_guarded != sc->sphase ) {
#ifdef VERBOSE
		printm("MESH: persistant phase mismatch\n");
#endif
		sc->exception |= EXC_PHASEMM;
		printm("MESH: fifo overflow due to persistant phase mismatch\n");
		overflow_fifo( 0 );
		end_cmd(20);
		scsi_xfer_status( sc->unit, 0 );
		UNLOCK;
		return ERR_NO_DATA;
	}
	end_it = 0;
	requested = count;

	avail = sc->req_count - sc->res_count;
	if( count > avail )
		count = avail;
	if( sc->cmd_dma ) {
		while( dma_write_req( sc->dma_irq, &pb ) ){
			UNLOCK;
			if( !may_block )
				return ERR_MAY_BLOCK;

			sc->dma_sb |= DMA_SB_DREQ;
			dbdma_sbits_changed( sc->dma_irq );

			dma_wait( sc->dma_irq, DMA_WRITE, NULL );

			sc->dma_sb &= ~DMA_SB_DREQ;
			dbdma_sbits_changed( sc->dma_irq );
			LOCK;
		}
		if( count > pb.req_count - pb.res_count )
			count = pb.req_count - pb.res_count;


		if( sc->fifo_count )
			printm("***** MESH: sending dma while fifo has data\n");

		/* XXX internal state could have changed here! Due to UNLOCK above*/
		if( dma_write_ack( sc->dma_irq, buf, count, &pb ) )
			count = 0;

		/* DMA counter */
		sc->cur_xfer_count -= count;
		sc->res_count += count;
		if( count && !sc->cur_xfer_count ){
			sc->interrupt |= INT_CMDDONE;
			end_it = 1;
		}
	} else {
		int i, fifo_top=0;
			/* when fifo fills, we refuse more - and request a new thread */
		if( !may_block && count > FIFO_SIZE - sc->fifo_count ){
			UNLOCK;
			return ERR_MAY_BLOCK;
		}
		for(i=0; i<count && i < ( FIFO_SIZE - sc->fifo_count ); i++, buf++ ){
			fifo_top = (sc->fifo_bot + sc->fifo_count) & FIFO_MASK;
			sc->fifo[fifo_top] = *buf;
			sc->fifo_count++;
		}
		count = i;
		sc->res_count += count;
		/* Decrement the xfer count, but don't suggest fifo overflow.
		 * When the fifo is read, if the count is > 16, we'll decrement
		 * the xfer_count as the fifo is read. This allows a large fifo
		 * in order to reduce the number of xfer calls between
		 * the target and the mesh.
		 */
		sc->cur_xfer_count -= (count > 16 ? 16 : count);
		if( sc->cur_xfer_count <= 0
#ifdef BY_THE_BOOK
		  && !sc->book ){
#else
		){
#endif
			sc->interrupt |= INT_CMDDONE;
			end_it = 1;
		}
	}
		/* Hmm, it is claimed that ACK is held except for
		 * syncronous transfers.  DataIn, DataOut are the
		 * only types of sync transfers.  But it seems
		 * they don't hold ACK even when not doing sync
		 * transfers. Because of the fake fifo length
		 * we halt it anyway for fifo transfers, and
		 * restart the phase when the fifo makes the xfer
		 * count go to 0.
		 */
	if( sc->req_count == sc->res_count && !sc->cmd_dma ){
		if( requested != count ){
			/* pause for fifo unloading */
#ifdef VERBOSE
			printm("MESH: pausing for fifo unload\n");
#endif
			sc->deferred = 1;
		} else {
#ifdef VERBOSE
			printm("MESH: holding ACK to stop next phase\n");
#endif
		}
		sc->bus_status0 &= ~BS0_REQ;
		sc->bus_status0 |= BS0_ACK;
		scsi_xfer_status( sc->unit, 0 );
	}
		/* drop BS0_REQ at end of DMA transfer */
	if( requested == count && sc->cmd_dma )
		sc->bus_status0 &= ~BS0_REQ;

#ifdef VERBOSE
	printm("MESH: scsi bus_data_in [%d] (req %d, res %d)\n",count, 
	       sc->req_count, sc->res_count );
	printm("MESH: bus status 0 = 0x%02x  [%d]\n",sc->bus_status0, id++);
#endif
	/* don't pass interrupt before bus status is set up properly */
	if( end_it )
		end_cmd(6);
	UNLOCK;
	return count;
}

static int
bus_data_req( int req_count, char *buf, int may_block, int sphase, void *usr )
{
	struct dma_read_pb pb;
	int avail;
	int dma_count, fifo_count;
	
	LOCK;
#ifdef VERBOSE
	printm("MESH: [fifo %d] bus_data_req: %d",sc->fifo_count, req_count );
#endif

	/* We detect phase changes here */
	if( sc->sphase != sphase  ){
		scsi_xfer_status( sc->unit, 0 /* halt */ );
		sphase_changed( sc->sphase, sphase, NULL );
	}

	if( sc->phase != writing  || (!sc->cmd_dma && sc->fifo_count == 0)) {
#ifdef VERBOSE
		printm(" - deferring\n");
#endif
		sc->deferred = 1;
		scsi_xfer_status( sc->unit, 0 /*no data*/ );
		if(!(sc->bus_status0 & BS0_ACK ))
			sc->bus_status0 |= BS0_REQ;
		if( sc->sphase_guarded == sphase_bus_free ){
			sc->exception |= EXC_PHASEMM;
			end_cmd(8);
		}
		UNLOCK;
		return ERR_NO_DATA;
	}
#ifdef VERBOSE
	else printm(" PROCESSING\n");
#endif

	avail = sc->req_count - sc->res_count;
	
	/* From FIFO. */
	fifo_count = MIN( req_count, sc->fifo_count );

	xfer_from_fifo( buf, fifo_count );
	buf += fifo_count;
	req_count -= fifo_count;
	
	/* From DMA */
	dma_count = 0;
	if( sc->cmd_dma ) {
		int n;
		
		while( dma_read_req( sc->dma_irq, &pb ) ){
			UNLOCK;
			if( !may_block )
				return ERR_MAY_BLOCK;
			sc->dma_sb |= DMA_SB_DREQ;
			dbdma_sbits_changed( sc->dma_irq );

			dma_wait( sc->dma_irq, DMA_READ, NULL );

			sc->dma_sb &= ~DMA_SB_DREQ;
			dbdma_sbits_changed( sc->dma_irq );
			LOCK;
		}
		/* XXX internal state could have changed here! */

		n = MIN( req_count, sc->fifo_count );
		xfer_from_fifo( buf, n );
		buf += n;
		req_count -= n;
		fifo_count += n;

		n = MIN( req_count, avail );
		n = MIN( n, pb.req_count-pb.res_count );

#ifdef VERBOSE
		printm("MESH: Using memcpy to copy %d bytes\n", n);
#endif
		memcpy( buf, pb.buf+pb.res_count, n );
		if( !dma_read_ack( sc->dma_irq, n, &pb ) )
			dma_count = n;

		/* DMA counter */
		sc->cur_xfer_count -= dma_count;
		sc->res_count += dma_count;
	} else {
		sc->res_count += fifo_count;
	}
#ifdef VERBOSE
	printm("MESH: scsi bus_data_req [%d = %d+%d]\n",dma_count + fifo_count,
	     dma_count, fifo_count );
#endif
		/* hold ACK at end of transfer until new command */
	if( !req_count && !sc->cur_xfer_count && !sc->fifo_count && sc->sequence != SEQ_DATAOUT ){
#ifdef VERBOSE
		printm("MESH: attempting to hold ACK at end of write transfer\n");
#endif
		sc->bus_status0 &= ~BS0_REQ;
		sc->bus_status0 |= BS0_ACK;
		scsi_xfer_status( sc->unit, 0 );
	}
/* interrupt assertion comes last */
	if( !sc->cur_xfer_count ){
		sc->interrupt |= INT_CMDDONE;
		end_cmd(7);
	}
	UNLOCK;
	return dma_count + fifo_count;
}


static void 
xfer_from_fifo( char *buf, int num_bytes )
{
	int i, end;

	if( num_bytes > sc->fifo_count )
		printm("MESH: fifo underflow ??\n");
	end = sc->fifo_bot + sc->fifo_count - 1;
	for(i=0; i<num_bytes; i++, buf++ ){
		end &= FIFO_MASK;
		*buf = sc->fifo[end--];
	}
	sc->fifo_count -= num_bytes;
	if( sc->fifo_count < 0 )
		sc->fifo_count = 0;
}


/************************************************************************/
/*	debugger cmds							*/
/************************************************************************/

/* command to read a mesh register */
static int __dcmd
cmd_meshr( int argc, char **args)
{
	ulong reg;
	ulong r;

	if (argc != 2)
		return 1;

	reg = string_to_ulong(args[1]);
	r = reg >> 4;
	reg = reg_io_read( get_mbase(sc->regs_range) + reg, 1, 0);
	printm("MESH register %s returns %02lx.  (State may be altered now)\n", reg_names[r], reg);
	return 0;
}
static int __dcmd
cmd_meshw( int argc, char **args)
{
	ulong reg;
	ulong r;

	if (argc != 3)
		return 1;

	reg = string_to_ulong(args[1]);
	r = string_to_ulong(args[2]);
	reg_io_write( get_mbase(sc->regs_range) + reg, r, 1, 0);
	reg = reg >> 4;
	printm("MESH wrote %02lx to register %s.  (State may be altered now)\n", r, reg_names[reg]);
	return 0;
}

static int __dcmd
cmd_meshstat( int argc, char **args)
{
	if (argc != 1)
		return 1;
	printm("----------- MESH STATE -------------\n");
	printm(" sc->deferred = %d\n sc->phase = %d\n", sc->deferred, sc->phase);
	printm(" sc->cmd_dma = %d\n sc->sphase_guarded = %d\n",sc->cmd_dma, sc->sphase_guarded);
	printm(" sc->sphase = %d\n sc->req_count = %d\n", sc->sphase, sc->req_count);
	printm(" sc->res_count = %d\n sc->done = %d\n", sc->res_count, sc->done);
	printm(" ------------------------\n");
	printm("sc->cur_xfer_count = %d\n",sc->cur_xfer_count);
	printm("sc->sequence = 0x%02x\n",sc->sequence);
	printm("sc->bus_status0 = 0x%02x\n", sc->bus_status0);
	printm("sc->bus_status1 = 0x%02x\n", sc->bus_status1);
	printm("sc->fifo_count = %d\n",sc->fifo_count);
	printm("sc->exception = 0x%02x\n", sc->exception);
	printm("sc->error = 0x%02x\n", sc->error);
	printm("sc->intr_mask = 0x%02x\n", sc->intr_mask);
	printm("sc->interrupt = 0x%02x\n", sc->interrupt);
	printm("sc->source_id = 0x%02x\n", sc->source_id);
	printm("sc->dest_id = 0x%02x\n", sc->dest_id);
	printm("sc->sel_timeout = 0x%02x\n", sc->sel_timeout);
	printm("sc->fifo_bot = %d\n", sc->fifo_bot);
	printm("sc->irq_line = 0x%02x\n", sc->irq_line);
	printm("sc->dma_sb = 0x%02x\n", sc->dma_sb);
	printm("sc->dma_sb_ext = 0x%02x\n", sc->dma_sb_ext);
	return 0;
}

static int __dcmd
cmd_relack( int argc, char **args)
{
	if (argc != 1)
		return 1;
	sc->deferred = 0;
	scsi_xfer_status(sc->unit, 1);
	sc->bus_status0 &= ~BS0_ACK;
	printm("MESH: manually released ACK\n");
	return 0;
}

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static io_ops_t ops = {
	.read	= reg_io_read,
	.write	= reg_io_write, 
	.print	= reg_io_print
};

static int 
mesh_init( void )
{
	mol_device_node_t *dn;	
	struct scsi_unit *unit;
	pthread_mutexattr_t mutex_attr;
	gc_bars_t bars;
	irq_info_t irqinfo;
	
	if( !(dn=prom_find_devices("mesh")) && !(dn=prom_find_devices("scsi")) )
		return 0;
	if( !is_gc_child(dn, &bars) )
		return 0;

	if( bars.nbars != 2 || prom_irq_lookup(dn, &irqinfo) != 2) {
		printm("MESH: Expecting 2 addrs and 2 intrs.\n");
		return 0;
	}

	/* debugger cmds */
	add_cmd( "meshr", "meshr x\nread mesh register at offset x\n", -1, cmd_meshr );
	add_cmd( "meshw", "meshw x y\nwrite mesh register x with y\n", -1, cmd_meshw );
	add_cmd( "meshstat", "meshstat\ndump the mesh state\n",-1,cmd_meshstat);
	add_cmd( "mesh_rel_ack", "mesh_rel_ack\nrelease ACK signal on mesh\n",-1,cmd_relack);

	/* register unit on bus */
	/* FIXME: we'll want multiple buses eventually */
	if( !(unit=scsi_register_unit(&unit_pb, 7, NULL)) ) {
		printm("Could not register MESH SCSI unit. Need a second bus?\n");
		return 0;
	}

	pthread_mutexattr_init( &mutex_attr );
	pthread_mutexattr_setkind_np( &mutex_attr, PTHREAD_MUTEX_RECURSIVE_NP );
	pthread_mutex_init( &sc->lock_mutex, &mutex_attr );
	pthread_mutexattr_destroy( &mutex_attr );

	sc->unit = unit;
	sc->phase = idling;

	sc->irq = irqinfo.irq[0];
		/* dbdma and scsi must share same interrupt */
	// sc->dma_irq == irqinfo.irq[1]
	sc->dma_irq = sc->irq;

	sc->regs_range = add_gc_range( bars.offs[0], 0x100, "mesh-regs", 0, &ops, NULL );
	allocate_dbdma( bars.offs[1], "mesh-dma", sc->dma_irq, dma_status_bits, 0 );
	
	reset_chip();

	printm("MESH SCSI-driver installed (IRQs %d/%d)\n",sc->irq, sc->dma_irq );

/* DEBUG */
	dbdma_sbits_changed(sc->dma_irq);
	return 1;
}

static void 
mesh_cleanup( void )
{
	pthread_mutex_destroy( &sc->lock_mutex );
}

driver_interface_t mesh_driver = {
	"mesh", mesh_init, mesh_cleanup
};
