/* 
 *   Creation Date: <2004/06/05 18:12:24 samuel>
 *   Time-stamp: <2004/06/05 20:11:07 samuel>
 *   
 *	<oss.c>
 *	
 *	
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */

#include "mol_config.h"
#include <sys/ioctl.h>
#include <sys/soundcard.h>
#include "res_manager.h"
#include "booter.h"
#include "poll_compat.h"
#include "sound-iface.h"

struct { 
	int 		fd; 
} oss;


static int
oss_convert_format( int format, int *fmt, int *channels )
{
	static struct {
		int mol_fmt, linux_fmt;
	} fmt_table[] = {
		{ kSoundFormat_U8,	AFMT_U8,	},
		{ kSoundFormat_S16_BE,	AFMT_S16_BE,	},
		{ kSoundFormat_S16_LE,	AFMT_S16_LE,	},
	};
	int f, i;	

	for( f=-1, i=0; i<sizeof(fmt_table)/sizeof(fmt_table[0]); i++ )
		if( format & fmt_table[i].mol_fmt )
			f = fmt_table[i].linux_fmt;
	if( f ==  -1 )
		return -1;

	*fmt = f;
	*channels = (format & kSoundFormat_Stereo)? 2:1;
	return 0;
}

static void
oss_close( void )
{
	close( oss.fd );
}

static const char *
oss_devname( void ) 
{
	char *s;
	if( !(s=get_filename_res("oss.device")) )
		s = "/dev/dsp";
	return s;
}

static int
oss_open( int format, int rate, int *fragsize, int ringmode )
{
	int nbuf, size, err, fmt, channels, rate_, fmt_, channels_;
	uint frag;
	
	/* printm("OSS Open: %d [%d]\n", rate, format ); */
	
	nbuf = get_numeric_res("oss.nbufs");
	size = get_numeric_res("oss.fragsize");

	if( nbuf < 2 || nbuf > 16 )
		nbuf = 4;
	if( size <= 0 || size > 8192 )
		size = is_classic_boot() ? 4096 : 256;
	if( size > 0 )
		frag = (nbuf << 16) | ilog2(size);

	/* must set fragsize before returning 1 (temporary unavailable) */
	*fragsize = size;

	if( oss_convert_format(format, &fmt, &channels) )
		return -1;
	if( (oss.fd=open(oss_devname(), O_WRONLY | O_NONBLOCK)) < 0 )
		return 1;

	err=0;
	fmt_ = fmt; channels_ = channels; rate_ = rate;
	if( ioctl(oss.fd, SNDCTL_DSP_SETFMT, &fmt) < 0 || fmt_ != fmt )
		err--;
	if( ioctl(oss.fd, SNDCTL_DSP_CHANNELS, &channels) < 0 || channels_ != channels )
		err--;
	if( ioctl(oss.fd, SNDCTL_DSP_SPEED, &rate) < 0 || rate_ != rate )
		err--;
	ioctl( oss.fd, SNDCTL_DSP_SETFRAGMENT, &frag );
	ioctl( oss.fd, SNDCTL_DSP_GETBLKSIZE, &size );
#if 0
	if( (1 << (frag & 0xffff)) != size )
		printm("oss: fragsize %d not accepted (using %d)\n", 1<<(frag & 0x1f), size );
#endif
	if( size <= 32 )
		size = 4096;

	*fragsize = size;
	if( err )
		close( oss.fd );
	return err;
}

/* 0: ok, 1: might be supported, <0: unsupported/error */
static int
oss_query( int format, int rate, int *fragsize )
{
	int ret;

	if( (ret=oss_open(format, rate, fragsize, 0)) )
		return ret;
	oss_close();
	return 0;
}

static void
oss_flush( void )
{
#if 0
	/* we are closing the device, this should not be necessary */
	if( ioctl(oss.fd, SNDCTL_DSP_SYNC) < 0 )
		perrorm("SNDCTL_DSP_SYNC\n");
#endif
}

/* size should _always_ equal ss.fragsize */
static void
oss_write( char *buf, int size )
{
	struct pollfd ufd;
	int s;

	while( size ) {
		ufd.fd = oss.fd;
		ufd.events = POLLOUT;

		poll( &ufd, 1, -1 );
		if( (s=write(oss.fd, buf, size)) < 0 )
			break;
		size -= s;
		buf += s;
	}
}

static void
oss_volume( int _lvol, int _rvol )
{
	int fd, vol, lvol, rvol;
	
	/* we can't use oss.fd here; it might be invalid */

	lvol = _lvol * 100 / 0xff;
	rvol = _rvol * 100 / 0xff;
	vol = rvol*0x100 | lvol;

	if( (fd=open(oss_devname(), O_RDONLY | O_NONBLOCK)) < 0 ) {
		printm("failed to set volume\n");
		return;
	}
	ioctl( fd, SOUND_MIXER_WRITE_VOLUME, &vol );
	close( fd );
}


static sound_ops_t oss_driver_ops = {
	.cleanup	= NULL,
	.query		= oss_query,
	.open		= oss_open,
	.close 		= oss_close,
	.write		= oss_write,
	.flush		= oss_flush,
	.volume		= oss_volume
};

sound_ops_t *
oss_probe( int dummy_exact )
{
	int fd = open( oss_devname(), O_WRONLY | O_NONBLOCK );
	if( fd >= 0 )
		close( fd );
	if( fd < 0 ) {
		if( errno != EBUSY )
			return NULL;
		else
			printm("OSS sound device unavailable (will be used anyway)\n");
	}
	printm("OSS sound driver loaded\n");
	return &oss_driver_ops;
}
