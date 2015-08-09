/* 
 *   Creation Date: <1997-07-27 05:32:54 samuel>
 *   Time-stamp: <2003/06/02 13:44:33 samuel>
 *   
 *	<53c94.c>
 *	
 *	Driver for the 53c94 SCSI controller
 *	(external SCSI chain on machines with two controllers)
 *   
 *   Copyright (C) 1998, 1999, 2000, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

/* #define VERBOSE */

#include <sys/param.h>
#include <pthread.h>

#include "molcpu.h"
#include "ioports.h"
#include "promif.h"
#include "dbdma.h"
#include "pic.h"
#include "debugger.h"
#include "verbose.h"
#include "gc.h"
#include "thread.h"
#include "driver_mgr.h"

#include "scsi-bus.h"
#include "53c94_hw.h"
#include "scsi_main.h"

#ifdef VERBOSE
SET_VERBOSE_NAME("53c94");
#endif

/*#define FIFO_VERBOSE*/

/* This implementation is based upon the specifications 
 * for the Amd53c94.
 */

/* S8..S0 bits in DMA_STATUS */
#define DMA_SB_CMD_DONE		0x80	/* ??? From MESH  */
#define DMA_SB_EXCEPTION	0x40	/* ??? From MESH  */
#define DMA_SB_DREQ		0x20
#define DMA_SB_EXTERNAL_MASK	0x1f	/* or only bit s0 ? */

static char *reg_rnames[NUM_REGS] = {
	"COUNT_LO", "COUNT_MID", "FIFO", "COMMAND", "STATUS", "INTERRUPT",
	"ISTATE", "FLAGS", "CONFIG1", "***", "***", "CONFIG2",
	"CONFIG3", "(CONFIG4)", "(COUNT_HI)", "FIFO_RES"
};
static char *reg_wnames[NUM_REGS] = {
	"COUNT_LO", "COUNT_MID", "FIFO", "COMMAND", "DEST_ID", "TIMEOUT",
	"SYNC_PERIOD", "SYNC_OFFS", "CONFIG1", "CLK_FACTOR", "TEST", "CONFIG2",
	"CONFIG3", "(CONFIG4)", "(COUNT_HI)", "FIFO_RES"
};
static char *cmd_names[128] = {
	"NOP", "FLUSH", "RESET", "SCSI_RESET", "ABORT_DMA",0,0,0,0,0,0,0,0,0,0,0,	
	"XFER_DATA", "I_COMPLETE", "ACCEPT_MSG", 0,0,0,0,0,"XFER_PAD",
		0,"SET_ATN", "CLR_ATN",0,0,0,0,
	"SEND_MSG", "SEND_STATUS", "SEND_DATA", "DISC_SEQ", "TERMINATE", 
		"T_COMPLETE", 0, "T_DISCONNECT", "RECV_MESG", "RECV_CDB", "RECV_DATA",
		"RECV_CMD",0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	"RESELECT","SELECT","SELECT_ATN","SELATN_STOP","ENABLE_SEL","DISABLE_SEL",
		"SEL_ATN3", "RESEL_ATN3",0,0,0,0,0,0,0,0
};


#define WARN_MISALIGN(len) \
	if(len!=1) { printm("53c94: Misaligned access ignored\n"); }

#define FIFO_SIZE	16
#define FIFO_MASK   	(FIFO_SIZE-1)


#ifdef VERBOSE
#define VER(str) printm(str);
#define VER1(str,p1) printm(str,p1);
#define VER2(str,p1,p2) printm(str,p1,p2);
#define VER3(str,p1,p2,p3) printm(str,p1,p2,p3);
#else
#define VER(str) ;
#define VER1(str,p1) ;
#define VER2(str,p1,p2) ;
#define VER3(str,p1,p2,p3) ;
#endif

/* mode */
enum {
	idle_mode=0, initiator_mode, target_mode
};

/* phase */
enum {
	idling=0, starting, completing, reading, writing,
	req_waiting, stepping
};

typedef int micro_cmd( int param, ulong err );

typedef struct micro_entry {
	micro_cmd	*cmd;
	ulong		param;
	ulong		err;
} micro_entry_t;

/* information about the command beeing processed */
typedef struct cmd_state {
	int 	interrupt;
	int	dma;
	char	cmd;

	int	req_count;	/* current transfer */
	int	res_count;

	micro_entry_t	*mc_ptr;
} cmd_state_t;

static struct {
	int		irq, dma_irq;

	gc_range_t	*regs_range;

	pthread_mutex_t	lock_mutex;

	int		mode;
	int		is_reset;
	int		phase;

	int		sphase_guarded;		/* true if sphase is monitored */
	int		guard_err;		/* error if sphase changes */
	int		request_err;		/* error on unexected req */

	int		cur_sphase;		/* curent sphase */
	int		new_sphase;		/* new sphase (valid after REQ) */

	struct scsi_unit *unit;

	cmd_state_t	cmd;

	/* registers */
	int		cur_xfer_count;		/* read only, */
	int		start_xfer_count;  	/* write only 0 == 2^16*/

	unsigned char	cmd_queue[2];		/* command que */
	int		num_cmds;

	unsigned char	fifo[FIFO_SIZE];	/* [CHIP] fifo_bot------xxx  [SCSI]  */
	int		fifo_bot;
	int		num_fifo;

       	unsigned char	status;			/* status register */
	unsigned char	latched_status;
	int		latched_status_valid;

	unsigned char	inter_status;		/* interrupt status */
	unsigned char	istate;			/* internal state status */

	unsigned char	dest_id;		/* write only */

	unsigned char 	config1;		/* r/w (control reg 1) */
	unsigned char 	config2;		/* r/w (control reg 2) */
	unsigned char 	config3;		/* r/w (control reg 3) */

	/* dma status byte (S8..S0) */
	unsigned char	dma_sb;
	unsigned char	dma_sb_ext;		/* external controlled bits */
} sc[1];


#define LOCK	pthread_mutex_lock( &sc->lock_mutex );
#define UNLOCK	pthread_mutex_unlock( &sc->lock_mutex );

/* COMMAND DEFINITIONS */

static micro_cmd mc_select;
static micro_cmd mc_expect_sphase;
static micro_cmd mc_leave_sphase;
static micro_cmd mc_set_atn;
static micro_cmd mc_mes_out;
static micro_cmd mc_input;
static micro_cmd mc_done;
static micro_cmd mc_keep_ack;
static micro_cmd mc_xfer_data;
static micro_cmd mc_unguard_sphase;
static micro_cmd mc_xfer_completion;
static micro_cmd mc_wait_req;

#define STATUS(istate,inter_status,mode) ( ((istate)<<16)+ ((inter_status)<<8) + (mode))
#define ISTATE(err)			 ((err)>>16)
#define INTER_STATUS(err)		 (((err)>>8) & 0xff)
#define MODE(err)			 ((err) & 0xff)


/* CMD_SET_ATN - not really tested */
static micro_entry_t cmd_set_atn[] = {
	{mc_set_atn,		1,		0 },
	{mc_done,		0,		STATUS(4,0x18,0) },
	{NULL,0,0}
};


/* CMD_SELECT */
static micro_entry_t cmd_sel_without_atn[] = {
	{mc_select,		0 /*no atn */, 	STATUS(0,0x20,0) },

	{mc_expect_sphase,   	sphase_command, STATUS(2,0x18,0) },

	{mc_xfer_data,		0,		STATUS(3,0x18,0) },
	{mc_unguard_sphase,	0,		0 },
	{mc_leave_sphase,	sphase_command, STATUS(4,0x18,0) },

	{mc_done,		0,		STATUS(4,0x18,0) },
	{NULL,0,0}
};


/* CMD_SEL_ATN */
static micro_entry_t cmd_sel_with_atn[] = {
	{mc_select,		1 /*atn */, 	STATUS(0,0x20,0) },

	{mc_expect_sphase,   	sphase_mes_out, STATUS(0,0x18,0) },
	{mc_mes_out,	 	1, 		STATUS(0,0x18,0) },
	{mc_set_atn,		0,		0 },
	{mc_leave_sphase,	sphase_mes_out,	STATUS(2,0x18,0) },

	{mc_expect_sphase,   	sphase_command, STATUS(2,0x18,0) },
	{mc_xfer_data,		0,		STATUS(3,0x18,0) },
	{mc_unguard_sphase,	0,		0 },
	{mc_leave_sphase,	sphase_command, STATUS(4,0x18,0) },

	{mc_done,		0,		STATUS(4,0x18,0) },
	{NULL,0,0}
};
/* CMD_SEL_ATN3 */
static micro_entry_t cmd_sel_with_atn3[] = {
	{mc_select,		1 /*atn */, 	STATUS(0,0x20,0) },

	{mc_expect_sphase,   	sphase_mes_out, STATUS(0,0x18,0) },
	{mc_mes_out, 		3,		STATUS(2,0x18,0) },
	{mc_set_atn,		0,		0 },
	{mc_leave_sphase,	sphase_mes_out,	STATUS(2,0x18,0) },

	{mc_expect_sphase,   	sphase_command, STATUS(2,0x18,0) },
	{mc_xfer_data,		0,		STATUS(3,0x18,0) },
	{mc_unguard_sphase,	0,		0 },
	{mc_leave_sphase,	sphase_command, STATUS(4,0x18,0) },

	{mc_done,		0,		STATUS(4,0x18,0) },
	{NULL,0,0}
};
/* CMD_SEL_ATN_STOP */
static micro_entry_t cmd_sel_with_atn_stop[] = {
	{mc_select,		1 /*atn */, 	STATUS(0,0x20,0) },

	{mc_expect_sphase,   	sphase_mes_out, STATUS(0,0x18,0) },
	{mc_mes_out, 		1, 		STATUS(0,0x18,0) },

	{mc_done,		0,		STATUS(4,0x18,0) },
	{NULL,0,0}
};
/* CMD_I_COMPLETE */
static micro_entry_t cmd_i_complete[] = {
	/* We should perhaps not require that we are in the status phase.
	 * Also the STATUS numbers are probably wrong 
	 */
	{mc_expect_sphase,	sphase_status,  	STATUS(0,0x20,0) },
	{mc_input,		1, 			STATUS(0,0x18,0) },
	{mc_leave_sphase,	sphase_status,		STATUS(2,0x18,0) },

	{mc_expect_sphase,	sphase_mes_in,		STATUS(2,0x18,0) },
	{mc_input,		1,			STATUS(2,0x18,0) },
	{mc_keep_ack,		1,			0 },
	{mc_done,		0,			STATUS(4,0x08,0) },
	{NULL,0,0}
};
/* CMD_ACCEPT_MSG */
static micro_entry_t cmd_accept_msg[] = {
	/* The mc_leave_sphase should not be necessary - mc_keep_ack
	 * will cause the target to change scsi-phase etc. 
	 */
	{mc_keep_ack,		0,			0 },
	{mc_wait_req,		0,			0 },
	{NULL,0,0}
};
/* CMD_XFER_DATA */
static micro_entry_t cmd_xfer_data[] = {
	{mc_xfer_data,		0,			STATUS(0,0x10,0) },
	{mc_unguard_sphase,	0,			0 },
	{mc_xfer_completion,	0,			0 },
	{mc_wait_req,		0,			0 },
	{NULL,0,0}
};


/************************************************************************/
/*	implementation							*/
/************************************************************************/

int 
s53c94_available( void )
{
	return prom_find_devices( "53c94" ) != NULL;
}

static void
do_interrupt( void )
{
	/* How should we handle command queuing? The specifications 
	 * for Amd53c94 is not entierly clear about what happens 
	 * if both commands in the command queue raise exceptions.
	 */

	sc->status |= STAT_IRQ;

#if 1
	if( sc->latched_status_valid )
		printm("53c94: WARNING: Multiple interrupt conditions\n");
#endif
	if( !sc->latched_status_valid ){
		sc->latched_status = sc->status;
		sc->latched_status_valid = 1;

		sc->status &= STAT_TC_ZERO | 0x7;
	}

	VER("*** 53c94 IRQ asserted ***\n");

	/* raise irq line... */
	irq_line_hi( sc->irq );
}

static void
reset_chip( void )
{
	VER("*** 53c94 CHIP RESET ***\n");
	
	/* XXX: This might not be complete */
	sc->cur_xfer_count = 0;
	sc->start_xfer_count = 0;
	sc->num_cmds = 0;
	sc->cmd_queue[0] = 0;
	sc->fifo_bot=0;
	sc->num_fifo = 0;
	sc->status = 0;
	sc->latched_status_valid = 0;
	sc->inter_status =0;		/* interrupt status */
	sc->istate = 0;
	sc->dest_id=0;		/* write only */
	sc->config1 = 0;
	sc->config2 = 0;
	sc->config3 = 0;
}

/* Implementation of the S0..S7 bits in the dbdma STATUS register.
 * (and they *ARE* used)
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
do_illegal_cmd( void )
{
	sc->num_cmds = 0;
	sc->status |= STAT_ERROR;
	do_interrupt();
	sc->phase = idling;
}

static void 
start_cmd( void )
{
	int illegal;
	int cmd = sc->cmd_queue[0];
	int dma = cmd & CMD_DMA_MODE;

	VER("start_cmd -----------------\n");
	sc->sphase_guarded = 0;
	sc->request_err = 0;

	cmd &= ~CMD_DMA_MODE;
	sc->cmd.interrupt = 1;
	sc->cmd.cmd = cmd;
	sc->cmd.dma = dma;
	sc->cmd.req_count = 0;
	sc->cmd.res_count = 0;
	sc->phase = completing;

	VER2("Starting COMMAND: %s %s\n", cmd_names[ cmd ], dma ? "[DMA]" : "" );

	/* illegal opcode? */
	illegal = (cmd & CMD_MODE_INIT) && sc->mode != initiator_mode;
	illegal |= (cmd & CMD_MODE_TARG) && sc->mode != target_mode;
	illegal |= (cmd & CMD_MODE_DISC) && sc->mode != idle_mode;
	if( illegal ) {
		do_illegal_cmd();
		return;
	}

	if( dma ) {
		sc->cur_xfer_count = sc->start_xfer_count;
		if( !sc->cur_xfer_count )
			sc->cur_xfer_count = 0x10000;
		sc->status &= ~STAT_TC_ZERO;
	}

	switch( cmd ){
	/* General commands */
	case CMD_NOP:
		sc->is_reset = 0;
		sc->cmd.interrupt = 0;
		break;
	case CMD_FLUSH:
		sc->fifo_bot = 0;
		sc->num_fifo = 0;
		sc->cmd.interrupt = 0;
		break;
	case CMD_RESET:
		printm("53c94: Internal error, CMD_RESET should be handled elsewhere\n");
		break;
	case CMD_SCSI_RESET:
		VPRINT("** SCSI_RESET **\n");
		/* XXX reset SCSI bus */
		sc->cmd.interrupt = 0;
		if( !(sc->config1 & CF1_NO_RES_REP) ) {
			sc->cmd.interrupt = 1;
			sc->inter_status |= INTR_RESET;
		}
		break;
		
	/* Idle mode commands */

	case CMD_SELECT_ATN: 	/* 0x42 */
		sc->cmd.mc_ptr = cmd_sel_with_atn;
		sc->phase = stepping;
		break;
	case CMD_SEL_ATN3:	/* 0x46 */
		printm("SEL_ATN3\n");
		sc->cmd.mc_ptr = cmd_sel_with_atn3;
		sc->phase = stepping;
		break;
	case CMD_SELECT:  	/* 0x41 */
		sc->cmd.mc_ptr = cmd_sel_without_atn;
		sc->phase = stepping;
		break;
	case CMD_SELATN_STOP:	/* 0x43 */
		printm("SEL_ATN_STOP\n");
		sc->cmd.mc_ptr = cmd_sel_with_atn_stop;
		sc->phase = stepping;
		break;

	case CMD_ENABLE_SEL:	/* 0x44 */
		/* XXX: reselection not implemented */
		sc->cmd.interrupt = 0;
		sc->phase = completing;
		break;
	case CMD_DISABLE_SEL:	/* 0x45 */
		/* XXX: reselection not implemented */
		sc->cmd.interrupt = 0;
		sc->phase = completing;
		break;
#if 0
	case CMD_RESELECT:	/* 0x40 */
	case CMD_RESEL_ATN3:	/* 0x47 */
		break;
#endif
	/* Initiator mode commands */
	case CMD_I_COMPLETE:	/* 0x11 */
		sc->cmd.mc_ptr = cmd_i_complete;
		sc->phase = stepping;
		break;
	case CMD_ACCEPT_MSG:	/* 0x12 */
#ifdef FIFO_VERBOSE
		printm("FIFO count: %d\n", sc->num_fifo );
#endif
		sc->cmd.mc_ptr = cmd_accept_msg;
		sc->phase = stepping;
		break;
	case CMD_XFER_DATA:	/* 0x10 */
#if 0
		if( !sc->cmd.dma ) {
			printm("Hardcoded breakpoint in " __FILE__ "\n");
			stop_emulation();
		}
#endif
		sc->cmd.mc_ptr = cmd_xfer_data;
		sc->phase = stepping;
		break;
	case CMD_SET_ATN: /* Not tested properly... */
		sc->cmd.interrupt = 0;
		sc->cmd.mc_ptr = cmd_set_atn;
		sc->phase = stepping;
		break;
	default:
		sc->cmd.interrupt = 1;
		sc->phase = completing;
		printm("53c94 WARNING: Unimplemented command %02X\n", cmd );
		break;
	}
}

static void
end_cmd( void )
{
	if( sc->cmd.interrupt )
		do_interrupt();

	/* ...if there was an interrupt, should we delay the start 
	 * of the next command until the interrupt has ben unlatched?
	 */
	if( sc->num_cmds )
		sc->num_cmds--;
	if( sc->num_cmds )
		sc->cmd_queue[0] = sc->cmd_queue[1];
	
	sc->phase = sc->num_cmds ? starting : idling;
}

/* Fetch and execute next mc-command. */
static int 
do_stepping( void )
{
	VER("do_stepping\n");

	for( ;; ) {
		int err;
		micro_entry_t *me = sc->cmd.mc_ptr;

		if( !me || !me->cmd ) {
			sc->phase = completing;
			return 1;
		}

		sc->cmd.mc_ptr++;
		err = me->cmd( me->param, me->err );
		if( err ){
			if( err == -1 ) {
				sc->cmd.mc_ptr = NULL;
				sc->istate = ISTATE( me->err );
				sc->inter_status = INTER_STATUS(me->err);
				sc->cmd.interrupt = 1;
				sc->phase = completing;
				return 1;
			}
			return 0;
		}
		if( sc->phase != stepping )
			break;
	}
	return 1;
}

static void
engine( void )
{
	int run = 1;
	
	while( run ){
		switch( sc->phase ){
		case idling:
		case req_waiting:
			run = 0;
			break;
		case starting:
			start_cmd();
			break;
		case completing:
			/* Note: It must be permitted to go to
			 * completing directly from idling (in the case
			 * of an unexpected phase change or REQ pulse)
			 */
			end_cmd();
			break;
		case reading:
			VER("data_available (reading)\n");
			run = 0;
			break;
		case writing:
			VER("data_available (writing)\n");
			run = 0;
			break;
		case stepping:
			run = do_stepping();
			break;
		default:
			printm("Unimplemented state %d\n",sc->phase);
			break;
		}
	}
}

static void
do_command( unsigned char cmd )
{
	int dma = cmd & CMD_DMA_MODE;
	cmd &= ~CMD_DMA_MODE;

	/* CMD_NOP enables the cmd register after a reset */
	if( sc->is_reset && cmd != CMD_NOP ) {
#ifdef VERBOSE
		printm("*53c94: Ignoring command in reset mode\n");
#endif
		return;
	}

	/* CMD_RESET and CMD_ABRT_DMA are the only commands
	 * which run from cmd_queue[1], i.e. bypasses the queue.
	 */
	if( cmd == CMD_RESET ){
		reset_chip();
		sc->is_reset = 1;
		return;
	}
	if( cmd == CMD_ABORT_DMA ){
		/* XXX */
		printm("53c94: CMD_ABORT_DMA not implemented!\n");
		return;
	}

	if( sc->num_cmds != 2 )
		sc->cmd_queue[sc->num_cmds++] = cmd | dma;
	else {
		/* COMMAND OWERFLOW! */
		printm("53c94: Command overwritten\n");
		sc->status |= STAT_ERROR;
		sc->cmd_queue[1] = cmd | dma;
	}

	if( sc->num_cmds == 1 ) {
		sc->phase = starting;
		engine();
	}
}

static int
mc_select( int atn, ulong err )
{
	VER1("mc_select, TARGET_ID = %d\n", sc->dest_id );

	if( scsi_arbitration( sc->unit ) )
		printm("53c94: Uimplemented, Arbitration lost!\n");

	sc->mode = initiator_mode;
	UNLOCK;
	scsi_selection( sc->unit, sc->dest_id, atn );
	LOCK;
	return 1;
}

static int
mc_done( int dummy, ulong err )
{
	VER("mc_done\n");
	return -1;
}


static int
mc_expect_sphase( int sphase, ulong err )
{
	VER1("mc_expect_sphase %d\n", sphase);

	if( (sc->status & 0x7) != sphase ) {
		printm("Unexpected scsi-phase. (was %d)\n", sc->status & 0x7 );
		return -1;
	}
	return 0;
}

static int
mc_leave_sphase( int sphase, ulong err )
{
	sc->request_err = err;

	if( (sc->status & 0x7) == sphase ) {
		sc->cmd.mc_ptr--;
		return 1;
	}
	return 0;
}

static int
mc_set_atn( int atn, ulong dummy )
{
	VER1("mc_atn %d\n", atn);

	scsi_set_atn( atn );
	return 0;
}

static int
mc_mes_out( int len, ulong err )
{
	VER1("mc_mes_out %d\n",len );

	sc->cmd.req_count = len;
	sc->cmd.res_count = 0;
	sc->phase = writing;
	scsi_xfer_status( sc->unit, 1 /* data ready */ );
	return 0;
}

static int
mc_input( int len, ulong err )
{
	VER1("mc_input %d\n",len );

	sc->cmd.req_count = len;
	sc->cmd.res_count = 0;
	sc->phase = reading;
	scsi_xfer_status( sc->unit, 1 /* data ready */ );
	return 0;
}


static int
mc_keep_ack( int keep, ulong err )
{
	VER("mc_keep_ack\n");
	scsi_xfer_status( sc->unit, !keep );
	return 0;
}

static int
mc_xfer_data( int dummy, ulong err )
{
	int incoming;
	VER("mc_xfer_data\n");

	incoming = sc->status & STAT_IO;
	
	sc->cmd.res_count = 0;
	if( sc->cmd.dma ){
		VER1("DMA, count = %d\n", sc->cur_xfer_count );
		sc->cmd.req_count = sc->cur_xfer_count;
	} else {
		/* If data flow is ingoing then we can use FIFO
		 * to determine who many bytes to transfer,
		 * otherwise we must wait for target to change
		 * phase (cur_xfer_count is not used if dma is not 
		 * set).
		 */
		if( !incoming )
			sc->cmd.req_count = sc->num_fifo;
		else {
			/* XXX IS THIS THE CORRECT THING TO DO???? 
			 * If we let more bytes into the FIFO, things seems
			 * not to work.
			 */
			/* printm("** Setting req_count to 1 **\n"); */
			sc->cmd.req_count = 1;	/* OR.... ? */
		}
	}
	sc->phase = incoming? reading : writing;

	VER("GUARDING SPHASE\n");
	sc->sphase_guarded = 1;
	sc->guard_err = err;

	VER1("xfer_data, req_count %d\n",sc->cmd.req_count );

	/* scsi_xfer_status must be the last thing, since
	 * it may result in a indirect call of engine()
	 */
	scsi_xfer_status( sc->unit, 1 /* data available */ );
	return 0;
}

static int
mc_unguard_sphase( int dummy, ulong err )
{
	VER("unguard sphase\n");
	sc->sphase_guarded = 0;
	return 0;
}

static int
mc_xfer_completion( int dummy, ulong err )
{
	VER("mc_xfer_completion\n");

	if( sc->cur_sphase == sphase_mes_out ){
		/* deassert ATN after last byte of message out */
		scsi_set_atn( 0 );
	} else if( sc->cur_sphase == sphase_mes_in ){
		scsi_xfer_status( sc->unit, 0 /* keep ack */ );
		sc->inter_status = INTR_DONE;
		sc->cmd.interrupt = 1;
		sc->phase = completing;
		return 1;
	}
	return 0;
}

static int
mc_wait_req( int dummy, ulong err )
{
	sc->cmd.interrupt = 0;
	sc->phase = req_waiting;
	return 1;
}

/* 
 * SCSI Bus interface
 * 
 */

static void 
sphase_changed( int old_sphase, int new_sphase, void *usr )
{
	LOCK;
	VER2("53c94: phase change: %d -> %d\n", old_sphase, new_sphase );

	sc->status &= ~0x7;
	if( new_sphase < 8 )
		sc->status |= new_sphase;
	
	/* XXX bus_free is not handled perfectly  */
	if( new_sphase == sphase_bus_free ){
		VER("*** DISCONNECT ***\n");

		sc->mode = idle_mode;
		/* no target... */
		sc->cmd.mc_ptr = NULL;
		sc->inter_status = INTR_DISCONNECT;
		sc->istate = 0;
		sc->phase = completing;
		sc->cmd.interrupt = 1;
		engine();
	} 
	UNLOCK;
}

static void
do_new_sphase( int new_sphase )
{
	sc->cur_sphase = new_sphase;
	sc->status &= ~STAT_PHASE;
	sc->status |= new_sphase;
	
	if( sc->sphase_guarded ){
		VER("******** SPHASE GUARD ********\n");
		sc->cmd.mc_ptr = NULL;
		sc->inter_status = INTER_STATUS( sc->guard_err );
		sc->istate = ISTATE( sc->guard_err );
		sc->phase = completing;
		sc->cmd.interrupt = 1;
		sc->sphase_guarded = 0;
	} else if( sc->phase == idling ){
		sc->inter_status = INTR_BUS_SERV;
		do_interrupt();
	} else if( sc->phase == req_waiting ){
		sc->cmd.interrupt = 1;
		sc->inter_status = INTR_BUS_SERV;
		sc->phase = completing;
	}
	engine();
}

/* SCSI-device asserted REQ, but we were not prepared
 * for any transfer.
 */
static void
req_pulse( void )
{
	VER("REQ_PULSE!\n");

	/* If no special error is set, generate a service
	 * interrupt.
	 */
	sc->inter_status = INTR_BUS_SERV;
	if( sc->request_err ){
		sc->istate = ISTATE( sc->request_err );
		sc->inter_status = INTER_STATUS( sc->request_err );
	}
	sc->phase = completing;
	sc->cmd.interrupt = 1;	
	sc->cmd.mc_ptr = NULL;
	engine();
}

static int
bus_data_in( int count, char *buf, int may_block, int sphase, void *usr )
{
	struct dma_write_pb pb;
	int avail, phase_changed=0;
/*	char *orgbuf=buf; */
	
	LOCK;
	VER1("bus_data_in: %d\n",count );

	/* We detect phase changes here */
	if( sc->cur_sphase != sphase  ){
		scsi_xfer_status( sc->unit, 0 /* halt */ );
		do_new_sphase( sphase );
		phase_changed = 1;
	}

	if( sc->phase != reading )
		engine();
	if( sc->phase != reading ) {
		scsi_xfer_status( sc->unit, 0 /*no data*/ );
		if( !phase_changed )
			req_pulse();
		VER("bus_data_in - NO_DATA\n");
		UNLOCK;
		return ERR_NO_DATA;
	}

	avail = sc->cmd.req_count - sc->cmd.res_count;
	if( count > avail )
		count = avail;

	if( sc->cmd.dma ) {
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


		if( sc->num_fifo )
			printm("***** 53c94 WARNING: Possible implementation error\n");
#if 0
		/* Empty fifo first */
		if( sc->num_fifo ){
			char fifo_buf[FIFO_SIZE];
			int i;
			if( count > sc->num_fifo )
				count = sc->num_fifo;
			printm("Sending residue byte in FIFO\n");
			for( i=0; i<count; i++ ) {
				fifo_buf[i] = sc->fifo[sc->fifo_bot++];
				sc->fifo_bot &= FIFO_MASK;
			}
			sc->num_fifo -= count;
			dma_write_ack( sc->dma_irq, fifo_buf, count, &pb );
		}
		else
#endif
		/* XXX internal state could have changed here! */
		if( dma_write_ack( sc->dma_irq, buf, count, &pb ) )
			count = 0;

		/* DMA counter */
		sc->cur_xfer_count -= count;
		if( count && !sc->cur_xfer_count )
			sc->status |= STAT_TC_ZERO;
	} else {
		int i, fifo_top=0;
		/* What happens if FIFO can't hold all DATA? Is input suspended
		 * bytes have been read from FIFO - or is data lost?
		 */
		for(i=0; i<count; i++, buf++ ){
			fifo_top = (sc->fifo_bot + sc->num_fifo) & FIFO_MASK;
			sc->fifo[fifo_top] = *buf;
			sc->num_fifo++;
		}
#if 1
		if( sc->num_fifo > FIFO_SIZE ) {
			printm("53c94: FIFO overflow\n");
			printm("Hardware breakpoint in " __FILE__ "\n");
			stop_emulation();
			sc->status |= STAT_ERROR;  /* or...? */
			sc->num_fifo = FIFO_SIZE;
			sc->fifo_bot = (fifo_top+1) & FIFO_MASK;
		}
#endif
	}
	sc->cmd.res_count += count;

#if 0
	{
		int i;
		for(i=0; i<count; i++ ) {
			printm("%02X ", orgbuf[i] );
			if( i%20 == 19 )
				printm("\n");
		}
		printm("\n");
	}
#endif

	if( sc->phase == reading && sc->cmd.res_count == sc->cmd.req_count ){
		sc->phase = stepping;
		engine();
#if 0
		/* XXX */
		if( sc->cmd.req_count == 10752 )
			HARD_BREAKPOINT;
#endif
	}

/*	printm(" ** %d **\n", count ); */
	VER3("bus_data_in [%d] (req %d, res %d)\n",count, 
	       sc->cmd.req_count, sc->cmd.res_count );
	UNLOCK;
	return count;
}

static void 
xfer_from_fifo( char *buf, int num_bytes )
{
	int i, end;

	end = sc->fifo_bot + sc->num_fifo - 1;
	for(i=0; i<num_bytes; i++, buf++ ){
		end &= FIFO_MASK;
		*buf = sc->fifo[end--];
	}
	sc->num_fifo -= num_bytes;
	if( sc->num_fifo < 0 )
		sc->num_fifo = 0;
}

static int
bus_data_req( int req_count, char *buf, int may_block, int sphase, void *usr )
{
	struct dma_read_pb pb;
	int avail, phase_changed=0;
	int dma_count, fifo_count;
	
	LOCK;
	VER2("[fifo %d] bus_data_req: %d\n",sc->num_fifo, req_count );

	/* We detect phase changes here */
	if( sc->cur_sphase != sphase  ){
		scsi_xfer_status( sc->unit, 0 /* halt */ );
		do_new_sphase( sphase );
		phase_changed = 1;
	}

	if( sc->phase != writing )
		engine();
	if( sc->phase != writing ) {
		scsi_xfer_status( sc->unit, 0 /*no data*/ );
		if( !phase_changed )
			req_pulse();
		UNLOCK;
		return ERR_NO_DATA;
	}

	avail = sc->cmd.req_count - sc->cmd.res_count;
	
	/* From FIFO. */
	fifo_count = MIN( req_count, sc->cmd.dma ? sc->num_fifo : avail );

	xfer_from_fifo( buf, fifo_count );
	buf += fifo_count;
	req_count -= fifo_count;
	
	/* From DMA */
	dma_count = 0;
	if( sc->cmd.dma ) {
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

		n = MIN( req_count, sc->num_fifo );
		xfer_from_fifo( buf, n );
		buf += n;
		req_count -= n;
		fifo_count += n;

		n = MIN( req_count, avail );
		n = MIN( n, pb.req_count-pb.res_count );

		memcpy( buf, pb.buf+pb.res_count, n );
		if( !dma_read_ack( sc->dma_irq, n, &pb ) )
			dma_count = n;

		/* DMA counter */
		sc->cur_xfer_count -= dma_count;
		if( dma_count && !sc->cur_xfer_count )
			sc->status |= STAT_TC_ZERO;
		sc->cmd.res_count += dma_count;
	} else {
		sc->cmd.res_count += fifo_count;
	}

	if( sc->phase == writing && sc->cmd.req_count == sc->cmd.res_count ){
		sc->phase = stepping;
		engine();
	}
	UNLOCK;
	VER3("bus_data_req [%d = %d+%d]\n",dma_count + fifo_count,
	     dma_count, fifo_count );
	return dma_count + fifo_count;
}


/************************************************************************/
/*	I / O								*/
/************************************************************************/

static ulong
reg_io_read( ulong addr, int len, void *usr )
{
	int 	r = (addr - get_mbase(sc->regs_range) )>>4;
	ulong 	ret=0;

	LOCK;
	
	WARN_MISALIGN(len);
	switch(r){
	case r_count_lo: 	/* 0 */
		ret = sc->cur_xfer_count & 0xff;
		break;
	case r_count_mid: 	/* 1 */
		ret = (sc->cur_xfer_count >> 8) & 0xff;
		break;
	case r_fifo: 		/* 2 */
#if DEBUG_SCSI
		if( !sc->num_fifo )
			printm("Reading empty fifo\n");
#endif	       
		ret = 0;
		if( sc->num_fifo ) {
			ret = sc->fifo[sc->fifo_bot];
			sc->fifo_bot = (sc->fifo_bot + 1) & FIFO_MASK;
			sc->num_fifo--;
		}
		break;
	case r_command:		/* 3 */
		ret = sc->cmd_queue[0];
		break;

	case r_status:		/* 4 */
		ret = sc->status;
		if( sc->latched_status_valid ){
			ret = sc->latched_status;
			if( !(sc->config2 & CF2_FEATURE_EN) )
				ret = (ret & ~0x7) | (sc->status & 0x7);
		}
		break;
	case r_interrupt:	/* 5 */
		if( sc->latched_status_valid )
			sc->latched_status_valid = 0;
		else
			sc->status &= STAT_TC_ZERO | 0x7;
		ret = sc->inter_status;
		sc->inter_status = 0;
		sc->istate = 0;
		irq_line_low( sc->irq );
		break;

	case r_seqstep:		/* 6 */
		ret = sc->istate;
		break;

	case r_flags: 		/* 7 */
		ret = (sc->num_fifo & 0x1f) | ((sc->istate << 5) & 0xe0);
#if 0
		stop_emulation();
#endif
		break;
	case r_config1:		/* 8 */
		ret = sc->config1;
		break;
	case r_config2:		/* 0xB */
		ret = sc->config2;
		break;
	case r_config3:		/* 0xC */
		ret = sc->config3;
		break;
	case r_count_hi:	/* not implemented */
		ret = 0;
		break;
	case 0x9:
	case 0xA:
	case 0xF:
		printm("53c94: Read from Write-Only register\n");
		break;
	case 0xD:
		printm("53c94: Read from register supposed to be reserved\n");
		break;
	}
	UNLOCK;

	return ret;
}

static void 
reg_io_write( ulong addr, ulong data, int len, void *usr )
{
	int r = (addr - get_mbase(sc->regs_range) )>>4;
	WARN_MISALIGN(len);
	data &= 0xff;

	LOCK;	
	switch(r) {
	case r_count_lo: 	/* 0 */
		sc->start_xfer_count = (sc->start_xfer_count & ~0xff) | data;
		break;
	case r_count_mid: 	/* 1 */
		sc->start_xfer_count = (sc->start_xfer_count & ~0xff00) | (data<<8);
		break;

	case r_fifo:	    	/* 2 */
		if( sc->num_fifo == FIFO_SIZE ) {
			sc->status |= STAT_ERROR;
			sc->num_fifo--;
		}
		sc->fifo_bot = (sc->fifo_bot-1) & FIFO_MASK;
		sc->num_fifo++;
		sc->fifo[sc->fifo_bot] = data;
		break;

	case r_command:		/* 3 */
		do_command( data );
		break;

	case r_dest_id: 	/* 4 */
		sc->dest_id = data & 0x7;
		break;

	case r_scsi_timeout:	/* 5 */
	case r_sync_period: 	/* 6 */
	case r_sync_offset: 	/* 7 */
	case r_clk_factor:	/* 9 */
	case r_test:		/* 0xA */
		break;
	case r_config1:		/* 8 */
		sc->config1 = data;
		break;
	case r_config2:		/* 0xB */
		if( data & CF2_RFB )
			VPRINT("53c94: Data alignment enable unimplemented!\n");
		sc->config2 = data;
		break;
	case r_config3:		/* 0xC */
		sc->config3 = data;
		break;
	case r_count_hi:	/* not implemented */
		break;
	default:
#ifdef ERROR_CHECKING		
		printm("53c94: Write to unimplemented register %d\n",r);
#endif
		break;
	}
	UNLOCK;
}

static void 
reg_io_print( int isread, ulong addr, ulong data, int len, void *usr )
{
	int r = (addr - get_mbase(sc->regs_range) )>>4;

#if 0
	if( isread && r==r_status )
		return;
#endif
#if 1
	printm("[%02X; %02X] ",sc->status, 
	       sc->latched_status_valid ? sc->latched_status : 0xff );
#endif

	if( isread )
		printm("53c94: %-10s read  %02lX ", reg_rnames[r], data );
	else
		printm("53c94: %-10s write %02lX ", reg_wnames[r], data );

	if( !isread && r == r_command ){
		char *cmdstr = cmd_names[ data & ~CMD_DMA_MODE ];
		printm("%s <%s>", data & CMD_DMA_MODE ? "DMA" : "   ",
		       cmdstr? cmdstr : "<__unknown__>");
	}
	printm("\n");
}

/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static io_ops_t ops = {
	.read	= reg_io_read,
	.write	= reg_io_write, 
	.print	= reg_io_print
};

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

static int 
s53c94_init( void )
{
	mol_device_node_t *dn;	
	struct scsi_unit *unit;
	pthread_mutexattr_t mutex_attr;
	gc_bars_t bar;
	irq_info_t irqinfo;
	
	if( !(dn=prom_find_devices("53c94")) || !is_gc_child(dn, &bar) )
		return 0;
	
	if( bar.nbars != 2 || prom_irq_lookup(dn, &irqinfo) != 2 ) {
		printm("53c94: Expecting 2 addrs and 2 intrs\n");
		return 0;
	}

	/* register unit on bus */
	if( !(unit=scsi_register_unit(&unit_pb, 7, NULL)) ) {
		printm("Could not register SCSI unit (need a second bus?).\n");
		return 0;
	}

	pthread_mutexattr_init( &mutex_attr );
	pthread_mutexattr_setkind_np( &mutex_attr, PTHREAD_MUTEX_RECURSIVE_NP );
	pthread_mutex_init( &sc->lock_mutex, &mutex_attr );
	pthread_mutexattr_destroy( &mutex_attr );

	sc->unit = unit;
	sc->phase = idling;
	/* XXX we should set the default id to 7 */
	sc->irq = irqinfo.irq[0];
	sc->dma_irq = irqinfo.irq[1];

	sc->regs_range = add_gc_range( bar.offs[0], 0x100, "53c94-regs", 0, &ops, NULL );
	allocate_dbdma( bar.offs[1], "53c94-dma", sc->dma_irq, dma_status_bits, 0 );

	printm("53c94 SCSI-driver installed (IRQs %d/%d)\n",sc->irq, sc->dma_irq );

/* DEBUG */
	dbdma_sbits_changed(sc->dma_irq);
	return 1;
}

static void 
s53c94_cleanup( void )
{
	pthread_mutex_destroy( &sc->lock_mutex );	
}

driver_interface_t s53c94_driver = {
	"53c94", s53c94_init, s53c94_cleanup
};
