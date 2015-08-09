/* 
 *   Creation Date: <2000/05/28 02:37:51 samuel>
 *   Time-stamp: <2004/06/05 20:01:20 samuel>
 *   
 *	<sound.c>
 *	
 *	Sound Driver
 *   
 *   Copyright (C) 2000-2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/resource.h>
#include <sys/param.h>
#include "driver_mgr.h"
#include "os_interface.h"
#include "memory.h"
#include "thread.h"
#include "res_manager.h"
#include "osi_driver.h"
#include "pic.h"
#include "booter.h"
#include "pci.h"
#include "timer.h"
#include "mac_registers.h"
#include "sound-iface.h"

static struct {
	pthread_mutex_t lock;
	int		irq;
	int		started;

	/* misc */
	int		rate_limit;	/* user imposed rate restriction */
	volatile int	thread_running;	/* 2=restarting, 1=running, 0=dead, -1=exiting */

	/* volume */
	int		mute;
	int		vol_updated;
	int		lvol, rvol;	/* PCM volume (0-255) */

	/* current mode */
	int		format;
	uint		rate;		/* in Hz */
	uint		bpf;		/* bytes per frame */

	/* sound parameters */
	int		fragsize;	/* client fragsize */

	/* output driver */
	sound_ops_t	*ops;

	/* double buffering */
	int		dbufsize;	/* double buffer size */
	char		*dbuf_src;	/* next input buffer */
	int		dbuf_srcsize;	/* size of input buffer */
	volatile int	dbuf_go;	/* generate irqs */

	/* ring buffer */
	char		*ringbuf;	/* ringbuf (if non-NULL) */
	int		ringfrags;	/* #frags in ringbuf */
	timestamp_t	timestamp;
	volatile uint	ringind;	/* last frag passed to hardware */

	/* misc */
	char		*startboingbuf;	/* too be freed */

	/* debugging */
	/* int		debug_tbl; */
} ss;

#define LOCK		pthread_mutex_lock( &ss.lock )
#define UNLOCK		pthread_mutex_unlock( &ss.lock )


/************************************************************************/
/*	no-sound driver support						*/
/************************************************************************/

static struct {
	sound_ops_t 	*old_ops;
	int		usecs_in_pipe;		/* usecs in pipe (from mark) */
	uint		tb_mark;
} nosound;

static sound_ops_t	nosound_driver_ops;

static int
nosound_query( int format, int rate, int *fragsize )
{
	if( (rate != 44100 && rate != 22050) || 
	    !(format & (kSoundFormat_S16_BE | kSoundFormat_U8)) )
		return -1;
	*fragsize = ss.fragsize ? ss.fragsize : 4096;
	return 0;
}

static void
nosound_write( char *dummybuf, int size )
{
	int usecs_res;
	uint mark;

	struct timespec tv;
	tv.tv_sec = 0;
	tv.tv_nsec = 1000;
	
	if( !size )
		return;

	nosound.usecs_in_pipe += 1000000/ss.rate * size/ss.bpf;
	for( ;; ) {
		mark = get_tbl();
		usecs_res = nosound.usecs_in_pipe - mticks_to_usecs(mark - nosound.tb_mark);
		if( usecs_res <= 0 )
			break;
		nanosleep(&tv, NULL);
	}
	nosound.usecs_in_pipe = usecs_res;
	nosound.tb_mark = mark;
}

static int
nosound_open( int format, int rate, int *fragsize, int ringmode )
{	
	if( nosound_query(format, rate, fragsize) )
		return -1;

	nosound.old_ops = ss.ops;
	ss.ops = &nosound_driver_ops;

	nosound.tb_mark = get_tbl();
	nosound.usecs_in_pipe = 0;
	return 0;
}

static void
nosound_close( void ) 
{
	ss.ops = nosound.old_ops;
}

static sound_ops_t nosound_driver_ops = {
	.cleanup	= NULL,
	.query		= nosound_query,
	.open		= nosound_open,
	.close 		= nosound_close,
	.write		= nosound_write,
	.volume		= NULL,
};


/************************************************************************/
/*	Sound Engine							*/
/************************************************************************/

uint
sound_calc_bpf( int format )
{
	uint bpf = (format & kSoundFormat_Len1Mask) ? 1 : 2;
	bpf *= (format & kSoundFormat_Stereo) ? 2 : 1;

	return bpf;
}

static void
ring_engine( void )
{
	char *p;
	DEBUG_SND("Ring Engine: Started\n");
	
	while( ss.thread_running > 0 ) {
		p = ss.ringbuf + ss.ringind * ss.fragsize;

		if( ss.vol_updated && ss.ops->volume ) {
			ss.vol_updated = 0;
			(*ss.ops->volume)( ss.lvol, ss.rvol );
		}
		UNLOCK;
		(*ss.ops->write)( p, ss.fragsize );
		LOCK;

		/* handle quick start-stop-start cycles */
		if( ss.thread_running > 1 ) {
			ss.thread_running = 1;
			continue;
		} else if( ss.thread_running <= 0 )
			break;

		if( ++ss.ringind == ss.ringfrags ) {
			ss.ringind = 0;
			get_timestamp( &ss.timestamp );
			irq_line_hi( ss.irq );
		} else {
			abort_doze();
		}
	}
	DEBUG_SND("Ring Engine: Stopped\n");
}


static void
dbuf_engine( void )
{
	char *dbuf = malloc( ss.dbufsize );
	int dbuf_cnt = 0;

	for( ;; ) {
		/* double buffering */
		if( dbuf_cnt ) {
			if( ss.vol_updated && ss.ops->volume ) {
				ss.vol_updated = 0;
				(*ss.ops->volume)( ss.lvol, ss.rvol );
			}
			UNLOCK;
			(*ss.ops->write)( dbuf, dbuf_cnt );
			dbuf_cnt = 0;
			LOCK;
		}

		/* switch doublebuffer */
		if( !ss.dbuf_srcsize ) {
			if( ss.thread_running < 0 )
				break;
			/* buffer underrun, drop frame */
			DEBUG_SND("Sound Double Buffer: Sound frame dropped\n");
			memset( dbuf, 0, ss.dbufsize );
			dbuf_cnt = ss.dbufsize;
		} else {
			int size = MIN(ss.dbuf_srcsize, ss.dbufsize);
			memcpy( dbuf, ss.dbuf_src, size );
			dbuf_cnt = size;
			ss.dbuf_src += size;
			ss.dbuf_srcsize -= size;
		}
	
		if( ss.dbuf_go ) {
			ss.dbuf_go = 0;
			irq_line_hi( ss.irq );
		}
	}
	free( dbuf );
}


static void
audio_thread( void *dummy )
{
	LOCK;
	if( ss.ringbuf )
		ring_engine();
	else
		dbuf_engine();

	if( ss.ops->flush )
		(*ss.ops->flush)();

	(*ss.ops->close)();
	ss.thread_running = 0;

	if( ss.startboingbuf ) {
		free( ss.startboingbuf );
		ss.startboingbuf = NULL;
	}
	UNLOCK;
}


/************************************************************************/
/*	generic routines						*/
/************************************************************************/

static int
wait_sound_stopped( void )
{
	if( ss.started )
		return -1;

	/* wait untill thread has exited */
	while( ss.thread_running < 0 )
		sched_yield();
	return 0;
}

static int
query( int format, int rate )
{
	int dummy_fragsize;
	
	if( ss.started )
		return 1;
	if( ss.rate_limit && rate > ss.rate_limit )
		return -1;
	return (*ss.ops->query)( format, rate, &dummy_fragsize );
}

static int
set_mode( int format, int rate )
{
	int ret, size, fragsize, bufsize;
	static int lastformat=-2, lastrate;

	/* optimize away the evil wait_sound_stopped() */
	if( lastformat == format && lastrate == rate )
		return 0;
	
	if( wait_sound_stopped() )
		return 1;
	if( (ret=(*ss.ops->query)(format, rate, &fragsize)) < 0 )
		return ret;

	lastformat = format;
	lastrate = rate;

	ss.rate = rate;
	ss.bpf = sound_calc_bpf( format );
	ss.fragsize = fragsize;
	ss.format = format;

	/* set doublebuffer size (must be a fragsize multiple) */
	if( (size=get_numeric_res("sound.bufsize")) == -1 )
		size = fragsize * 2;
	for( bufsize=fragsize*2; bufsize < size ; bufsize += fragsize )
		;
	ss.dbufsize = bufsize;

	DEBUG_SND("SoundMode: %x, %d Hz, %d/%d\n", format, rate, fragsize, bufsize );
	return 0;
}

static void
stop_sound( void )
{
	if( !ss.started )
		return;
	ss.started = 0;

	LOCK;
	ss.thread_running = -1;
	irq_line_low( ss.irq );
	UNLOCK;

	/* sound is stopped asynchronously */
}

static int
start_sound( void ) 
{
	if( ss.started || !ss.format )
		return -1;
	ss.started = 1;

	LOCK;
	ss.ringind = 0;
	ss.dbuf_go = 0;

	if( ss.thread_running ) {
		/* jump start already running thread */
		ss.thread_running = 2;
	} else {
		int ringmode = ss.ringbuf ? 1:0;
		if( (*ss.ops->open)(ss.format, ss.rate, &ss.fragsize, ringmode) ) {
			printm("audiodevice unavailable\n");
			nosound_open( ss.format, ss.rate, &ss.fragsize, ringmode );
		}
		ss.thread_running = 1;
		create_thread( (void*)audio_thread, NULL, "audio-thread" );
	}

	UNLOCK;
	return 0;
}

static void
resume_sound( void )
{
	if( ss.started )
		ss.dbuf_go = 1;
}

static void
pause_sound( void )
{
	if( ss.started )
		ss.dbuf_go = 0;
}


/************************************************************************/
/*	OSI interface							*/
/************************************************************************/

static int
osip_sound_irq_ack( int sel, int *params )
{
	mregs->gpr[4] = ss.timestamp.hi;
	mregs->gpr[5] = ss.timestamp.lo;
	return irq_line_low( ss.irq );
}

static int
osip_sound_cntl( int sel, int *params )
{
	switch( params[0] ) {
	/* ---- Version Checking (new API) ---- */
	case kSoundCheckAPI:
		if( params[1] != OSI_SOUND_API_VERSION ) {
			printm("Incompatible MOLAudio version %d!\n", params[1] );
			return -1;
		}
		break;
	case kSoundOSXDriverVersion:
		set_driver_ok_flag( kOSXAudioDriverOK, params[1] == OSX_SOUND_DRIVER_VERSION );
		break;

	/* ---- New API (OSX) ---- */
	case kSoundSetupRing: /* bugfize, buf */
		ss.ringfrags = params[1] / ss.fragsize;
		ss.ringbuf = params[2] ? transl_mphys(params[2]) : NULL;
		break;
	case kSoundGetRingSample: 
		return ss.ringind * ss.fragsize/ss.bpf;

	case kSoundGetSampleOffset:
		/* no-touch zone, in frames */
		return 0;
	case kSoundGetSampleLatency:
		/* internal buffering, in frames */
		return 2048;

	/* ---- Both new and old API ---- */
	case kSoundQueryFormat: /* format, rate */
		return query( params[1], params[2] );
	case kSoundSetFormat: /* format, rate */
		return set_mode( params[1], params[2] );
	case kSoundStart:
		return start_sound();
	case kSoundStop:
		stop_sound();
		break;
	case kSoundFlush:
		/* no-op */
		break;

	/* ---- Old API (classic) ---- */
	case kSoundClassicDriverVersion:
		if( params[1] < CLASSIC_SOUND_DRIVER_VERSION ) {
			printm("Install the latest MOLAudio extension!\n");
			return -1;
		}
		break;
	case kSoundResume:
		resume_sound();
		break;
	case kSoundPause:
		pause_sound();
		break;
	case kSoundGetBufsize: /* return doublebuf size */
		/* printm("kSoundGetFragsize: %d (hw %d)\n", ss.dbufsize, ss.fragsize ); */
		return ss.dbufsize;

	case kSoundRouteIRQ:
		oldworld_route_irq( params[1], &ss.irq, "sound" );
		break;
	default:
		return -1;
	}
	return 0;
}

/* Params:
 * phys_buffer - Physical Buffer Address (cast as int)
 * len - Length of Buffer
 * restart - Restart the engine? (not used)
 */
static int
osip_sound_write( int sel, int *params )
{
	char *buf = transl_mphys( params[0] );
	int len = params[1];
	
	if( !params[0] || !buf )
		return -1;

	if( ss.dbuf_srcsize )
		printm("warning: ss.dbuf_srcsize != 0\n");

	ss.dbuf_src = buf;
	ss.dbuf_srcsize = len;
	ss.dbuf_go = 1;

	return 0;
}

/* Params:
 * hwVolume: Volume setting
 * speakerVolume: Unused
 * hwMute: Mute boolean
 */
static int
osip_volume( int sel, int *params )
{
	uint lvol = (params[0] & 0xffff0000) >> 16;
	uint rvol = params[0] & 0xffff;

	/* speaker volume is ignored for now */
	if( lvol > 255 )
		lvol = 255;
	if( rvol > 255 )
		rvol = 255;

	LOCK;
	ss.lvol = lvol;
	ss.rvol = rvol;
	ss.mute = params[2] ? 1:0;
	ss.vol_updated = 1;
	UNLOCK;
	return 0;
}

/* Plays the initial startup sound */
static void
startboing( void )
{
	char *name = get_filename_res("startboing.file");
	int len, fd;
	char *buf;
	
	if( !name )
		return;
	if( (fd=open(name, O_RDONLY)) < 0 ) {
		printm("Could not open '%s'", name);
		return;
	}
	len = lseek( fd, 0, SEEK_END );
	lseek( fd, 0, SEEK_SET );

	if( (buf=malloc(len)) ) {
		if( read( fd, buf, len) != len ) {
			printm("Could not read from '%s'", name);
			free(buf);
			close(fd);
			return;
		}
			
		ss.dbuf_src = buf;
		ss.dbuf_srcsize = len;
		ss.dbuf_go = 1;
		ss.startboingbuf = buf;
	}
	close( fd );
	free(buf);

	set_mode( kSoundFormat_S16_BE | kSoundFormat_Stereo, 22050 );
	start_sound();
	stop_sound();
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static osi_iface_t osi_iface[] = {
	{ OSI_SOUND_CNTL,	osip_sound_cntl		},
	{ OSI_SOUND_IRQ_ACK,	osip_sound_irq_ack	},
	{ OSI_SOUND_WRITE,	osip_sound_write	},
	{ OSI_SOUND_SET_VOLUME,	osip_volume 		},
};

static int 
osi_sound_init( void )
{
	static pci_dev_info_t pci_config = { 0x6666, 0x6668, 0x02, 0x0, 0x0000 };
	char *drv = is_oldworld_boot() ? "sound" : NULL;
	int i, limit;
	
	memset( &ss, 0, sizeof(ss) );

	if( !register_osi_driver(drv, "mol-audio", is_classic_boot()? &pci_config : NULL, &ss.irq) ) {
		printm("Failed to register sound card\n");
		return 0;
	}
	pthread_mutex_init( &ss.lock, NULL );

	/* handle user rate limits */
	if( (limit=get_numeric_res("sound.maxrate")) > 0 )
		ss.rate_limit = limit;

	/* select output driver */
	for( i=0 ;; i++ ) {
		char *s = get_str_res_ind("sound.driver", 0, i );
		if( i && !s )
			break;
		if( s && !strcasecmp("any", s) )
			s = NULL;
		if( (!s || !strcasecmp("sdl_sound", s)) && (ss.ops=sdl_sound_probe(s?1:0)) )
			break;
		if( (!s || !strcasecmp("alsa", s)) && (ss.ops=alsa_probe(s?1:0)) )
			break;
		if( (!s || !strcasecmp("oss", s) || !strcasecmp("dsp", s)) && (ss.ops=oss_probe(s?1:0)) )
			break;
		if( !s || !strcasecmp("nosound", s) ) {
			ss.ops = &nosound_driver_ops;
			break;
		}
	}
	if( !ss.ops )
		ss.ops = &nosound_driver_ops;
	else {
		if( get_bool_res("startboing.enabled") == 1 )
			startboing();
	}

	/* the osi procs must be registered even if sound is unavailable */
	register_osi_iface( osi_iface, sizeof(osi_iface) );
	return 1;
}

static void
osi_sound_cleanup( void )
{
	stop_sound();
	wait_sound_stopped();
	if( ss.ops->cleanup )
		(*ss.ops->cleanup)();
}

driver_interface_t osi_sound_driver = {
    "osi_sound", osi_sound_init, osi_sound_cleanup
};
