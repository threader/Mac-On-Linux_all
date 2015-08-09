/*
 * SDL Sound Driver
 *
 * Copyright (C) 2007, Joseph Jezak (josejx@gentoo.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 2
 * as published by the Free Software Foundation
 *
 * This isn't fancy, it's basically just a shim between the MOL audio driver
 * on the host and the SDL Audio library.  No mixer, no complications. :)
 *
 */

#include <SDL/SDL.h>
#include <SDL/SDL_audio.h>
#include <time.h>

#include "mol_config.h"
#include "sound-iface.h"

#define NUM_SDL_SOUND_BUFFERS	 16		/* ~64k for most setups */

/* This struct holds persistant data for the sdl sound device */
typedef struct {
	/* Left and right channel volume */
	int lvol;
	int rvol;

	/* Current sound format (AudioSpec) */
	SDL_AudioSpec *format;

	/* Circular sound buffer */
	unsigned char *buf;
	/* Size of the buffer */
	int buf_sz;
	/* Pointer to current buffer position */
	unsigned char *buf_head;
	/* Pointer to end of buffer position */
	unsigned char *buf_tail;
	/* Length of data in buffer */
	int buf_used;
	/* Audio Semaphore for access to the data */
	SDL_mutex *buf_lock;
} sdl_sound_info_t;

static sdl_sound_info_t	s;

/******************************************************************************
 * SDL Interface
 *****************************************************************************/

/* Callback function for SDL sound 
 * NOTE: This assumes that data doesn't cross the buffer 
 * OSX writes in 2K chunks...
 */
static void sdl_sound_stream (void *arg, u8 *stream, int stream_len) {
	int copy_amt;
	DEBUG_SND("Stream Length: %i\n", stream_len);
	/* Write out 0's to the stream so we can mix it */
	memset(stream, 0, stream_len);
	/* If we've got data to push */
	SDL_LockMutex(s.buf_lock);
	if (s.buf_used > 0) {
		copy_amt = (s.buf_used < stream_len) ? s.buf_used : stream_len;
		/* Copy the data */
		/* FIXME Volume */
		SDL_MixAudio(stream, s.buf_head, copy_amt, s.lvol);
		s.buf_head += (s.buf_used < stream_len) ? s.buf_used : stream_len;
		s.buf_used = (s.buf_used < stream_len) ? 0 : s.buf_used - stream_len;
		if(s.buf_head >= s.buf + s.buf_sz)
			s.buf_head = s.buf;
		SDL_UnlockMutex(s.buf_lock);
	}
	/* Otherwise, just write out silence */
	else {
		SDL_UnlockMutex(s.buf_lock);
		DEBUG_SND("SDL Sound: Wrote silence\n");
	}
}

/******************************************************************************
 * MOL Interface
 *****************************************************************************/

/* Initialize the AudioSpec struct */
static SDL_AudioSpec * sdl_sound_init_spec(int format, int rate) {
	SDL_AudioSpec *spec;
	spec = malloc(sizeof(SDL_AudioSpec));
	if (spec == NULL)
		return NULL;
	memset(spec, 0, sizeof(SDL_AudioSpec));
	
	/* Find the desired rate */
	spec->freq = rate;
	/* Find format */
	switch(format & kSoundFormatMask) {
	case kSoundFormat_S16_BE:
		spec->format = AUDIO_S16MSB;	
		break;
	case kSoundFormat_S16_LE:
		spec->format = AUDIO_S16LSB;	
		break;
	/* Default to U8 - Should always work... */
	case kSoundFormat_U8:
	default:
		spec->format = AUDIO_U8;	
		break;
	}
	
	/* If stereo is enabled, use two channels */
	if(format & kSoundFormat_Stereo)
		spec->channels = 2;
	else
		spec->channels = 1;
	
	/* And a few defaults */
	spec->callback = sdl_sound_stream;
	spec->userdata = NULL;
	return spec;
}

/* Destroy audio device */
static void sdl_sound_cleanup (void) {
	return;
}

/* Query the audio device 
 *
 * Takes:
 * format - Sound Format Type (BE, etc.)
 * rate - Sound Rate (44KHz, 22kHz etc)
 * fragsize - Write the fragment size back to this var
 *
 * Returns:
 * -1 if we can't match format/rate
 * 0 if the rate is supported
 * 1 if it may be supported
 */
static int sdl_sound_query (int format, int rate, int *fragsize) {
	SDL_AudioSpec *s = sdl_sound_init_spec(format, rate);
	int ret = 0;
	int fmt = 0;

	/* Check if we got the Spec */
	if (s == NULL)
		return -1;
	
	/* Open the audio device */
	if (SDL_OpenAudio(s, s) < 0) {
		printm("Unable to open the audio device: %s", SDL_GetError());
		ret = -1;
	}
	else {
		/* Is the rate any good ? */
		if (s->freq != rate) 
			ret = -1;
		/* Is the format any good ? */
		fmt = format & kSoundFormatMask;	
		if (fmt == kSoundFormat_S16_BE && s->format != AUDIO_S16MSB)
			ret = -1;
		if (fmt == kSoundFormat_S16_LE && s->format != AUDIO_S16LSB)
			ret = -1;
		if (fmt == kSoundFormat_U8 && s->format != AUDIO_U8)
			ret = -1;
		*fragsize = s->samples;
		DEBUG_SND("SND: Frag size: %i\n", s->samples);
		SDL_CloseAudio();
	}
	/* Free the spec */
	free(s);
	return ret;
}

/* Open the audio device */
static int sdl_sound_open (int format, int rate, int *fragsize, int ringmode) {
	DEBUG_SND("SDL Sound Open (format: %i rate: %i)\n", format, rate);
	s.format = sdl_sound_init_spec(format, rate);
	if (s.format == NULL) {
		printm("Unable to alloc the audio spec\n");
		return -1;
	}
	
	/* Open the audio device -- we assume that the format is okay from the query earlier */
	if (SDL_OpenAudio(s.format, s.format) < 0) {
		printm("Unable to open the audio device: %s", SDL_GetError());
		return -1;
	}
	*fragsize = s.format->samples;

	/* Allocate memory for the buffer */
	s.buf_sz = s.format->samples * NUM_SDL_SOUND_BUFFERS;
	s.buf = malloc(s.buf_sz);
	s.buf_used = 0;
	s.buf_head = s.buf;
	s.buf_tail = s.buf;

	/* Okay, we're ready to start sound playback */
	SDL_PauseAudio(0);
	return 0;
}

/* Close audio device */
static void sdl_sound_close (void) {
	/* Cleanup ! */
	SDL_CloseAudio();
	/* Free the buffer */
	free(s.buf);
	/* Free AudioSpec */
	if(s.format != NULL)
		free(s.format);
}
	
/* Write audio buffers 
 * NOTE: This assumes that data doesn't cross the buffer 
 * OSX writes in 2K chunks... 
 */
static void sdl_sound_write (char *fragptr, int size) {
	struct timespec t;
	t.tv_sec = 0;
	t.tv_nsec = 500;

	DEBUG_SND("SDL Sound Write: %i bytes\n", size);
	/* Check for overrun - we sleep until we can write into the buffer
	 * otherwise, OSX just pumps us data and things don't work right */
	while(s.buf_used + size > s.buf_sz)
		nanosleep(&t, NULL);
		
	SDL_LockMutex(s.buf_lock);
	/* Data fits in buffer */
	memcpy(s.buf_tail, fragptr, size);
	s.buf_used += size;
	s.buf_tail += size;
	/* Loop back */
	if (s.buf_tail >= s.buf + s.buf_sz)
		s.buf_tail = s.buf;
	SDL_UnlockMutex(s.buf_lock);
}

/* Flush the audio buffers */
static void sdl_sound_flush (void) {
	return;
}
	
/* Change the volume */
static void sdl_sound_volume (int lvol, int rvol) {
	/* Volumes should already be adjusted... */
	s.lvol = lvol;
	s.rvol = rvol;
}

static sound_ops_t sdl_sound_driver_ops = {
	.cleanup	= sdl_sound_cleanup,
	.query		= sdl_sound_query,
	.open		= sdl_sound_open,
	.close 		= sdl_sound_close,
	.write		= sdl_sound_write,
	.flush		= sdl_sound_flush,
	.volume		= sdl_sound_volume,
};

/* Exact indicates whether we called this explicitly or the user specified "any" */
sound_ops_t * sdl_sound_probe(int exact) {
	printm("SDL sound driver loaded\n");
	/* Do we always say okay?  I think so... */
	s.format = NULL;
	return &sdl_sound_driver_ops;
}
