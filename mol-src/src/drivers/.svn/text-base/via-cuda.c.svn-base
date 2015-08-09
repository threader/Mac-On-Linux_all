/*
 *   via-cuda.c
 *   copyright (c)1997, 1999, 2000, 2001, 2002, 2003, 2004 Samuel Rydh
 *   re-written from original sources by Ben Martz
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "mol_config.h"

#include <stdarg.h>
#include <pthread.h>
#include <sys/time.h>

#include "via-cuda.h"
#include "cuda_hw.h"
#include "via_hw.h"
#include "adb.h"
#include "debugger.h"
#include "timer.h"
#include "ioports.h"
#include "promif.h"
#include "pic.h"
#include "res_manager.h"
#include "gc.h"
#include "driver_mgr.h"
#include "verbose.h"
#include "mac_registers.h"		/* XXX temporary */
#include "session.h"
#include "booter.h"

SET_VERBOSE_NAME("via-cuda");


/************************************************************************/
/*	CONSTANTS							*/
/************************************************************************/

#define VIA_TICKS_TO_USECS_NUM		6000	/* Time frequency: 4700000/6 Hz */
#define VIA_TICKS_TO_USECS_DEN	        4700

/* Setting these too low will prevent MacOS from booting */
#define VIA_SR_SHIFT_DELAY		60	/* was 300 */
#define CUDA_TREQ_DELAY			10	/* was 150 */

#define	VIA_BUFFER_LEN			16

#define CUDA_RTC_OFFSET			0x7C25BE58


/************************************************************************/
/*	STATE VARIABLES							*/
/************************************************************************/

static struct {
	pthread_mutex_t	lock;

	int		irq;
	char		reg[ VIA_REG_COUNT ];

	ullong		t1_stamp, t2_stamp;
	int		t1_id, t2_id;

	int		sr_active;
	int		hack_sr_shift_delay;

	gc_range_t	*reg_range;
} via;

static struct {
	pthread_mutex_t	lock;

	int		tack, tip;	/* from VIA */
	int		treq;

	char		out_buffer[ VIA_BUFFER_LEN ];
	int		out_count, out_req_count;
	int		fill_out_buffer;
	
	char		in_buffer[ VIA_BUFFER_LEN ];
	int		in_count;

	ulong		rtc;
	time_t		rtc_stamp;

	int		onesec_enabled, onesec_pending;
	ulong		onesec_last;

	int		autopoll;

	int		hack_treq_delay;
} cuda;

static int		via_cuda_verbose;

/************************************************************************/
/*	STATIC FUNCTIONS						*/
/************************************************************************/

/* VIA */
static void	via_timer_update( int is_timer1 );
static void	via_timer_start( int is_timer1 );
static void	via_expire_timer( int id, void *dummy, int latency );

static char	via_shift_out( void );
static void	via_shift_in( char c );
static void	via_dummy_shift( void );

static void	via_sr_int_post( int set_sr_data_flag );
static void	via_sr_int( int id, void *data_flag, int latency );

/* CUDA */
static void	cuda_handshake( int tack, int tip );
static void	cuda_sr_idle( void );
static int	cuda_msg_avail( void );
static void	cuda_incoming( void );
static void	cuda_poll( void );
static void	cuda_packet( int cmd,char *data,int len );
static void	cuda_power_down( int id, void *dummy, int latency );
static void	cuda_one_sec_timer( int id, void *dummy, int latency );
static void 	cuda_start_reply( int id, void *usr, int latency );


/************************************************************************/
/*	MISC								*/
/************************************************************************/

static char *reg_names[] = {
        "B","A","DIRB","DIRA","T1CL","T1CH","T1LL","T1LH",
        "T2CL","T2CH","SR","ACR","PCR","IFR","IER","ANH"
};

/* inline a frequently used routine */
#define UPDATE_IRQ	do { \
			if(via.reg[IFR] & via.reg[IER] & 0x7f) { \
				/*printm("VIA irq\n");*/ \
				irq_line_hi(via.irq); \
			} else { \
				 irq_line_low(via.irq); \
			 }} while(0)



#define V_LOCK            pthread_mutex_lock(&via.lock);
#define V_UNLOCK          pthread_mutex_unlock(&via.lock);

#define C_LOCK            pthread_mutex_lock(&cuda.lock);
#define C_UNLOCK          pthread_mutex_unlock(&cuda.lock);


/************************************************************************/
/*	VIA implementation						*/
/************************************************************************/

static ulong
via_io_read( ulong addr, int len, void *x )
{
	int reg, ret;
	ulong base = get_mbase( via.reg_range );

	V_LOCK;
	
	reg = (addr - base) / VIA_REG_LEN;
	ret = via.reg[reg];
	
	switch( reg ) {
	case B:
		/* we should possibly clear the shift register intterupt here */
		ret &= via.reg[DIRB];
		if(cuda.treq) 
			ret |= TREQ;
		break;
	case T1CL:
		via.reg[IFR] &= ~T1_INT;
		UPDATE_IRQ;
	case T1CH:	/* fall through */
#if 1
		/* XXX: TEMPORARY Hack to fix G3 calibration */
		if( mregs->nip == 0xfff8407c  ){
			static int hack_count=0;
			if( hack_count++ == 4 ){
				mregs->nip = 0xfff840c8 - 4;
				printm("G3 calibration HACK in via-cuda.c\n");
				/* stop_emulation(); */
			}
		}
#endif
#if 0
		/* XXX: Hack to fix the 9600-ROM calibration */
		if( mregs->nip == 0xfff039d8 ) {
			static int hack_count=0;
			if( hack_count++ == 4 ){
				mregs->gpr[5]=mregs->gpr[26];
				mregs->nip = 0xfff03a14 - 4;
				printm("9600 Hack in calibration hack in via-cuda\n");
			}
		}
#endif
		via_timer_update(1);
		ret = via.reg[reg ];
		break;
	case T2CL:
		via.reg[IFR] &= ~T2_INT;
		UPDATE_IRQ;
	case T2CH:	/* fall through */
		via_timer_update(0);
		ret = via.reg[reg ];
		break;
	case SR:
		via.reg[IFR] &=
			~(SR_INT | SR_DATA_INT | SR_CLOCK_INT);
		UPDATE_IRQ;
		cuda_sr_idle();
		break;
	case IFR:
		ret &= 0x7f;
		if(ret & via.reg[IER])
			ret |= IRQ_LINE;
		break;
	case IER:
		ret |= 0x80;
		break;
	}
	
	V_UNLOCK;
	
	return ret;
}

static void
via_io_write( ulong addr, ulong data, int len, void *x )
{
	ulong base = get_mbase( via.reg_range );
	int reg;
	
	V_LOCK;
	
	reg = (addr - base) / VIA_REG_LEN;
	
	switch(reg) {
	case B:
		via.reg[reg] =
			(data & via.reg[DIRB]) |
			(via.reg[reg] & ~via.reg[DIRB]);
		cuda_handshake(via.reg[B] & TACK, via.reg[B] & TIP);
		break;
	case T1CL:
		via.reg[reg] = data;
		via.t1_stamp = get_mticks_();
		break;
	case T1CH:
		via.reg[T1CL] = 0xff;
		via.reg[reg] = data;
		via.t1_stamp = get_mticks_();
		via_timer_start(1);
		break;
	case T2CL:
		via.reg[reg] = data;
		via.t2_stamp = get_mticks_();
		break;
	case T2CH:
		via.reg[T2CL] = 0xff;
		via.reg[reg] = data;
		via.t2_stamp = get_mticks_();
		via_timer_start(0);
		break;
	case T1LL:
		/* fallthrough */
	case T1LH:	/* XXX: vMac clears interrupt here... should we? */
		via.reg[reg] = data;
		break;
	case SR:
		via.reg[reg] = data;
		via.reg[IFR] &= ~(SR_INT | SR_DATA_INT | SR_CLOCK_INT);
		UPDATE_IRQ;
		break;
	case ACR:
		via.reg[reg] = data;
		if(data & T1MODE_CONT) {
			via_timer_update(1);
			via_timer_start(1);
		}
		break;
	case IFR:
		via.reg[reg] &= ~data;
		UPDATE_IRQ;
		break;
	case IER:
		if(data & IER_SET)
			via.reg[reg] |= data & 0x7f;
		else
			via.reg[reg] &= ~data;
		break;
	default:
		via.reg[reg] = data;
	}
	
	V_UNLOCK;
}


static void
via_timer_update( int is_timer1 )
{
	ulong diff;
	ullong now, mdiff;

	now = get_mticks_();
	if( is_timer1 ) {
		mdiff = now - via.t1_stamp;
		via.t1_stamp = now;
	} else {
		mdiff = now - via.t2_stamp;
		via.t2_stamp = now;
	}

	diff = mticks_to_usecs( mdiff );	
	diff = (diff * VIA_TICKS_TO_USECS_DEN) / VIA_TICKS_TO_USECS_NUM;
	
	if( is_timer1 ) {
		diff = ((via.reg[T1CH] << 8) |
			via.reg[T1CL]) - diff;
		via.reg[T1CH] = (diff >> 8) & 0xff;
		via.reg[T1CL] = diff & 0xff;
	} else {
		diff = ((via.reg[T2CH] << 8) |
			via.reg[T2CL]) - diff;
		via.reg[T2CH] = (diff >> 8) & 0xff;
		via.reg[T2CL] = diff & 0xff;
	}
}

static void
via_timer_start( int is_timer1 )
{
	ulong val, usecs;
	int *timer_id;
	
	if( is_timer1 ) {
		val = (via.reg[T1CH] << 8) | (via.reg[T1CL]);
		timer_id = &via.t1_id;
	} else {
		val = (via.reg[T2CH] << 8) | (via.reg[T2CL]);
		timer_id = &via.t2_id;
	}
	
	if( *timer_id )
		cancel_timer( *timer_id );
	
	usecs = (val * VIA_TICKS_TO_USECS_NUM) / VIA_TICKS_TO_USECS_DEN;
	
	*timer_id = usec_timer( usecs, via_expire_timer, NULL );
}

static void
via_expire_timer( int id, void *dummy, int latency )
{
	V_LOCK;
	
	if( id != via.t1_id && id != via.t2_id ) 
		return;
	
	if( id == via.t1_id ) {
		via.t1_id = 0;
		via.reg[T1CL] = via.reg[T1LL];
		via.reg[T1CH] = via.reg[T1LH];
		via.t1_stamp = get_mticks_();
		
		if( via.reg[ACR] & T1MODE_CONT )
			via_timer_start(1);
		
		via.reg[IFR] |= T1_INT;
		/* DUMP_FREQ("T1"); */
	} else {
		via.t2_id = 0;
		via.reg[T2CL] = via.reg[T2CH] = 0xff;
		via.t2_stamp = get_mticks_();
		
		via.reg[IFR] |= T2_INT;
		/* DUMP_FREQ("T2");*/
	}
	
	UPDATE_IRQ;
	V_UNLOCK;
}

static char
via_shift_out(void)
{
	via_sr_int_post(0);
	return via.reg[SR];
}

static void
via_shift_in(char c)
{
	via.reg[SR] = c;
	via_sr_int_post(1);
}

static void
via_dummy_shift( void )
{
	via.reg[SR] = 0;
	via_sr_int_post(0);
}

static void
via_sr_int_post( int set_sr_data_flag )
{
	if( via.sr_active )
		return;
	
	via.sr_active = 1;
	
	usec_timer( via.hack_sr_shift_delay, via_sr_int,
		    set_sr_data_flag ? (void *) -1 : 0L );
}

static void
via_sr_int( int id, void *data_flag, int latency)
{
	V_LOCK;
	
	if( data_flag )
		via.reg[IFR] |= SR_DATA_INT;
	
	via.reg[IFR] |= SR_INT | SR_CLOCK_INT;
	via.sr_active = 0;
	
	UPDATE_IRQ;
	V_UNLOCK;
}


/************************************************************************/
/*	CUDA Implementation						*/
/************************************************************************/

static void
cuda_handshake( int tack, int tip )
{
	char c;
	
	C_LOCK;
	
	tack = tack ? 1 : 0;
	tip = tip ? 1 : 0;

	if(tack == cuda.tack && tip == cuda.tip) {
		C_UNLOCK;
		return;
	}
	
	cuda.tack = tack;
	cuda.tip = tip;
	
	/* syncinc/starting reply if TIP is high*/
	if( tip ) {
		cuda.fill_out_buffer = 0;
#if 0
		if( !cuda.treq )
			LOG("Collision\n"); 
#endif
		if( cuda.in_count )
			cuda_incoming();
		
		cuda.out_count = cuda.in_count = 0;
		
		/* Perhaps all messages should be flushed if ~tack && tip.
		 * Currently, they are only _delayed_.
		 */
		if( !tack )
			cuda.out_req_count = 0;
		
		cuda.treq = tack;

		if( tack && cuda_msg_avail() )
			usec_timer( cuda.hack_treq_delay, cuda_start_reply, NULL);
		else
			via_dummy_shift();		
		C_UNLOCK;
		return;
	}

	
	if( cuda.treq ) {
		/* To CUDA */
		c = via_shift_out();
		if(cuda.in_count < VIA_BUFFER_LEN)
			cuda.in_buffer[cuda.in_count++] = c;
	} else {
		/* From CUDA */
		if( cuda.fill_out_buffer ) {
			cuda.fill_out_buffer = 0;
			if( !cuda_msg_avail() ) {
				if(cuda.onesec_enabled && cuda.onesec_pending) {
					cuda.onesec_pending--;
					via_cuda_reply(1,TIMER_PACKET);
				} else {
					cuda_poll();
				}
			}
			if( !cuda_msg_avail() ) {
				LOG("Message supposed to be available\n");
				/* this currently leads to an enless loop... best to exit */
 				C_UNLOCK;
				exit(1);
			}
		}
		
		via_shift_in( cuda.out_buffer[cuda.out_count++] );
		if( cuda.out_count >= cuda.out_req_count ) {
			cuda.out_count = cuda.out_req_count = 0;
			cuda.treq = 1;
		}
	}
	
	C_UNLOCK;
}	

static void
cuda_start_reply( int id, void *usr, int latency )
{
	C_LOCK;
	if( cuda.out_req_count && cuda.treq && cuda.tip ) {
		cuda.treq = 0;
		via_dummy_shift();
	}
	C_UNLOCK;
}


static void
cuda_sr_idle( void )
{
	C_LOCK;

	if( cuda.treq && cuda.tip && !cuda.out_req_count &&
	    (cuda.autopoll /*|| cuda.onesec_enabled*/) ) 
	{
		if( adb_mes_avail() ) {
			cuda.fill_out_buffer = 1;
			cuda.treq = 0;
			via_dummy_shift();
		}
	}
	
	C_UNLOCK;
}

static int
cuda_msg_avail( void )
{
	return cuda.out_req_count ? 1 : 0;
}

void
adb_service_request( void )
{
	C_LOCK;
	if( (cuda.autopoll || cuda.onesec_enabled) && cuda.tip &&
	    cuda.tack && !cuda.out_req_count )
	{
		cuda.fill_out_buffer = 1;
		cuda.treq = 0;
		via_dummy_shift();
	}
	C_UNLOCK;
}

static void
cuda_incoming( void )
{
	cuda.out_count = 0;
	
	if( cuda.in_count < 2 )
		return;
	
	via_cuda_reply( 3,0x2,0x2,cuda.in_buffer[0] );
	
	switch( cuda.in_buffer[0] ) {
	case ADB_PACKET:
		adb_packet( cuda.in_buffer[1], (unsigned char *) (cuda.in_buffer+2),
			   cuda.in_count-2 );
		break;
	case CUDA_PACKET:
		cuda_packet( cuda.in_buffer[1], cuda.in_buffer+2,
				cuda.in_count-2 );
		break;
	default:
		LOG("Unimplemented packet type %d received\n", cuda.in_buffer[1] );
		break;
	}
	
	cuda.in_count = 0;
}

static void
cuda_poll( void )
{
	if( !cuda.out_req_count && cuda.autopoll )
		adb_poll();
}

void
via_cuda_set_reply_buffer( char *buf, int len )
{
	if( len > VIA_BUFFER_LEN || cuda.out_count ) {
		LOG("via_cuda_set_reply_buffer error\n");
		len = VIA_BUFFER_LEN;
	}
	
	cuda.out_req_count = len;
	memmove( cuda.out_buffer,buf,len );
}

void
via_cuda_reply(int len, ...)
{
	va_list	args;
	int	i;
	
	va_start( args, len );
	
	if( len > VIA_BUFFER_LEN || cuda.out_count ) {
		LOG("via_cuda_reply: error\n");		
		len = VIA_BUFFER_LEN;
	}
	
	cuda.out_req_count = len;
	
	for(i = 0; i < len; i++)
		cuda.out_buffer[i] = va_arg(args,int);
	
	va_end( args );
}

static void
cuda_packet( int cmd, char *data, int len )
{
	char reply_buf[8], *reply;
	int slen;

	/*
	 * Reply packet: CUDA_PACKET, 0, CMD,  *MES*
	 * Error packet: ERROR_PACKET, reason, CUDA_PACKET, CMD
	 *
       	 * Reason:
	 *	too long (?)		3
	 *	bad parameters (?)	5
	 *	nonexistant cmd		2
	 */
	if( via_cuda_verbose ) {
		int	i;
		
		printm("CUDA command %02X %s ",cmd,len ? "[" : "");
		for(i = 0; i < len; i++) printm("%02X ",data[i]);
		printm("%s\n",len ? "]" : "");
	}
	
	reply = reply_buf;
	*reply++ = CUDA_PACKET;
	*reply++ = 0;
	*reply++ = cmd;
	slen = 0;
	
	switch( cmd ) {
	case CUDA_WARM_START:
	case CUDA_RESET_SYSTEM:
	case CUDA_MS_RESET:
	case CUDA_POWERDOWN:
		usec_timer( 5000, cuda_power_down, NULL );
		break;
	case CUDA_POWERUP_TIME:
		break;
	case CUDA_AUTOPOLL:
		cuda.autopoll = *data ? 1 : 0;
		break;
	case CUDA_GET_TIME:
		slen = 4;
		*(long *)reply = cuda.rtc + time(NULL);
		break;
	case CUDA_SET_TIME:
		cuda.rtc_stamp = time( NULL );
		cuda.rtc = *(long *)data;
		break;
	case CUDA_GET_PRAM:
	case CUDA_SET_PRAM:
	case CUDA_GET_6805_ADDR:
	case CUDA_SET_6805_ADDR:
	case CUDA_SEND_DFAC:
	case CUDA_SET_IPL_LEVEL:
	case CUDA_SET_AUTO_RATE:
		break;
	case CUDA_GET_AUTO_RATE:
		slen = 1;
		*reply = 0xb;
		break;
	case CUDA_SET_DEVICE_LIST:
		break;
	case CUDA_GET_DEVICE_LIST:
		slen = 2;
		*reply++ = 0;
		*reply = 0;
		break;
	case CUDA_GET_SET_IIC:
		/* 1 or 3 data bytes. In the later case the bytes have the
		 * meaning iicAddr, iicReg, iicData (probably a SET)
		 */
		if(len != 3) {
			via_cuda_reply(4,ERROR_PACKET,0x2,CUDA_PACKET,cmd);
			return;
		}
		break;
	case CUDA_CMD_1C:
		reply = reply_buf;
		*reply++ = 2;
		*reply++ = 2;
		*reply++ = 1;
		*reply = cmd;
		slen = 1;
		break;
	case CUDA_TIMER_TICKLE:
		/* I'm not sure what's this used for. Is it a confirmation of the
		   1 sec timer ? Should we suspend the 1 sec timer while not receiving
		   this ? */
		/* LOG("Cuda: CUDA_TIMER_TICKLE data (%d): %02x\n", len, data[0]);*/
		break;
	case CUDA_SET_POWER_MESSAGES:
	case CUDA_ENABLE_DISABLE_WAKEUP:
	case CUDA_CMD_26:
		break;
	case CUDA_SET_ONE_SECOND_MODE:
		if( data[0] ) {
			cuda.onesec_enabled = 1;
			cuda.onesec_last = 0;
			usec_timer( 1000000, cuda_one_sec_timer, NULL );
		} else
			cuda.onesec_enabled = 0;
		break;
	case CUDA_COMBINED_FORMAT_IIC:
		/* this reply is important on B&W G3 */
		via_cuda_reply( 4,ERROR_PACKET,0x5,CUDA_PACKET, cmd );
		return;
	default:
		via_cuda_reply( 4,ERROR_PACKET,0x2,CUDA_PACKET,cmd);
		return;
	}
	
	slen += 3;
#if 0
	printm("CUDA Reply: ");
	for( i=0; i<slen; i++ )
		printm("%02X ", reply_buf[i] );
	printm("\n");
#endif	
	via_cuda_set_reply_buffer(reply_buf,slen);
}

static void
cuda_power_down( int id, void *dummy, int latency )
{
	exit(0);
}

static void
cuda_one_sec_timer( int id, void *dummy, int latency )
{
	struct timeval tv;
	long count;
	
	C_LOCK;
	
	if( !cuda.onesec_enabled ) {
		C_UNLOCK;
		return;
	}
	
	/* calibrate using gettimeofday... */
	gettimeofday( &tv, NULL );
	if( !cuda.onesec_last )
		cuda.onesec_last = tv.tv_sec;
	
	cuda.onesec_last++;
	count = (cuda.onesec_last - tv.tv_sec) * 1000000;
	count -= tv.tv_usec;
	if( count <= 0 )
		count = 1;
	
	cuda.onesec_pending++;
	
	C_UNLOCK;
	
	usec_timer( count, cuda_one_sec_timer, NULL );
	adb_service_request();
}

int
cuda_autopolls( void )
{
	return cuda.autopoll;
}


/************************************************************************/
/*	VIA debug							*/
/************************************************************************/

static void print_reg( int reg, int val );

static void
via_io_print( int is_read, ulong addr, ulong data, int len, void *usr )
{
	static int save_b=-1;
	int reg = (addr - get_mbase(via.reg_range))/VIA_REG_LEN;

	/* Suppress polling of B */
	if( reg == B && data == save_b )
		return;
	save_b = (reg==B) ? data : -1;

	printm("via: %08lX %s ", addr, is_read ? "read ": "write" );
	print_reg( reg, data );
}

static void
print_reg( int reg, int val )
{
	static int oldb=0;
	int tog;

	printm("%4s %02X  ", reg_names[reg], val );

	switch( reg ){
	case B:
		printm("%s%s%s",
		       !(val & TIP)? "~TIP " : "",
		       !(val & TACK)? "~TACK " : "",
		       !(val & TREQ)? "~TREQ " : "" );
		if( val & ~(TACK|TREQ|TIP) )
			printm("**MORE** ");
		tog = (oldb ^ val) & (TACK|TREQ|TIP);
		oldb = val;
		if( !tog )
			break;
		printm("  Toggled: %s%s%s",
		       (tog & TACK)? "TACK " : "",
		       (tog & TREQ)? "TREQ " : "",
		       (tog & TIP)? "TIP " : "");
		break;
	case DIRB:
		printm("TACK[%s] TREQ[%s] TIP[%s] ",
		       (val & TACK)? "out" : "in",
		       (val & TREQ)? "out" : "in",
		       (val & TIP)? "out" : "in" );

		if( val & ~(TACK|TREQ|TIP) ) {
			/* stop_emulation(); */
			printm("**MORE** ");
		}
		break;
	case ACR:
		printm("%s%s",
		       (val & SR_EXT) ? "SR_EXT " : "",
		       (val & SR_OUT) ? "SR_OUT[to cuda] " : "~SR_OUT[from cuda] ");
		if( val & ~(SR_OUT|SR_EXT) )
			printm("**MORE** ");
		break;
	}
	printm("\n");
}



/************************************************************************/
/*	Debugger CMDs							*/
/************************************************************************/

static int __dcmd
cmd_viaregs( int argc, char **argv )
{
	int i, val;

	printm("VIA registers:\n");
	for(i=0; i<VIA_REG_COUNT; i++ ) {
		printm("   ");
		val = via.reg[i];
		if( i==B )
			val = (val & ~TREQ) | (cuda.treq ? TREQ : 0 );
		print_reg( i, val );
	}
	printm( "SR is %s  ;  ", via.sr_active ? "*active*" : "idle" );
	printm( "Timer_1 is %s  ;  ", via.t1_id ? "*active*" : "idle" );
	printm( "Timer_2 is %s.\n ", via.t1_id ? "*active*" : "idle" );
	return 0;
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static io_ops_t via_ops = {
	.read	= via_io_read,
	.write	= via_io_write,
	.print	= via_io_print
};

static int
prepare_save( void )
{
	/* cuda must be idle */
	return via.sr_active || !( cuda.treq && cuda.tip && !cuda.out_req_count );
}

static int
save_viacuda_regs( void ) 
{
	return write_session_data( "VIA", 0, (char*)&via, sizeof(via) )
	    || write_session_data( "CUDA", 0, (char*)&cuda, sizeof(cuda) );
}

static int
via_cuda_init(void)
{
	mol_device_node_t *dn;
	gc_bars_t bars;
	irq_info_t irqinfo;
	int flags = 0;

	via_cuda_verbose = get_bool_res("via_verbose") == 1;
	if( !(dn=prom_find_devices("via-cuda")) || !is_gc_child(dn, &bars) )
		return 0;
	if( bars.nbars != 1 || prom_irq_lookup(dn, &irqinfo) != 1 )
		return 0;

	/* MacOS expect hardware latency - not pretty */
	if( is_newworld_boot() || is_oldworld_boot() ) {
		cuda.hack_treq_delay = CUDA_TREQ_DELAY;
		via.hack_sr_shift_delay = VIA_SR_SHIFT_DELAY;
	}

	session_save_proc( save_viacuda_regs, prepare_save, kDynamicChunk );
	if( loading_session() ) {
		int err = read_session_data( "VIA", 0, (char*)&via, sizeof(via) ) ||
			  read_session_data( "CUDA", 0, (char*)&cuda, sizeof(cuda) );
		if( err )
			session_failure("Could not load via-cuda\n");
	} else {
		via.irq = irqinfo.irq[0];
	
		via.t1_id = via.t2_id = 0;

		cuda.rtc_stamp = time(NULL);
		cuda.rtc = cuda.rtc_stamp + CUDA_RTC_OFFSET;
	
		cuda.onesec_enabled = cuda.onesec_pending = 0;
	}

	pthread_mutex_init( &via.lock, NULL );
	pthread_mutex_init( &cuda.lock, NULL );

	via.reg_range = add_gc_range( bars.offs[0], (VIA_REG_COUNT*VIA_REG_LEN),
				      "via-cuda", flags, &via_ops, NULL );

	add_cmd("viaregs", "viaregs \nDump VIA registers\n", -1, cmd_viaregs );	
	adb_init();
	return 1;
}

static void
via_cuda_cleanup( void )
{
	adb_cleanup();

	pthread_mutex_destroy(&via.lock);
	pthread_mutex_destroy(&cuda.lock);
}

driver_interface_t via_cuda_driver = {
    "via-cuda", via_cuda_init, via_cuda_cleanup
};
