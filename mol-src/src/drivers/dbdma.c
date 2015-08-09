/* 
 *   Creation Date: <1999-01-16 19:45:15 samuel>
 *   Time-stamp: <2004/02/07 18:34:15 samuel>
 *   
 *	<dbdma.c>
 *	
 *	Descriptor based DMA emulation
 *   
 *   Copyright (C) 1998-2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

/* #define VERBOSE */

#include <pthread.h>

#include "extralib.h"
#include "debugger.h"
#include "memory.h"
#include "dbdma.h"
#include "ioports.h"
#include "thread.h"
#include "verbose.h"
#include "driver_mgr.h"
#include "gc.h"
#include "dbdma_hw.h"
#include "byteorder.h"
#include "drivers.h"

SET_VERBOSE_NAME("dbdma");

/* #define DEBUG_DMA */
/* #define DEBUG_LOCK */
#define ERROR_CHECKING

#define NUM_IRQS	32
#define NUM_REGS	16

#define MASK_INSERT(dest, src, mask) \
	dest = ((src) & (mask)) | ((dest) & ~(mask))

#define IRQ_VALID(irq)	((irq)>=0 && (irq)<NUM_IRQS)

#ifdef DEBUG_DMA
#define DEBUG_STR(x)	do { printm(x); } while(0)
#else
#define DEBUG_STR(x)	do {} while(0)
#endif

/* Implementation remarks (these apply to the mac-io chip as well):
 *	1. ChannelTransferMode (optional) is not implemented
 *	2. KEY_REGS, KEY_DEVICE are not implemented
 *	3. KEY_SYSTEM is only implemented for LOAD/STORE_WORD
 *
 *	XXX: 	If the channel program runs in a loop without
 *		INPUT/OUTPUT commands or wait-conditions, 
 *		process_cmds will never return. 
 *	
 *	XXX:	ACTIVE flag is cleared as a response to PAUSE
 *		even though a reading from a OUPUT command might 
 *		still be progress.
 *	
 *	XXX: 	Much in this file is more or less untested.
 *
 *	4. We respond to a clear of the RUN bit immediately,
 *	   since this is the expected behaviour. This implies
 *	   that we must buffer input data (or we could be writing 
 *	   to a channel gone IDLE).
 */

static char *reg_names[] = {
	"CONTROL", "STATUS", "CMDPTR_HI", "CMDPTR",
	"INTR_SEL", "BR_SEL", "WAIT_SEL", "XFER_MODE",
	"DATA2PTR_HI", "DATA2PTR", "RES1", "ADDRESS_HI", 
	"BR_ADDR_HI", "RES2", "RES3", "RES4"
};

static char *cmd_names[] = {
	"OUTPUT_MORE", "OUTPUT_LAST", "INPUT_MORE", "INPUT_LAST", 
	"STORE_WORD", "LOAD_WORD", "DBDMA_NOP", "DBDMA_STOP"
};
static char *key_names[] = {
	"KEY_STREAM0", "KEY_STREAM1", "KEY_STREAM2", "KEY_STREAM3",
	"** RES **", "KEY_REGS", "KEY_SYSTEM", "KEY_DEVICE"
};

typedef union {
	unsigned int * p_int;	
	unsigned long * p_long;
} punfix_t;

enum {				/* DBDMA control registers. All little-endian */
	r_control=0,		/* let you change bits in status */
	r_status, 		/* dma and device status bits */
	r_cmdptr_hi, 		/* upper 32 bits of command addr */
	r_cmdptr,
	r_intr_sel, 		/* select interrupt condition bit */
	r_br_sel, 		/* select branch condition bit */
	r_wait_sel,		/* select wait condition bit */
	r_xfer_mode, 
	r_data_2ptr_hi,
	r_data2ptr,
	r_res1,
	r_address_hi,
	r_br_addr_hi,
	r_res2,
	r_res3,
	r_res4
};

#ifdef DEBUG_LOCK
#define LOCK( channel ) \
	do { printm("LOCKING...") ; pthread_mutex_lock( &channel->lock_mutex ); printm("OK\n"); } while(0)

#define UNLOCK( channel ) \
	do { printm("UNLOCKING..."); pthread_mutex_unlock( &channel->lock_mutex ); printm("OK\n"); } while(0)
#else
#define LOCK( channel )		pthread_mutex_lock( &channel->lock_mutex )
#define UNLOCK( channel )	pthread_mutex_unlock( &channel->lock_mutex );
#endif


/* state of current OUTPUT command */
struct io_state {
	int	req_count;
	int	res_count;	/* 0 when *no* data has been xfered */

	int	key;
	int	is_last;
	char	*buf;		/* linuxvirtual address */
};

struct ret_container {
	struct ret_container 	*next;
	ulong			ret;
};

struct dbdma_channel {
	int			irq;

	char			*name;		/* for debugging purposes */

	gc_range_t 		*gc_range;	
	long			user;		/* user specified information */

	/* locks and semaphores */
       	pthread_mutex_t 	lock_mutex;

	pthread_cond_t		outputing_cond;
	struct ret_container 	*wait_outputing_ret;

	pthread_cond_t		inputing_cond;
	struct ret_container 	*wait_inputing_ret;

	pthread_cond_t		ioing_cond;
	struct ret_container 	*wait_ioing_ret;

	int			pause_cond_count;
	pthread_cond_t		pause_cond;


	ulong 			identifier;	/* incremented and assigned to tasks */

	/* user procs */
	dma_sbits_func		status_bits_func;

	/* state variables */
	int			idle;		/* channel program not running */
	int			cmd_phase;	/* command interpreter phase (if !idle) */
	ulong			status;		/* B.E. (used instead of reg[r_status]) */


	char			status_byte;	/* private to default_status_bits() */
	ulong			reg[NUM_REGS+1]; /* (the +1 allows "of table" writing) */

	/* FIELDs valid if not idling */
	struct dbdma_cmd 	*cur_cmd;
	int			cur_cmd_in_rom;

	/* FIELDs valid only in some phases */
	int			may_branch;


	/* INPUT/OUTPUT state variables */
	struct io_state 	iostate;
};

/* cmd_phase */
enum{
	idling=0, preparing, starting, inputing, outputing, finishing, ioaccessing
};

#ifdef DEBUG_DMA
static char *phase_names[7] = {
	"IDLING", "PREPARING", "STARTING", "INPUTING",
	"OUTPUTING", "FINISHING", "IOACCESSING"
};
#endif

static struct dbdma_channel *ch_table[NUM_IRQS];


/************************************************************************/
/*	verbose output							*/
/************************************************************************/

static void
dump_registers( int irq )
{
	struct dbdma_channel *ch = ch_table[irq];
	if( !IRQ_VALID(irq) || !ch ) {
		printm("DBDMA: irq %d not valid\n", irq);
		return;
	}
	
	printm("%s [IRQ %d]  [%s %s %s %s %s %s %s] Sx: %02lX\n" ,
	       ch->name, ch->irq,
	       ch->status & RUN ? "RUN" : "run",
	       ch->status & PAUSE ? "PAUSE" : "pause",
	       ch->status & FLUSH ? "FLUSH" : "flush",
	       ch->status & WAKE ? "WAKE" : "wake",
	       ch->status & DEAD ? "DEAD" : "dead",
	       ch->status & ACTIVE ? "ACTIVE" : "active",
	       ch->status & BT ? "BT" : "bt",
	       ch->status & 0xff );
	printm("CmdPtr %08X  IntrSel %08X  BrSel %08X  WSel %08X\n",
	       ld_le32(&ch->reg[r_cmdptr]), ld_le32(&ch->reg[r_intr_sel]), 
	       ld_le32(&ch->reg[r_br_sel]), ld_le32( &ch->reg[r_wait_sel] ));
}

static void 
dump_dbdma_cmd( ulong mphys )
{
	struct dbdma_cmd *cp = (struct dbdma_cmd*)transl_ro_mphys( mphys );
	short cmd;
	punfix_t phy_addr;
	punfix_t cmd_dep;
	
	if( !cp ) {
		printm("dbdma: bad access\n");
		return;
	}
	if( mphys & 0xf )
		printm("WARNING: dbdma-command not properly aligned\n");

	cmd = ld_le16( &cp->command );
	printm("%s  [%08lX]   %s  ", cmd_names[(cmd>>12)&0x7], mphys,
	       key_names[(cmd>>8)&0x7] );
	printm("I:%02X  BR:%02X  W:%02X\n", (cmd & 0x30)>>4, 
	       (cmd&0xc)>>2, (cmd&0x3) );
	phy_addr.p_int = &(cp->phy_addr);
	cmd_dep.p_int = &(cp->cmd_dep);
	printm("req_count: %04X   phy_addr: %08X   cmd_dep: %08X\n", 
	       ld_le16( &cp->req_count ), ld_le32( phy_addr.p_long ), 
	       ld_le32( cmd_dep.p_long ) );
	printm("res_count: %04X   xfer_status: %04X\n",
	       ld_le16( &cp->res_count ), ld_le16( &cp->xfer_status ));
}

static void 
reg_io_print( int isread, ulong addr, ulong data, int len, long irq )
{
	struct dbdma_channel *ch = ch_table[irq];
	int	reg = (addr-get_mbase(ch->gc_range))>>2;

	printm("%s [IRQ %d], %s %s %08X\n", ch->name, ch->irq, reg_names[reg],
	       isread ? "read: " : "write:", ld_le32(&data) );
}



/************************************************************************/
/*	misc								*/
/************************************************************************/

static void
stamp_status( struct dbdma_channel *ch ) 
{
	int 	res_count = 0;
	ulong 	cmd;
	
	if( ch->idle || ch->cur_cmd_in_rom )
		return;

	cmd = ld_le16( &ch->cur_cmd->command ) & 0xf000;
	
	if( cmd == INPUT_MORE || cmd == INPUT_LAST || cmd == OUTPUT_MORE ||
	    cmd == OUTPUT_LAST )
		res_count = ch->iostate.req_count - ch->iostate.res_count;

	/* Urk. We use res_count to denote number of xfered bytes.
	 * DBDMA uses res_count to denote number of bytes not xfered.
	 */
	st_le16( &ch->cur_cmd->xfer_status, ch->status );
	st_le16( &ch->cur_cmd->res_count, res_count );
}

/* This function is called if the command processing is aborted; it should
 * release any allocated data and let the channel go idle (but ch->status
 * should be left alone).
 */
static void
go_idle( struct dbdma_channel *ch )
{
	ch->identifier++;

	ch->cmd_phase = idling;
	ch->idle = 1;
}

static void
raise_interrupt( int irq )
{
	/* irq_interrupt( irq ); */
	printm("UNIMPLEMENTED! Interrupt raised, irq = %d\n", irq );
}

static void
conditional_interrupt( struct dbdma_channel *ch )
{
	ulong 	cmd, sel;
	int	cond;
	int	s_status = ch->status & 0xff;

	cmd = ld_le16( &ch->cur_cmd->command ) & 0xf000;

	sel = ld_le32( &ch->reg[r_intr_sel] ) & 0x0f0f;
	cond = (s_status & (sel>>16)) == (s_status & sel);
	if( (cond && (sel && cmd & 0x0010)) || (!cond && (!sel && cmd & 0x0020)) )
		raise_interrupt( ch->irq );	

}

static void
kill_channel( struct dbdma_channel *ch )
{
	printm("DEAD condition in DMA channel (irq %d).\n", ch->irq );
	ch->status |= DEAD;
	ch->status &= ~ACTIVE;
	go_idle( ch );

	raise_interrupt( ch->irq );
}


/************************************************************************/
/*	load / store							*/
/************************************************************************/

/* It is necessary to relase the LOCK before calling do_io_read/do_io_write. 
 * Thus the channel might be in a different at return. A thread calling this
 * function might for instance be suspended while not holding the lock, so
 * the main thread could possibly clear the RUN etc.
 */
static void
load_word( struct dbdma_channel *ch, int key, ulong mac_phys, int len  )
{
	char	*src;
	ulong	val, identifier;

	VPRINT("DBDMA: WARNING, load_word untested, len %d @ %08lX\n",len, mac_phys );

	if( key != KEY_SYSTEM ){
		printm("DBDMA, load_word: Key %d is unimplemented\n", key );
		kill_channel(ch);
		return;
	}

	ch->cmd_phase = ioaccessing;
	identifier = ++ch->identifier;

	if( mphys_to_lvptr(mac_phys, &src) >=0 ){
		/* RAM/ROM */
		val = read_mem(src, len );
	} else {
		UNLOCK(ch );
		do_io_read( NULL, mac_phys, len, &val );
		LOCK( ch );
	}

	/* We use the identifier field to make sure the release of the lock
	 * did not have a nasty side effect. 
	 */
	if( identifier == ch->identifier && ch->cmd_phase == ioaccessing ){
		if( len == 2 ) { /* endian stuff */
			val = (val << 16) + (ch->cur_cmd->cmd_dep & 0xffff);
		} else if( len == 1 ) {
			val = (val << 24) + (ch->cur_cmd->cmd_dep & 0xffffff);
		}
		ch->cur_cmd->cmd_dep = val;
		ch->cmd_phase = finishing;
	}
}

static void
store_word( struct dbdma_channel *ch, int key, ulong mac_phys, ulong data, int len )
{
	char *dest;
	ulong	identifier;
	
	/* XXX is the endianess correct? */
	printm("DBDMA: WARNING store_word is untested\n");

	if( key != KEY_SYSTEM ){
		printm("DBDMA, store_word: Key %d is unimplemented\n", key );
		kill_channel(ch);
	}

	ch->cmd_phase = ioaccessing;
	identifier = ++ch->identifier;

	switch( mphys_to_lvptr(mac_phys, &dest) ){
	case 0:	/* RAM */
		write_mem( dest,data, len );
		break;
	case 1:	/* ROM */
		break;
	default:
		UNLOCK(ch);
		do_io_write( NULL, mac_phys, data, len );
		LOCK(ch);
		break;
	}

	/* We use the identifier field to make sure we are still in
	 * business. The release of the lock could cause unexpected 
	 * changes.
	 */
	if( identifier == ch->identifier && ch->cmd_phase == ioaccessing )
		ch->cmd_phase = finishing;	
}


/************************************************************************/
/*	cmd processing							*/
/************************************************************************/

static void
prepare_cmd( struct dbdma_channel *ch )
{
	ulong 	cp_phys;
	int 	s;

	union {
		struct dbdma_cmd ** p_cmd;
		char ** p_char;
	} p;
	
	cp_phys = ld_le32( &ch->reg[r_cmdptr]);
	
	/* check that the address is valid (in RAM/ROM) */
	p.p_cmd = &(ch->cur_cmd);
	s = mphys_to_lvptr( cp_phys, p.p_char );
	
	ch->cur_cmd_in_rom = 0;
	if( s<0 ) {
		kill_channel( ch );
		return;
	}
	else if( s==1 )
		ch->cur_cmd_in_rom = 1;

	ch->cmd_phase = starting;
}

static void
start_input( struct dbdma_channel *ch, int key, ulong mphys, int req_count, int is_last )
{
	/* Only read to RAM. Also, KEY_REGS, KEY_DEVICE and KEY_STREAM  
	 * are not implemented in the mac_io ship (we neglect them also)
	 */
	ch->iostate.buf = transl_mphys( mphys );
	if( !ch->iostate.buf || key > KEY_STREAM3 ) {
		kill_channel(ch);
		return;
	}
	ch->iostate.req_count = req_count;
	ch->iostate.res_count = 0;
	ch->iostate.key = key;
	ch->iostate.is_last = is_last;

	ch->cmd_phase = inputing;
}

static void
start_output( struct dbdma_channel *ch, int key, ulong mphys, int req_count, int is_last )
{
	ch->iostate.buf = transl_ro_mphys( mphys );
	if( !ch->iostate.buf || key > KEY_STREAM3 ) {
		kill_channel(ch);
		return;
	}
	ch->iostate.req_count = req_count;
	ch->iostate.res_count = 0;
	ch->iostate.key = key;
	ch->iostate.is_last = is_last;

	ch->cmd_phase = outputing;
}

static int
start_cmd( struct dbdma_channel *ch )
{
	struct 	dbdma_cmd *cp;
	short	cmd;
	int	key;
	ulong	addr;
	int	req_count;
	punfix_t phy_addr;

#ifdef DEBUG_DMA
	printm("\n");
	dump_registers(ch->irq);
	dump_dbdma_cmd( ld_le32( &ch->reg[r_cmdptr] ));
#endif
	
	cp = ch->cur_cmd;
	cmd = ld_le16( &cp->command );
	key = cmd & 0x700;
	cmd &= 0xf000;

	ch->may_branch = 0;

	/* clear WAKE flag at command fetch */
	ch->status &= ~WAKE;

	switch( cmd ){
	case OUTPUT_MORE:
	case OUTPUT_LAST:
	case INPUT_MORE:
	case INPUT_LAST:
		ch->may_branch = 1;

		req_count = ld_le16( &cp->req_count );
		phy_addr.p_int = &(cp->phy_addr);
		addr = ld_le32( phy_addr.p_long );
		
		if( key == 0x400 ) {
			printm("DBDMA: Reserved KEY 0x400\n");
			kill_channel(ch);
			break;
		}
		
		if( cmd == INPUT_MORE || cmd == INPUT_LAST ) {
			DEBUG_STR("starting input\n");
			start_input( ch, key, addr, req_count, cmd==INPUT_LAST );
		} else {
			DEBUG_STR("starting output\n");			
			start_output( ch, key, addr, req_count, cmd==OUTPUT_LAST );
		}
		break;
		
	case LOAD_WORD:
	case STORE_WORD:
		if( key < KEY_REGS ) {
			static int warned=0;
			if( warned++ < 3 )
				VPRINT("DBDMA [%d]: Illegal key type %d\n", ch->irq, key );
			key = KEY_SYSTEM;
		}
		phy_addr.p_int = &(cp->phy_addr);
		addr = ld_le32( phy_addr.p_long );
		req_count = ld_le16( &cp->req_count ) & 0x7;

		/* endian stuff */
		if( req_count & 0x4 ) {
			req_count = 4;
			addr &= ~3;
		} else if( req_count & 0x2 ) {
			req_count = 2;
			addr &= ~1;
		} else
			req_count = 1;
		
		if( (cmd & 0xf000) == LOAD_WORD )
			load_word( ch, key, addr, req_count );
		else {
			ulong value = cp->cmd_dep;

			if( req_count == 2 ) {
				value = value >> 16;
			} else if( req_count == 1 )
				value = value >> 24;

			store_word( ch, key, addr, value, req_count );
		}
		/* cmd_phase is set in load_word / store_word, it can't be
		 * set here, since an unexpected change might have occured
		 * due to the relase of the LOCK.
		 */
		break;
		
	case DBDMA_NOP:
		ch->may_branch = 1;
		ch->cmd_phase = finishing;
		break;
		
	case DBDMA_STOP:
		ch->status &= ~ACTIVE;
		ch->idle = 1;
		ch->cmd_phase = idling;
		return 0;
		
	default:
		printm("Reserved dbdma command %d\n", cmd & 0xf000 );
		kill_channel(ch);
		return 0;
	}

	return 1;
}

static void
do_dma_cancel_wait( struct dbdma_channel *ch, int flags, int retval )
{
	struct ret_container *rc;
	
	if( flags & DMA_READ ){
		for( rc=ch->wait_outputing_ret; rc; rc=rc->next )
			rc->ret = retval;
		ch->wait_outputing_ret = NULL;
		pthread_cond_broadcast( &ch->outputing_cond );
	}
	if( flags & DMA_WRITE ){
		for( rc=ch->wait_inputing_ret; rc; rc=rc->next )
			rc->ret = retval;
		ch->wait_inputing_ret = NULL;
		pthread_cond_broadcast( &ch->inputing_cond );
	}
	if( flags & DMA_RW ) {
		for( rc=ch->wait_ioing_ret; rc; rc=rc->next )
			rc->ret = retval;
		ch->wait_ioing_ret = NULL;
		pthread_cond_broadcast( &ch->ioing_cond );
	}
}

static int
do_inputing( struct dbdma_channel *ch, int may_block )
{
	do_dma_cancel_wait( ch, DMA_WRITE, 0 );
	return 0;
}

static int
do_outputing( struct dbdma_channel *ch, int may_block )
{
	do_dma_cancel_wait( ch, DMA_READ, 0 );
	return 0;
}

static int
finish_cmd( struct dbdma_channel *ch )
{
	int 	s_status, cond;
	int	inc_cp=1;
	ulong	cmd, sel;

	s_status = ch->status & 0xff;
	
	cmd = ld_le16( &ch->cur_cmd->command );

	/* WAIT */
	sel = ld_le32( &ch->reg[r_wait_sel] ) & 0x0f0f;
	cond = (s_status & (sel>>16)) == (s_status & sel);
	if( (cond && (sel && cmd & 0x0001)) || (!cond && (!sel && cmd & 0x0002) ))
		return 0;
	
	/* BRANCH */
	if( ch->may_branch ) {
		sel = ld_le32( &ch->reg[r_br_sel] ) & 0x0f0f;
		cond = (s_status & (sel>>16)) == (s_status & sel);
		ch->status &= ~BT;
		if( (cond && (sel && cmd & 0x0004)) || (!cond && (!sel && cmd & 0x0008)) ) {
			ch->reg[r_cmdptr] = ch->cur_cmd->cmd_dep;
			ch->status |= BT;
			inc_cp = 0;
		}
	}
	if( inc_cp ) {
		ulong cp_phys = ld_le32( &ch->reg[r_cmdptr] );
		st_le32( &ch->reg[r_cmdptr], cp_phys + sizeof( struct dbdma_cmd ));
	}

	/* STAMP */
	stamp_status( ch );
	
	/* INTERRUPT */
	conditional_interrupt( ch );

	/* OK, start processing next command */
	ch->cmd_phase = preparing;

	return 1;
}

/* It must be permissible for *anyone* who obtains the lock to call
 * this function. If may_block is set then the calling thread might call
 * a proc that will possibly block. If blocking is not permitted,
 * then a thread is assigned to the task.
 */
static void
process_cmds( struct dbdma_channel *ch, int may_block )
{
	int run=1;

	while( run && !(ch->status & PAUSE) ) {
#ifdef DEBUG_DMA
		printm("process_cmds (%d), PHASE: %s\n", ch->irq, phase_names[ch->cmd_phase] );
#endif	
		switch( ch->cmd_phase ) {
		case idling:
			run = 0;
			break;
		case preparing:
			prepare_cmd( ch );
			break;
		case starting:
			run = start_cmd( ch );
			break;
		case inputing:
			run = do_inputing( ch, may_block );
			break;
		case outputing:
			run = do_outputing( ch, may_block );
			break;
		case finishing:
			run = finish_cmd( ch );
			break;
		case ioaccessing:
			run = 0;
			break;
		}
	}
}

/* Write to control register. Mask is a combination of 
 *   RUN, PAUSE, WAKE, FLUSH
 * 
 * The contents of ch->status has been updated prior to the entry of
 * this function.
 */
static void
control_write( struct dbdma_channel *ch, ulong old_status, ulong mask )
{
	/* FLUSH */
	if( (ch->status & FLUSH) && !ch->idle ) {
		ulong cmd;

		DEBUG_STR("Flushing\n");
		cmd = ld_le16( &ch->cur_cmd->command ) & 0xf000;
		if( cmd == INPUT_MORE || cmd == INPUT_LAST ){
			/* perhaps we should accept flush for other commands too */
			stamp_status( ch );
			ch->status &= ~FLUSH;
		}
	}
	ch->status &= ~FLUSH;	/* Seems to be necessary... */

	/* STOP [RUN -> 0] */
	if( (mask & RUN) && !(ch->status & RUN) ) {
		ch->status &= ~DEAD;
		if( !ch->idle ){
			DEBUG_STR("Stopping DMA\n");

			stamp_status( ch );
			conditional_interrupt( ch );
			go_idle( ch );
		}
	}

	/* PAUSE released */
	if( (mask & PAUSE) && !(ch->status & PAUSE) && ch->pause_cond_count ){
		pthread_cond_broadcast( &ch->pause_cond );
		ch->pause_cond_count = 0;
		/* A process waiting on outputing_cond (due to the pause)
		 * will be restarted from do_outputing 
		 */
	}
	
	/* PAUSE.
	 * We implement PAUSE by freezing the current command phase.
	 * OUTPUT: No more "Data available" calls will be made.
	 * INPUT: Data input will be delayed
	 */
#ifdef DEBUG_DMA
	if( (ch->status & PAUSE) && !ch->idle)
		printm("Pausing running DMA...\n");
#endif

	/* ...OK lets see if we should start processing commands...  */
	if( (ch->status & RUN) && ch->idle && !(ch->status & DEAD) ) {
		prepare_cmd( ch );
		ch->idle = 0;
	}

	/* update ACTIVE status */
	ch->status &= ~ACTIVE;
	if( !ch->idle && !(ch->status & PAUSE) && !(ch->status & DEAD) ) {
		ch->status |= ACTIVE;

		/* off we go */
		process_cmds( ch, 0 /* don't block */);
	}
}


/************************************************************************/
/*	reg I/O								*/
/************************************************************************/

static ulong 
reg_io_read( ulong ptr, int len, long irq )
{
	struct dbdma_channel *ch = ch_table[irq];
	int	reg = (ptr-get_mbase(ch->gc_range))>>2;
	ulong 	rbuf[2], ret;

	LOCK( ch );

	/* Note: the registers are little-endian */
	ret = ch->reg[reg];
	
	switch( reg ){
	case r_control:
		ret = 0;
		break;
	case r_status:
		st_le32( &ret, ch->status );
		break;
	case r_cmdptr:
	case r_intr_sel: 	/* (opt) select interrupt condition bit */
	case r_br_sel:		/* (opt) select branch condition bit */
	case r_wait_sel:	/* (opt) select wait condition bit */
		break;
	case r_xfer_mode: 	/* (opt) */
	case r_cmdptr_hi:	/* (opt)  */
	case r_data_2ptr_hi:	/* (opt) */
	case r_data2ptr:	/* (opt) */
	case r_address_hi:	/* (opt) */
	case r_br_addr_hi:	/* (opt) */
		/* these registers are not implemented - we always return 0 */ 
		ret = 0;
		break;
	}

	rbuf[0] = ret;
	rbuf[1] = 0;		/* for unaligned access */

	ret = read_mem( (char*)&rbuf, len );

	UNLOCK( ch );
       	return ret;
}

static void
reg_io_write( ulong ptr, ulong data, int len, long irq )
{
	struct dbdma_channel *ch = ch_table[irq];
	int	reg = (ptr-get_mbase(ch->gc_range))>>2;
	ulong	v, old, mask, old_status;

#ifdef DEBUG_DMA
	reg_io_print( 0, ptr, data, len, irq );
#endif

	LOCK( ch );
	
	old = ch->reg[reg];		/* original value */
	
	write_mem( (char*)&ch->reg - get_mbase(ch->gc_range) + ptr, data, len );

	switch( reg ){
	case r_control:		/* DMA and device status bits */
		v=ld_le32( &ch->reg[reg] );
		mask = v>>16;
		old_status = ch->status;

		/* s0-s7 */
		if( mask & 0xff ) {
	       		ch->status &= ~0xff;
			ch->status |= ch->status_bits_func( ch->irq, mask&0xff, v&0xff) & 0xff;
		}
		/* control bits */
		MASK_INSERT( ch->status, v, mask );
		control_write( ch, old_status, mask );
		break;

	case r_status:
		break;
	case r_cmdptr:
		if( ch->status & (RUN | ACTIVE) ) {
			/* no effect if RUN or ACTIVE is set */
			st_le32( &ch->reg[reg], old );
		} else {
			/* should be 16-byte aligned */
			st_le32( &ch->reg[reg], ld_le32( &ch->reg[reg ]) & ~0xf );
		}
		break;

	case r_intr_sel: 	/* select interrupt condition bit */
	case r_br_sel:		/* select branch condition bit */
	case r_wait_sel:	/* select wait condition bit */
		break;
	case r_xfer_mode: 	/* the following registers are unused */
	case r_cmdptr_hi:	/* and return 0 on read */
	case r_data_2ptr_hi:
	case r_data2ptr:
	case r_address_hi:
	case r_br_addr_hi:
		break;
	}

	UNLOCK( ch );
}


/************************************************************************/
/*	client interface						*/
/************************************************************************/

int
dma_write_req( int irq, struct dma_write_pb *pb )
{
	struct dbdma_channel *ch = ch_table[irq];
	
	LOCK( ch );

	if( ch->status & PAUSE ) {
		UNLOCK( ch );
		return 1;
	}
	if( ch->cmd_phase != inputing ) {
		process_cmds( ch, 0 /* don't block */ );
		if( ch->cmd_phase != inputing ) {
			UNLOCK( ch );
			return 1;
		}
	}
	pb->res_count = ch->iostate.res_count;
	pb->req_count = ch->iostate.req_count;
	pb->is_last = ch->iostate.is_last;
	pb->key = ch->iostate.key;
	pb->identifier = ++ch->identifier;
	
	UNLOCK( ch );
	return 0;
}

int
dma_write_ack( int irq, char *buf, int len, struct dma_write_pb *pb )
{
	struct dbdma_channel *ch = ch_table[irq];
	int ret=0;
	
	LOCK(ch);
	if( pb->identifier != ch->identifier || ch->cmd_phase != inputing )
		ret = 1;
	else {
#ifdef ERROR_CHECKING
		if( ch->iostate.res_count+len > ch->iostate.req_count ){
			printm("DMA write_ack owerflow\n");
			len = ch->iostate.req_count - ch->iostate.res_count;
		}
#endif
		memcpy( ch->iostate.buf+ch->iostate.res_count, buf, len );
		ch->iostate.res_count += len;

		if( ch->iostate.res_count >= ch->iostate.req_count ){
			ch->cmd_phase = finishing;
			process_cmds( ch, 0 );
		}
	}
	UNLOCK(ch);
	return ret;
}	


/* Request data from DMA. Returns pointer and length
 * to current OUTPUT data. Returns a non-zero value if no
 * no data is available at the moment.
 */
int
dma_read_req( int irq, struct dma_read_pb *pb )
{
	struct dbdma_channel *ch = ch_table[irq];
	
	LOCK( ch );

	if( ch->status & PAUSE ) {
		UNLOCK( ch );
		return 1;
	}
	if( ch->cmd_phase != outputing ) {
		process_cmds( ch, 0 /* don't block */ );
		if( ch->cmd_phase != outputing ) {
			UNLOCK( ch );
			return 1;
		}
	}
	pb->res_count = ch->iostate.res_count;
	pb->req_count = ch->iostate.req_count;
	pb->buf = ch->iostate.buf;
	pb->is_last = ch->iostate.is_last;
	pb->key = ch->iostate.key;
	pb->identifier = ++ch->identifier;
	
	UNLOCK( ch );
	return 0;
}

/* Returns DMA_READ, DMA_WRITE or user value (if canceled).  
 * The flags parameter should be DMA_READ, DMA_WRITE or DMA_RW.
 */
int
dma_wait( int irq, int flags, struct timespec *abstimeout )
{
	struct dbdma_channel *ch = ch_table[irq];
	struct ret_container *ret, **ret_chain = NULL;
	pthread_cond_t *cond = NULL;
	int retval = DMA_ERROR;

	LOCK( ch );
	if( (flags & DMA_RW) == DMA_RW ) {
		if( !(ch->cmd_phase == outputing || ch->cmd_phase == inputing) || ch->status & PAUSE ) {
			ret_chain = &ch->wait_ioing_ret;
			cond = &ch->ioing_cond;
		} else {
			retval = DMA_WRITE;
			if( ch->cmd_phase == outputing )
				retval = DMA_READ;
		}
	} else if( flags & DMA_READ ) {
		if( ch->cmd_phase == outputing && !(ch->status & PAUSE) )
			retval = DMA_READ;
		else {
			ret_chain = &ch->wait_ioing_ret;
			cond = &ch->outputing_cond;
		}
	} else if( flags & DMA_WRITE ){
		if( ch->cmd_phase == inputing && !(ch->status & PAUSE) )
			retval = DMA_WRITE;
		else {
			ret_chain = &ch->wait_ioing_ret;
			cond = &ch->inputing_cond;
		}
	}

	/* If we're not ready to do DMA, the condition is set */
	if( cond ){
		ret = *ret_chain;
		*ret_chain = ret->next;

		if( abstimeout == NULL ){
			pthread_cond_wait( cond, &ch->lock_mutex );
			retval = ret->ret;
		} else {
			int err;
			for( ;; ){
				err = pthread_cond_timedwait( cond, &ch->lock_mutex, abstimeout );
				if( !err ) {
					retval = ret->ret;
					break;
				}
				if( err == EINTR ) {
					/* we must unchain ourselves if it timed out - not otherwise */
					for( ; *ret_chain != ret && *ret_chain ; *ret_chain = (*ret_chain)->next )
						;
					if( *ret_chain )
						*ret_chain = (*ret_chain)->next;
					else
						printm("Error: We were not in the chain!\n");
					retval = DMA_TIMEOUT;
					break;
				}
			}
		}
	}
	UNLOCK( ch );
	return retval;
}

void
dma_cancel_wait( int irq, int flags, int retval )
{
	struct dbdma_channel *ch = ch_table[irq];

	LOCK( ch );
	do_dma_cancel_wait( ch, flags, retval );
	UNLOCK( ch );
}

/* Return 1 if an error occurred (acknowledge of a
 * data no longer available, for instance after a clear of
 * the RUN bit.)
 */
int
dma_read_ack( int irq, int len, struct dma_read_pb *pb )
{
	struct dbdma_channel *ch = ch_table[irq];
	int ret=0;
	
	LOCK(ch);
	if( pb->identifier != ch->identifier || ch->cmd_phase != outputing )
		ret = 1;
	else {
		ch->iostate.res_count += len;
#ifdef ERROR_CHECKING
		if( ch->iostate.res_count > ch->iostate.req_count ){			
			printm("DMA read_ack owerflow\n");
			ch->iostate.res_count = ch->iostate.req_count;
		}
#endif
		if( ch->iostate.res_count >= ch->iostate.req_count ){
			ch->cmd_phase = finishing;
			process_cmds( ch, 0 );
		}
	}
	UNLOCK(ch);
	return ret;
}


/* This function must be called whenever "hardware" controlled status
 * bits change. In particular, this is important for the 
 * "wait" mechanism to work properly.
 */
void
dbdma_sbits_changed( int irq )
{
	int old_bits;
	struct dbdma_channel *ch = ch_table[irq];

	LOCK( ch );
	old_bits = ch->status & 0xff;
	ch->status &= ~0xff;
	ch->status |= ch->status_bits_func( ch->irq, 0,0 ) & 0xff;
	
	if( (ch->status & 0xff) ^ old_bits )
		process_cmds( ch, 0 /* don't block */ );
	UNLOCK( ch );
}


long
dbdma_get_user( int irq )
{	
	return ch_table[irq]->user;
}

/* By default, the status bits ChannelStatus.s0-s7 are
 * controled by software. This function should always return
 * s0-s7 and set bits specified by write_mask (possibly 0).
 */
static char
default_status_bits( int irq, int mask, char data )
{
	struct dbdma_channel *ch = ch_table[irq];
	
	ch->status_byte = (data & mask) | (ch->status_byte & ~mask);
	return ch->status_byte;
}

/************************************************************************/
/*	debugger CMDs							*/
/************************************************************************/

/* display registers (irq) */
static int __dcmd
cmd_dma_dr( int numargs, char **args ) 
{
	int irq, flag=0;

	if( numargs>2 )
		return 1;	
	if( numargs==2 ) {
		irq = string_to_ulong(args[1]);
		dump_registers(irq);
		return 0;
	}
	for(irq=0; irq<NUM_IRQS; irq++){
		if( !ch_table[irq] )
			continue;
		if( flag )
			printm("\n");
		dump_registers(irq);
		flag = 1;
	}
	return 0;
}

/* display dbdma-cmd at addr */
static int __dcmd
cmd_dma_dc( int numargs, char **args ) 
{
	ulong addr;

	if( numargs!=2 )
		return 1;	
	addr = string_to_ulong(args[1]);
	dump_dbdma_cmd( addr );
	return 0;
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static io_ops_t ops = {
	.read	= (io_read_fp)reg_io_read, 
	.write	= (io_write_fp)reg_io_write,
	.print	= (io_print_fp)reg_io_print
};

void 
allocate_dbdma( int gc_offs, char *name, int irq, dma_sbits_func status_bits_func, long user )
{
	struct dbdma_channel *ch;

	if( gc_offs > 0x10000 ) {
		LOG("%s is not inited with a GC offset!\n", name);
		return;
	}
	if( !IRQ_VALID(irq) || ch_table[irq] ) {
		printm("DBDMA-allocate error (IRQ=%d)\n",irq);
		return;
	}

	ch = calloc( sizeof(struct dbdma_channel), 1 );
	ch_table[irq] = ch;

	ch->irq = irq;
	ch->name = name ? name : "Dummy";
	ch->user = user;
	ch->idle = 1;

	ch->status_bits_func = status_bits_func ?
		status_bits_func : default_status_bits;

	pthread_mutex_init( &ch->lock_mutex, NULL );

	pthread_cond_init( &ch->pause_cond, NULL );
	pthread_cond_init( &ch->outputing_cond, NULL );
	pthread_cond_init( &ch->inputing_cond, NULL );
	pthread_cond_init( &ch->ioing_cond, NULL );

	/* register I/O area */
	ch->gc_range = add_gc_range( gc_offs, 256, ch->name, 0, &ops, (void*)irq );
}

void 
release_dbdma( int irq )
{
	struct dbdma_channel *ch = ch_table[irq];
	
	if( !IRQ_VALID(irq) || !ch ) {
		printm("DBDMA-channel %d is not allocated\n", irq);
		return;
	}

	pthread_mutex_destroy( &ch->lock_mutex );
	pthread_cond_destroy( &ch->outputing_cond );
	pthread_cond_destroy( &ch->inputing_cond );
	pthread_cond_destroy( &ch->ioing_cond );
	pthread_cond_destroy( &ch->pause_cond );

	free( ch );
	ch_table[irq] = NULL;
}

static int
dbdma_init( void )
{
	int i;

	for( i=0; i<NUM_IRQS; i++ )
		ch_table[i] = NULL;
	
	/* cmds */
	add_cmd( "dma_dr", "dma_dr [irq] \ndisplay dbdma-registers\n", -1, cmd_dma_dr );
	add_cmd( "dma_dc", "dma_dc phys_addr \ndisplay dbdma command\n", -1, cmd_dma_dc );

	VPRINT("Descriptor based DMA services inited\n");
	
	return 1;
}

static void
dbdma_cleanup( void )
{
	int i;
	
	for(i=0; i<NUM_IRQS; i++ )
		if( ch_table[i] )
			release_dbdma( i );
}

driver_interface_t dbdma_driver = {
    "dbdma", dbdma_init, dbdma_cleanup
};
