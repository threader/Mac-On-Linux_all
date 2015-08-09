/* 
 *   Creation Date: <2004/06/05 18:09:41 samuel>
 *   Time-stamp: <2004/06/05 19:57:51 samuel>
 *   
 *	<sound-iface.h>
 *	
 *	
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */

#ifndef _H_SOUND_IFACE
#define _H_SOUND_IFACE

#include "sound_sh.h"

#define SND_DEBUG 0
#if SND_DEBUG
#define DEBUG_SND \
	if (0) {} else printm
#else
#define DEBUG_SND \
	if (1) {} else printm
#endif /* SND_DEBUG */

typedef struct {
	void	(*cleanup)( void );

	int	(*query)( int format, int rate, int *frasize );
	int	(*open)( int format, int rate, int *fragsize, int ringmode );
	void	(*close)( void );

	void	(*write)( char *fragptr, int size );
	void	(*flush)( void );
	void	(*volume)( int lvol, int rvol );
} sound_ops_t;

extern uint			sound_calc_bpf( int format );

#ifdef CONFIG_OSS
extern sound_ops_t		*oss_probe( int dummy_exact );
#else
static inline sound_ops_t	*oss_probe( int exact ) { return NULL; }
#endif

#ifdef CONFIG_ALSA
extern sound_ops_t 		*alsa_probe( int exact );
#else
static inline sound_ops_t	*alsa_probe( int exact ) { return NULL; }
#endif

#ifdef CONFIG_SDL_SOUND
extern sound_ops_t		*sdl_sound_probe(int exact);
#else
static inline sound_ops_t	*sdl_sound_probe(int exact) { return NULL; }
#endif

/* We always build the no sound plugin */
extern sound_ops_t		*nosound_prove(int exact);

#endif   /* _H_SOUND_IFACE */
