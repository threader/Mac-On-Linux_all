/*
 * SDL I/O Driver
 * This driver initializes the SDL subsystem
 *
 * Copyright (C) 2007, Joseph Jezak (josejx@gentoo.org)
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License, version 2
 * as published by the Free Software Foundation
 */

#include "mol_config.h"
#include "res_manager.h"
#include "driver_mgr.h"

#ifdef HAVE_SDL
/* Global */

#include <SDL/SDL.h>

static int sdl_init (void) {
	int sdl_init_flags;

	printm("SDL Master Driver\n");
	sdl_init_flags = SDL_INIT_TIMER;

	if (get_bool_res("sdl_sound")) {
		sdl_init_flags |= SDL_INIT_AUDIO;
		printm("\tSDL Sound Supported\n");
	}
	
	if (get_bool_res("sdl_video")){
		sdl_init_flags |= SDL_INIT_VIDEO;
		printm("\tSDL Video Supported\n");
	}
	
	/* Initialize SDL */
	if (SDL_Init(sdl_init_flags) < 0) {
		printm("Couldn't initialize SDL: %s\n", SDL_GetError());
		return 0;
	}
	return 1;
}

static void sdl_cleanup (void) {
	SDL_Quit();
	return;
}

driver_interface_t sdl_driver = {
    "sdl", 
    sdl_init, 
    sdl_cleanup
};
#endif /* HAVE_SDL */
