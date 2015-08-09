/* 
 *   Creation Date: <1999-02-04 06:04:39 samuel>
 *   Time-stamp: <2004/06/12 21:31:35 samuel>
 *   
 *	<scsi-bus.c>
 *	
 *	SCSI bus emulation
 *   
 *   Copyright (C) 1999, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <pthread.h>
#include <scsi/scsi.h>

#include "thread.h"
#include "scsi-bus.h"
#include "debugger.h"

/* XXX: After a abort condition, the xfer->buf pointer in do_handling_xxx() 
 * becomes invalid (perhaps freed).
 */

/* XXX: SCSI-reset and other abort conditions are not implemented, or 
 * may leave data dangling.
 */


#define ERROR_CHECKING 
/* #define DEBUG_SCSI */

#define DEFAULT_BLOCK_SIZE	(0x10000) /* was 8192  */

typedef struct scsi_unit {
	struct scsi_unit   *next;		/* linked list */
	void		   *usr_data;		/* user private data */

	int		scsi_id;     		/* unit id */
	int		data_available;		/* 1 if unit might have data available */
	scsi_unit_pb_t	pb;			/* user provided parameterblock */
} scsi_unit_t;

typedef struct xfer_info {
       	int		req_count;
	int		res_count;
	char		*buf;

	transfer_fp	xfer_proc;
	int		done_state;

	scsi_unit_t	*unit;
} xfer_info_t;

typedef struct mes_rec {
	struct mes_rec *next;
	int	len;
	char	*buf;
} mes_rec_t;

typedef struct data_rec {
	int	from_target;
       	size_t	req_count;
} data_rec_t;

typedef struct message_out_state {
	char	bufspace[4];		/* we normally use this buffer */
	char	*alloc_buf;
} message_out_state_t;

typedef struct message_in_state {
	mes_rec_t *cur_mes;
} message_in_state_t;

typedef struct command_state {
	char	bufspace[16];		/* no SCSI-2 commands are longer than 16 bytes */
} command_state_t;

typedef struct data_state {
	int	from_target;
	char	*buf;
} data_state_t;


typedef struct scsi_bus {
	pthread_mutex_t	lock_mutex;	/* Hard lock */

	ulong	identifier;		/* increased at abort etc */
	int	engine_lock;		/* == identifier if locking */

	scsi_unit_t *units;

	int	state;
	int	sphase;	

	/* the following two variables are only valid in message phases */
	int		save_state;
	int		save_sphase;
	xfer_info_t	*save_xfer;

	int	atn;
	scsi_unit_t	*initiator;
	scsi_unit_t	*target;

	/* queues / pending data (processing of it has *not* started) */
	mes_rec_t	*mes_queue;
	data_rec_t	data;
	char		ret_status;		/* status byte */
	int		ending_msg;		/* closing message (send after STATUS) */

	/* variables used in particular states */
	xfer_info_t		info_xfer;	/* messages */
	xfer_info_t		data_xfer;	/* cmd/data/status  */
	xfer_info_t		*xfer;

	message_out_state_t 	mo_state;
	message_in_state_t 	mi_state;
	command_state_t		cmd_state;
	data_state_t		data_state;
} scsi_bus_t;

/* state */
enum{
	idling=0, advancing, transfering,
	mes_out_ending, mes_handling,
	mes_in_ending,
	cmd_preparing, cmd_read_ending, cmd_handling,
	data_preparing, data_switching,
	status_preparing,
	bus_freeing, resetting_device
};

static scsi_bus_t bus[1];

static char *phase_names[12] = { 
	"DATA_OUT", "DATA_IN", "COMMAND", "STATUS",
	"RES1", "RES2", "MES_OUT", "MES_IN", 
	"BUS_FREE", "ARBITRATION", "RESELECTION", "SELECTION"
};

#ifdef DEBUG_SCSI
static char *state_names[20] = {
	"IDLING", "ADVANCING", "TRANSFERING",
	"MES_OUT_ENDING", "MES_HANDLING",
	"MES_IN_ENDING",
	"CMD_PREPARING", "CMD_READ_ENDING", "CMD_HANDLING",
	"DATA_PREPARING", "DATA_SWITCHING",
	"STATUS_PREPARING",
	"BUS_FREEING", "RESETTING_DEVICE"
};
#endif


#define LOCK	pthread_mutex_lock( &bus->lock_mutex );
#define UNLOCK 	pthread_mutex_unlock( &bus->lock_mutex );

#define CALL_USER \
	{ ulong _identifier; bus->engine_lock=_identifier=bus->identifier; UNLOCK;
#define IF_ABORTED \
	LOCK; if( _identifier != bus->identifier ) {
#define CALL_USER_END } else { bus->engine_lock--; } }


#ifdef ERROR_CHECKING
#define PREREQ( ph ) \
	if( bus->sphase != ph ) { \
		printm("Unexpected SCSI-phase %s in '%s()' (%s was expected)\n", \
		phase_names[bus->sphase], __FUNCTION__, phase_names[ph] ); }
#else
#define PREREQ( ph )
#endif

#ifdef DEBUG_SCSI
#define DUMP_MES(str) printm(str);
#define DUMP_MES2(str,p2) printm(str, p2);
#else
#define DUMP_MES(str) ;
#define DUMP_MES2(str,p2) ;
#endif

static void scsi_engine_entry( void *dummy );
static void scsi_engine( int may_block );

static int do_transfer( int may_block );

static int start_mes_out( void );
static void end_mes_out( void );
static int do_mes_handling( int may_block );

static int start_mes_in( void );
static void end_mes_in( void );

static void start_cmd_read( void );
static void end_cmd_read( void );
static int do_cmd_handling( int may_block );

static void start_data_transfer( void );
static void do_data_switching( void );
static void start_status_transfer( void );

static int get_mes_len( int len, char *buf );

static int default_mes_handler( int mes_len, unsigned char *buf, void *dummy_usr );
static int default_next_sphase( int sphase, void *usr );

static int go_next_phase( void );
static int conditional_mes_phase( void );
static int restore_save_phase( void );
static scsi_unit_t *lookup_unit( int scsi_id );

static void do_queue_mes( int len, char *buf, int put_first );

static int change_sphase( int new_sphase );


void 
scsi_bus_init( void )
{
	memset( bus, 0, sizeof(scsi_bus_t) );
	bus->identifier++;
	bus->sphase = sphase_bus_free;
	
	pthread_mutex_init( &bus->lock_mutex, NULL );
}

void
scsi_bus_cleanup( void )
{
	scsi_unit_t *uptr, *next;
	
	/* free units */
	for( uptr = bus->units; uptr; uptr = next ){
		next = uptr->next;
		free( uptr );
	}
	
	pthread_mutex_destroy( &bus->lock_mutex );
}

struct scsi_unit *
scsi_register_unit( scsi_unit_pb_t *pb, int scsi_id, void *usr_data )
{
	scsi_unit_t *unit;
	
	if( lookup_unit(scsi_id) )
		return NULL;

	unit = calloc( sizeof( scsi_unit_t),1 );
	unit->next = bus->units;
	bus->units = unit;

	unit->scsi_id = scsi_id;
	unit->pb = *pb;

	/* DEFAULT values */
	if( !unit->pb.block_size )
		unit->pb.block_size = DEFAULT_BLOCK_SIZE;

	if( !unit->pb.handle_mes_proc )
		unit->pb.handle_mes_proc = default_mes_handler;
	if( !unit->pb.next_sphase_proc )
		unit->pb.next_sphase_proc = default_next_sphase;

	unit->usr_data = usr_data;

	return unit;
}

static scsi_unit_t *
lookup_unit( int scsi_id )
{
	scsi_unit_t *unit;

	for( unit = bus->units; unit; unit=unit->next )
		if( unit->scsi_id == scsi_id )
			return unit;
	return NULL;
}


/* Returns 1 if an abort condition occured
 */
static int
change_sphase( int new_sphase )
{
	int old_sphase = bus->sphase;

	if( bus->initiator )
		bus->initiator->data_available = 1;
	bus->sphase = new_sphase;
	if( old_sphase != new_sphase ) {
		CALL_USER;
		bus->initiator->pb.sphase_changed_proc( old_sphase, new_sphase, bus->initiator->usr_data );
		IF_ABORTED return 1;
		CALL_USER_END;
	}
	return 0;
}

/* Returns 1 if arbitration was lost, -1 if aborted */
int 
scsi_arbitration( struct scsi_unit *unit )
{
	int ret=1;
	
	LOCK;
	if( bus->sphase == sphase_bus_free ) {
		bus->initiator = unit;
		ret=0;
		if( change_sphase( sphase_arbitration ))
			ret = -1;
	}
	UNLOCK;
	return ret;
}


/* returns 1 if target is non-existent, -1 if error, 0 if success. */
int
scsi_selection( struct scsi_unit *initiator, int target_id, int atn )
{
	int ret=1;
	scsi_unit_t *targ;

	LOCK;

	if( bus->sphase != sphase_arbitration || bus->initiator != initiator ){
		UNLOCK;
		printm("scsi_selection, error!\n");
		return -1;
	}
	targ = lookup_unit( target_id );
	
	if( targ && targ->pb.can_be_target ) 
	{
		if( change_sphase( sphase_selection ) ){
			UNLOCK;
			return -1;
		}
		bus->target = targ;
		ret = 0;
		bus->atn = atn;

		bus->state = advancing;
#if 0
		if( go_next_phase() ){
			UNLOCK;
			return -1;
		}
#endif
		scsi_engine( 0 /* no blocking */ );
	} else {
		if( bus->sphase == sphase_arbitration )
			change_sphase( sphase_bus_free );
	}
	UNLOCK;
	return ret;
}


void
scsi_set_atn( int atn )
{
	/* should we do the locking when setting ATN? */
	LOCK;
	bus->atn = atn;
	UNLOCK;
}

void
scsi_reset( void )
{
	LOCK;
	bus->atn = 0;
	bus->state = idling;
	change_sphase( sphase_bus_free );
	bus->target = NULL;
	bus->initiator = NULL;
	/* 
	 * Add identifier to make sure any operations in progress
	 * are properly aborted.
	 */
	bus->identifier++;
	printm("SCSI-reset may be poorly implemented!\n");
	UNLOCK;
}


static void
scsi_engine_entry( void *dummy )
{
	LOCK;
	scsi_engine( 1 );
	UNLOCK;
}


void
scsi_engine( int may_block )
{
	int run=1;

	while( run && bus->engine_lock != bus->identifier ) {
#ifdef DEBUG_SCSI
		printm("ENGINE: %s\n", state_names[bus->state] );
#endif		

		switch( bus->state ) {
		case idling:
			run = 0;
			break;

		case transfering:
			run = do_transfer( may_block );
			break;

		case mes_out_ending: /* I => T */
			end_mes_out();
			break;
		case mes_handling:
			run = do_mes_handling( may_block );
			break;

		case mes_in_ending:
			end_mes_in();
			break;

		case cmd_preparing:
			start_cmd_read();
			break;
		case cmd_read_ending:
			end_cmd_read();
			break;
		case cmd_handling:
			run = do_cmd_handling( may_block );
			break;
			
		case data_preparing:
			start_data_transfer();
			break;
		case data_switching:
			do_data_switching();
			break;

		case status_preparing:
			start_status_transfer();
			break;

		case advancing:
			if( go_next_phase() )
				run = 0;
			break;

		case resetting_device: /* not entire SCSI-chain */
			/* XXX reset device */
			/* fall through */
		case bus_freeing:
			run = 0;
			bus->state = idling;
			if( change_sphase( sphase_bus_free ) )
				break;
			bus->target = NULL;
			bus->initiator = NULL;
			break;
		default:
			printm("SCSI-BUS: Unimplemented state %d\n", bus->state );
		}
	}
}

/* go_next_phase should be called at the completion of each
 * non-message phase.
 *
 * Returns 1 if aborted.
 */
static int
go_next_phase( void )
{
	int val, sphase, again;

	DUMP_MES2("go_next_phase (is %s)\n",phase_names[bus->sphase] );

	/* Check for MESSAGE_IN or MESSAGE_OUT requests */
	val = conditional_mes_phase();
	if( val )
		return (val==-1)? 1 : 0;
	
#ifdef ERROR_CHECKING
	if( bus->sphase == sphase_mes_in || bus->sphase == sphase_mes_out )
		printm("SCSI-BUS: Message phase in go_next_phase\n");
#endif
	
	sphase = bus->sphase;
	do {
		again = 0;

		CALL_USER;
		sphase = bus->target->pb.next_sphase_proc( sphase, bus->target->usr_data );
		IF_ABORTED return 1;
		CALL_USER_END;
		
		switch( sphase ){
		case sphase_command:
			bus->state = cmd_preparing;
			break;
		case sphase_data_in:
		case sphase_data_out:
			bus->state = data_preparing;
			/* Skip data phase if no data to transfer */
			if( !bus->data.req_count )
				again = 1;
			break;
		case sphase_status:
			bus->state = status_preparing;
			break;
		case sphase_bus_free:
			bus->state = bus_freeing;
			break;
		default:
			printm("Illegal phase %d return from next_phase proc\n", sphase);
			bus->state = bus_freeing;
		}
	} while( again );
	return 0;
}

/* MESSAGE_OUT if ATN is set
 * MESSAGE_IN if ingoing message is pending
 * Returns 
 * 	0 if no message phase was started
 * 	1 if a message phase has been started,
 *	-1 if aborted
 */
static int
conditional_mes_phase( void )
{
	if( bus->atn || bus->mes_queue ){
		if( bus->sphase != sphase_mes_in && bus->sphase != sphase_mes_out ) {
			bus->save_state = bus->state;
			bus->save_sphase = bus->sphase;
			bus->save_xfer = bus->xfer;
		}
#ifdef DEBUG_SCSI
		printm("conditional_mes_phase in state %s\n", 
			state_names[bus->state] );
#endif
		if( bus->atn ) {
			if( start_mes_out() )
				return -1;
		} else if( start_mes_in() )
			return -1;
		return 1;
	}
	return 0;
}

/* This function should be called at the end of a transfered message.
 * Returns 1 if aborted
 */
static int
restore_save_phase( void )
{
	int val;
#ifdef ERROR_CHECKING
	if( bus->sphase != sphase_mes_out && bus->sphase !=sphase_mes_in )
		printm("restore_save_phase called from NON-message phase\n");
#endif
	/* Check for further messages to transfer */
	val = conditional_mes_phase();
	if( val )
		return (val==-1)? 1 : 0;

	/* restore saved phase */
	bus->state = bus->save_state;
	bus->xfer = bus->save_xfer;
#ifdef DEBUG_SCSI
	printm("Resore sphase: will return to state %s\n",state_names[bus->state] );
#endif
	if( change_sphase( bus->save_sphase ))
		return 1;
	return 0;
}


static int
do_transfer( int may_block )
{
	int		count;
	xfer_info_t	*xf = bus->xfer;

	if( !xf->unit->data_available )
		return 0;

	count = 0;
	if( xf->req_count - xf->res_count > 0 ) {
		CALL_USER;
		count = xf->xfer_proc( xf->req_count-xf->res_count, 
				       xf->buf + xf->res_count, may_block, bus->sphase, xf->unit->usr_data );
		IF_ABORTED return 0;
		CALL_USER_END;
	}

	if( count<0 ){
		if( count == ERR_MAY_BLOCK ) {
			create_thread( scsi_engine_entry, NULL, "SCSI-BUS-thread" );
			return 0;
		}
		if( count == ERR_NO_DATA ){
			/* ERR_NO_DATA is return if the proc made a call
			 * to set_xfer_status with data_available set to
			 * false. We *can't* be certain that this is still is
			 * the case.
			 */
			return xf->unit->data_available;
		}
		/* Should not come here */
		printm("xfer_proc return malicious value\n");
		return 0;
	}

#ifdef ERROR_CHECKING
	if( count > xf->req_count - xf->res_count ) {
		printm("SCSI-BUS: buffer OVERFLOW %d %d!\n", 
		       count, xf->req_count - xf->res_count);
		count = xf->req_count - xf->res_count;

	}
#endif
	xf->res_count += count;

	if( xf->res_count == xf->req_count ) {
		/* If data_available is clear *after* all data has been sent,
		 * but before a new phase has been set, we freeze the current
		 * phase. This feature emulates the behaviour of a real 
		 * controllers which typically keep the ACK signal until the user
		 * has decided whether ATN should be set or not.
		 */
		if( !xf->unit->data_available )
			return 0;
		bus->state = xf->done_state;
	}
	return 1;
}

void 
scsi_xfer_status( struct scsi_unit *unit, int data_available )
{
	int old;
	LOCK;

	old = unit->data_available;
	unit->data_available = data_available;
	if( !old && data_available && bus->state == transfering )
		scsi_engine(0);
	UNLOCK;
}


/* Respond to ATN signal. Returns 1 if an abort condition
 * occured.
 */
static int
start_mes_out( void )
{
	message_out_state_t *mos = &bus->mo_state;
	xfer_info_t *xf;
	
	if( mos->alloc_buf ){
		free( mos->alloc_buf );
		mos->alloc_buf = NULL;
	}
	xf = &bus->info_xfer;

	xf->res_count = 0;
	xf->req_count = 1;
	xf->buf = mos->bufspace;
	xf->xfer_proc = bus->initiator->pb.mes_req_proc;
	xf->done_state = mes_out_ending;
	xf->unit = bus->initiator;
/*	xf->unit->data_available = 1; */

	bus->xfer = xf;
	bus->state = transfering;
	return change_sphase( sphase_mes_out );
}


/* One byte of the message has been received.
 * Determine how many further bytes there are.
 * If done, handle the message.
 */
static void
end_mes_out( void )
{
	message_out_state_t *mos = &bus->mo_state;
	xfer_info_t *xf = bus->xfer;
	int 	len;
	PREREQ( sphase_mes_out );

	/* Have we got the whole message? */
	len = get_mes_len( xf->req_count, xf->buf );

	if( len == xf->req_count )
		bus->state = mes_handling;
	else {
		if( len > 4 && xf->req_count <= 4 ) {
			mos->alloc_buf = malloc( len );
			memcpy( mos->alloc_buf, xf->buf, xf->req_count );
			xf->buf = mos->alloc_buf;
		}
		xf->req_count = len;
		bus->state = transfering;
	}
}

static int
do_mes_handling( int may_block ) 
{
	xfer_info_t *xf = bus->xfer;
	int 	what;
	/* char	 mes_byte; */

	PREREQ( sphase_mes_out );

	CALL_USER;
	what = bus->target->pb.handle_mes_proc( xf->req_count, (unsigned char *) xf->buf, bus->target->usr_data );
	IF_ABORTED return 0;
	CALL_USER_END;

	if( what == SCSI_DEF_MES_HANDLER ){
		/* currently the CALL_USER guard is unnecessary here */
		CALL_USER;
		what = default_mes_handler( xf->req_count, (unsigned char *)xf->buf, NULL );
		IF_ABORTED return 0;
		CALL_USER_END;

	}

	/* we perform a few services from here */
	switch( what ){
	case SCSI_FREE_BUS:
		bus->state = bus_freeing;
		break;
	case SCSI_MES_REJECT:
#if 0
		mes_byte = MESSAGE_REJECT;
		do_queue_mes( 1, &mes_byte, 1 /*put first in queue*/ );
#else
		/* This works better with the 53c94 code - problably due
		 * to an error in the implementation. The SCSI-2 specs are
		 * probably violated.
		 */
		printm("scsi-bus: Going busfree instead of sending a reject message!\n");
		bus->state = bus_freeing;
#endif
		break;
	}

	/* If we haven't gone to a new phase/state, proceed */
	if( bus->state == mes_handling )
		if( restore_save_phase() ) 
			return 0;		/* aborted */
	return 1;
}

/* PREREQ: queued message 
 * Returns 0 if aborted/error
 */
static int
start_mes_in( void )
{
	message_in_state_t *mis = &bus->mi_state;
	xfer_info_t *xf;

#ifdef ERROR_CHECKING
	if( !bus->mes_queue ) {
		printm("SCSI-BUS, start_mes_in: queue empty!\n");
		return 0;
	}
#endif
	/* we release sent messages _next_ time we get here */
	if( mis->cur_mes )
		free( (char*)mis->cur_mes );
	mis->cur_mes = bus->mes_queue;
	bus->mes_queue = bus->mes_queue->next;

	xf = &bus->info_xfer;
	xf->buf = mis->cur_mes->buf;
	xf->req_count = mis->cur_mes->len;
	xf->res_count = 0;
	xf->xfer_proc = bus->initiator->pb.mes_prov_proc;
	xf->done_state = mes_in_ending;
	xf->unit = bus->initiator;
/*	xf->unit->data_available = 1; */

	bus->xfer = xf;
	bus->state = transfering;
	return change_sphase( sphase_mes_in );
}

static void
end_mes_in( void )
{
	PREREQ( sphase_mes_in );
	restore_save_phase();		/* can be aborted */
}

static void
start_cmd_read( void )
{
	command_state_t *cs = &bus->cmd_state;
	xfer_info_t *xf;
	
	xf = &bus->data_xfer;
	xf->res_count = 0;
	xf->req_count = 6;
	xf->buf = cs->bufspace;
	xf->xfer_proc = bus->initiator->pb.cmd_req_proc;
	xf->done_state = cmd_read_ending;
	xf->unit = bus->initiator;
/*	xf->unit->data_available = 1; */
	
	bus->xfer = xf;
	bus->state = transfering;
	change_sphase( sphase_command );
}

static int cmd_len[8] = { 
	6, 10, 10, -1, -1, 12, -1, -1 };

static void
end_cmd_read( void )
{
	xfer_info_t *xf = bus->xfer;
	int 	len;
	PREREQ( sphase_command );

	/* Have we received the whole command? */
	len = cmd_len[ xf->buf[0] >> 5 ];
	if( len == -1 )
		len = xf->req_count;

	bus->state = cmd_handling;
	if( len != xf->req_count ) {
		xf->req_count = len;
		bus->state = transfering;
	}
}

static int
do_cmd_handling( int may_block )
{
	xfer_info_t	*xf = bus->xfer;
	int		ret;

	CALL_USER;
	ret = bus->target->pb.handle_cmd_proc( xf->req_count, (unsigned char *) xf->buf, may_block, bus->target->usr_data );
	IF_ABORTED return 0;
	CALL_USER_END;

	if( ret ){
		if( may_block )
			printm("do_cmd_handling: unexpected error\n");
		create_thread( scsi_engine_entry, NULL, "SCSI-BUS-CMD-thread" );
		return 0;
	}
	go_next_phase();
	return 1;
}

static void
start_data_transfer( void )
{
	data_state_t 	*ds = &bus->data_state;
	xfer_info_t 	*xf;
	int		len;
	scsi_unit_t	*unit;	

       	/* We allow messages to be transfered between data blocks */
	if( conditional_mes_phase() )
		return;

	/* Request next phase if no data to transfer */
	if( !bus->data.req_count  ){
		if( ds->buf ) {
			free( ds->buf );
			ds->buf = NULL;
		}
		go_next_phase();
		return;
	}
	ds->from_target = bus->data.from_target;

	unit = bus->data.from_target ? bus->target : bus->initiator;

	/* we always let the sender of data decide the prefered block size */
	len = unit->pb.block_size;
	if( len > bus->data.req_count )
		len = bus->data.req_count;
	bus->data.req_count -= len;

	if( ds->buf )
		free( ds->buf );

	ds->buf = malloc( len );

	xf = &bus->data_xfer;
	xf->done_state = data_switching;
	xf->xfer_proc = unit->pb.data_req_proc;
	xf->res_count = 0;
	xf->req_count = len;
	xf->buf = ds->buf;
	xf->unit = unit;
/*	xf->unit->data_available = 1; */

	bus->xfer = xf;
	bus->state = transfering;
	change_sphase( ds->from_target ? sphase_data_in : sphase_data_out );
}

/* A block has been received from target, setup for transfer
 * to initiator (or vice versa).
 */
static void
do_data_switching( void )
{
	scsi_unit_t *unit;
	xfer_info_t *xf = bus->xfer;

#ifdef ERROR_CHECKING
	if( bus->sphase != sphase_data_out && bus->sphase != sphase_data_in )
		printm("SCSI-BUS, do_data_switching: Not a data phase!\n");
#endif	
	xf->done_state = data_preparing;
	xf->res_count = 0;
	unit = bus->data_state.from_target ? bus->initiator : bus->target;
	xf->unit = unit;
/*	xf->unit->data_available = 1; */

	xf->xfer_proc = unit->pb.data_prov_proc;
	bus->state = transfering;
}

static void
start_status_transfer( void )
{
/*	command_state_t *cs = &bus->statusing; */
	xfer_info_t *xf;
	
	xf = &bus->data_xfer;
	xf->res_count = 0;
	xf->req_count = 1;
	xf->buf = &bus->ret_status;
	
	xf->xfer_proc = bus->initiator->pb.status_prov_proc;
	xf->done_state = advancing;
	xf->unit = bus->initiator;
/*	xf->unit->data_available = 1; */

	bus->xfer = xf;
	bus->state = transfering;
	if( change_sphase( sphase_status ) )
		return;

	/* queue ending message */
	if( bus->ending_msg != -1 )
		do_queue_mes( 1, (char*)&bus->ending_msg, 0 /* at end */ );
}


void
scsi_set_data_request( int req_count, int from_target )
{
	LOCK;	
	bus->data.req_count = req_count;
	bus->data.from_target = from_target;
	UNLOCK;
}

/* Set status (e.g. GOOD) and closing message 
 * (typically COMMAND_COMPLETE).
 * The ending message will not be sent if ending_msg
 * is set to -1.
 */ 
void
scsi_set_status( int ret_status, int ending_msg )
{
	LOCK;
	bus->ret_status = ret_status;
	bus->ending_msg = ending_msg;
	UNLOCK;
}


void
scsi_queue_mes( int len, char *buf, int put_first )
{
	LOCK;
	do_queue_mes( len, buf, put_first );
	UNLOCK;
}


static void 
do_queue_mes( int len, char *buf, int put_first )
{
	mes_rec_t *mr, **mp;

	mr = (mes_rec_t*)malloc( len + sizeof(mes_rec_t) );
	mr->buf = (char*)mr + sizeof(mes_rec_t );
	mr->next = NULL;

	memcpy( mr->buf, buf, len );
	mr->len = len;

	if( put_first ){
		mr->next = bus->mes_queue;
		bus->mes_queue = mr;
	} else {
		/* insert last */
		for( mp = &bus->mes_queue; *mp; mp=&((*mp)->next) )
			;
		*mp = mr;
	}
}
	

void
scsi_flush_mes_queue( void )
{
	mes_rec_t *mr, *next;
	LOCK;
	for( mr=bus->mes_queue; mr; mr=next ){
		next = mr->next;
		free( mr );
	}
	bus->mes_queue = NULL;
	UNLOCK;
}
 

/* The default next_sphase_proc implements the ordinary
 *
 *    SELECTION/RESELECTION -> MES_OUT -> COMMAND -> DATA -> 
 *	-> STATUS -> MES_IN -> BUS_FREE
 * 
 * Phases preceding SELECTION/RESELECTION are not handled
 * from here.
 */
static int
default_next_sphase( int sphase, void *dummy_usr )
{
	int ret;

	LOCK;
	ret = sphase_bus_free;
	switch( sphase ){
	case sphase_selection:
	case sphase_reselection:
		ret = sphase_command;
		break;
	case sphase_command:
		/* data_in or data_out does matter here */
		ret = sphase_data_in;
		break;
	case sphase_status:
		ret = sphase_bus_free;
		break;
	case sphase_data_in:
	case sphase_data_out:
		ret = sphase_status;
		break;
	default:
		printm("Unexpected phase %d in default_next_sphase\n", sphase);
		ret = sphase_bus_free;
		break;
	}
	UNLOCK;
	return ret;
}


/* Default handling of message in received during
 * the MESSAGE_OUT phase.
 */
static int
default_mes_handler( int mes_len, unsigned char *buf, void *dummy_usr )
{
	int ret = 0;
#ifdef DEBUG_SCSI
	int i;
	printm("SCSI message handler, MES: ");
	for(i=0; i<mes_len; i++ )
		printm("%02X ", buf[i] );
	printm("\n");
#endif	

	/* BEWARE: this proc is running unlocked! */

	if( mes_len != 1 ) {
		printm("scsi-bus: MESSAGE_REJECTED (by busfreeing)\n");
		return SCSI_MES_REJECT;
	}

	/* IDENTIFY */
	if( buf[0] >= 0x80 ) {
		/* XXX: If we receive two IDENTIFY messages, then
		 * we should go bus free after the second one.
		 */
		if( buf[0] & 0x3f ){
			printm("scsi-bus: MESSAGE_REJECTED\n");
			return SCSI_MES_REJECT;
		}
		return 0;
	}

	switch( buf[0] ){
	case ABORT:
		ret = SCSI_FREE_BUS;
		break;
	case BUS_DEVICE_RESET:
		ret = SCSI_FREE_BUS;
		break;
	case INITIATOR_ERROR:
	case MSG_PARITY_ERROR:
		/* We should not receive these messages, but if we
		 * do it is best to abort. (perhaps SCSI-reset?)
		 */
		ret = SCSI_FREE_BUS;
		break;
	case MESSAGE_REJECT:
	case NOP:
		ret = 0;
		break;
	default:
		ret = SCSI_MES_REJECT;
		break;
	}
	return ret;
}

/* Determine length of message (the whole message has 
 * been fetched when the return value equals the parameter len)
 */
static int
get_mes_len( int len, char *buf )
{
	if( buf[0] == 0 ){
		return 1;
	}
	if( buf[0] == 0x1 ){
		/* Extended message. Further length in next byte */
		if( len > 1 )
       			return buf[1]+2;
		else
			return 2;	/* intermediate....*/
	}
	if( buf[0] <= 0x1f )
		return 1;
	if( buf[0] <= 0x2f )
		return 2;

	/* messages 0x30 - 0x7f are reserved, 
	 * we treat them as 1 byte long
	 */
	return 1;
}


