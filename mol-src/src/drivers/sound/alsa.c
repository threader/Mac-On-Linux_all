/* 
 *   Creation Date: <2004/06/05 18:06:31 samuel>
 *   Time-stamp: <2004/06/05 20:11:16 samuel>
 *   
 *	<alsa.c>
 *	
 *	ALSA sound backend
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */

#define ALSA_PCM_NEW_HW_PARAMS_API
/* #define ALSA_PCM_NEW_SW_PARAMS_API */

#include "mol_config.h"
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>
#include <alsa/error.h>
#include "booter.h"
#include "poll_compat.h"
#include "res_manager.h"
#include "sound-iface.h"

static struct {
	snd_pcm_t		*pcm;
	snd_mixer_t		*mixer;
	snd_mixer_elem_t	*pcm_elem;

	int			bpf;
} alsa;

static int
alsa_fragsize( void ) 
{
	int s, size = is_classic_boot()? 2048 : 512;

	s = get_numeric_res("alsa.fragsize");
	if( s > 128 && s <= 32768 )
		size = s;
	return size;
}

static int
alsa_query( int format, int rate, int *fragsize )
{
	if( rate != 44100 && rate != 22050 && rate != 48000 )
		return -1;
	if( !(format & (kSoundFormat_S16_BE | kSoundFormat_U8)) )
		return -1;
	*fragsize = alsa_fragsize();

	return 0;
}

static void
alsa_prefill( void )
{
	snd_pcm_status_t *status;
	char buf[512];
	int frames, cnt;

	snd_pcm_status_malloc( &status );
	snd_pcm_status( alsa.pcm, status );
	frames = snd_pcm_status_get_avail(status);
	snd_pcm_status_free( status );

	memset( buf, 0, sizeof(buf) );
	cnt = sizeof(buf)/alsa.bpf;
	for( ; frames > 0; frames -= cnt  ) {
		if( cnt > frames )
			cnt = frames;
		snd_pcm_writei( alsa.pcm, buf, sizeof(buf)/alsa.bpf );
	}
}

static const char *
alsa_devname( int for_mixer )
{
	const char *name = NULL;
	if( for_mixer )
		name = get_str_res("alsa.mixerdevice");
	if( !name )
		name = get_str_res("alsa.device");
	if( !name )
		name = "default";
	return name;
}

static int
alsa_open( int format, int rate, int *fragsize, int ringmode )
{
	const char *name = alsa_devname(0);
	snd_pcm_hw_params_t *params;
	int alsaformat;

	alsa.bpf = sound_calc_bpf( format );
	
	/* printm("ALSA open (%s) %d \n", name, rate ); */
	if( snd_pcm_open(&alsa.pcm, name, SND_PCM_STREAM_PLAYBACK, SND_PCM_NONBLOCK ) < 0 )
		return -1;
	snd_pcm_close( alsa.pcm );
	if( snd_pcm_open(&alsa.pcm, name, SND_PCM_STREAM_PLAYBACK, 0) < 0 )
		return -1;

	if( snd_pcm_hw_params_malloc(&params) < 0 ) {
		snd_pcm_close( alsa.pcm );
		return -1;
	}

	alsaformat = SND_PCM_FORMAT_S16_BE;
	if( format & kSoundFormat_U8 )
		alsaformat = SND_PCM_FORMAT_U8;
	
	snd_pcm_hw_params_any( alsa.pcm, params );
	snd_pcm_hw_params_set_access( alsa.pcm, params, SND_PCM_ACCESS_RW_INTERLEAVED );
	snd_pcm_hw_params_set_format( alsa.pcm, params, alsaformat );
	snd_pcm_hw_params_set_rate_near( alsa.pcm, params, (unsigned int *)&rate, 0 );
	snd_pcm_hw_params_set_channels( alsa.pcm, params, (format & kSoundFormat_Stereo)? 2:1 );
	snd_pcm_hw_params( alsa.pcm, params );

	snd_pcm_hw_params_free( params );
	snd_pcm_prepare( alsa.pcm );

	if( ringmode )
		alsa_prefill();

	*fragsize = alsa_fragsize();
	return 0;
}

static void
alsa_close( void )
{
	snd_pcm_drain( alsa.pcm );
	snd_pcm_close( alsa.pcm );
	alsa.pcm = NULL;
}

static void
alsa_write( char *buf, int size ) 
{
	struct pollfd pfd;

	snd_pcm_poll_descriptors( alsa.pcm, &pfd, 1 );
	poll( &pfd, 1, -1 );

	snd_pcm_writei( alsa.pcm, buf, size/alsa.bpf );
}

static void
alsa_flush( void ) 
{
}

static void
alsa_volume( int lvol, int rvol )
{
	if( alsa.pcm_elem ) {
		snd_mixer_selem_set_playback_volume(alsa.pcm_elem,
						    SND_MIXER_SCHN_FRONT_LEFT, lvol);
		snd_mixer_selem_set_playback_volume(alsa.pcm_elem,
						    SND_MIXER_SCHN_FRONT_RIGHT, rvol);
	}
}

static void
alsa_cleanup( void )
{
	if( alsa.mixer ) {
		snd_mixer_close( alsa.mixer );
		alsa.mixer = NULL;
	}
}

static sound_ops_t alsa_driver_ops = {
	.cleanup	= alsa_cleanup,
	.query		= alsa_query,
	.open		= alsa_open,
	.close 		= alsa_close,
	.write		= alsa_write,
	.flush		= alsa_flush,
	.volume		= alsa_volume,
};

sound_ops_t *
alsa_probe( int exact )
{
	snd_pcm_t *pcm;
	const char *name = alsa_devname(0);
	
	if( !snd_mixer_open(&alsa.mixer, 0) ) {
		snd_mixer_selem_id_t *selem_id;
		const char *s, *mixername = get_str_res("alsa.mixer");
		const char *devname = alsa_devname(1);
		int mixerindex=0;
		
		if( (s=get_str_res_ind("alsa.mixer", 0, 1)) )
			mixerindex = atoi(s);
		if( mixername )
			printm("Using ALSA mixer %s %d (mixerdevice '%s')\n",
			       mixername, mixerindex, devname );
		if( !mixername )
			mixername = "PCM";

		snd_mixer_selem_id_alloca( &selem_id );
		snd_mixer_selem_id_set_index( selem_id, mixerindex );
		snd_mixer_selem_id_set_name( selem_id, mixername );

		int err = snd_mixer_attach( alsa.mixer, devname )
			|| snd_mixer_selem_register( alsa.mixer, NULL, NULL )
			|| snd_mixer_load( alsa.mixer );
		if( !err )
			alsa.pcm_elem=snd_mixer_find_selem( alsa.mixer, selem_id );
		if( alsa.pcm_elem )
			snd_mixer_selem_set_playback_volume_range( alsa.pcm_elem, 0, 0xff );
	}
	if( !snd_pcm_open(&pcm, name, SND_PCM_STREAM_PLAYBACK, 1 * SND_PCM_NONBLOCK) )
		snd_pcm_close( pcm );
	else if( !exact )
		return NULL;
	else
		printm("ALSA device unavailable (will be used anyway)\n");

	printm("ALSA sound driver loaded (device '%s')\n", name);
	if( !alsa.pcm_elem )
		printm("ALSA: failed to setup mixer\n");
	return &alsa_driver_ops;
}
