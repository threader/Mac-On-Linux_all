/*
 *   driver_mgr.c
 *
 *   copyright (c)2000, 2001, 2002, 2003, 2004, Samuel Rydh
 *
 *   copyright (c)1999, 2003 Ben Martz
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
#include "driver_mgr.h"
#include "debugger.h"
#include "booter.h"
#include "drivers.h"

typedef struct {
	int			initted;
	driver_interface_t	*interface;
} driver_t;

/* All of the possible drivers that may be loaded */
extern driver_interface_t	osi_driver;
extern driver_interface_t	pseudofs_driver;
extern driver_interface_t	osi_mouse_driver;
extern driver_interface_t	hacks_driver;
extern driver_interface_t	pci_driver;
extern driver_interface_t	pci_bridges_driver;
extern driver_interface_t	gc_driver;
extern driver_interface_t	pic_driver;
extern driver_interface_t	osi_pic_driver;
extern driver_interface_t	hostirq_driver;
extern driver_interface_t	dbdma_driver;
extern driver_interface_t	via_cuda_driver;
extern driver_interface_t	adb_driver;
extern driver_interface_t	nvram_driver;
extern driver_interface_t	escc_driver;
extern driver_interface_t	swim3_driver;
extern driver_interface_t	awacs_driver;
extern driver_interface_t	hammerhead_driver;
extern driver_interface_t	mac_enet_driver;
extern driver_interface_t	enet2_driver;
extern driver_interface_t	old_scsi_driver;
extern driver_interface_t	s53c94_driver;
extern driver_interface_t	mesh_driver;
extern driver_interface_t	x11_common_driver;
extern driver_interface_t	video_driver;
extern driver_interface_t	osi_sound_driver;
extern driver_interface_t	usb_driver;
extern driver_interface_t	osi_kbd_driver;
extern driver_interface_t	keycodes_driver;
extern driver_interface_t	blkdev_setup;
extern driver_interface_t	ablk_driver;
extern driver_interface_t	scsi_driver;
extern driver_interface_t	tty_driver;
extern driver_interface_t	ioports_driver;
extern driver_interface_t	rtas_driver;
extern driver_interface_t	pciproxy_driver;
extern driver_interface_t	sdl_driver;
extern driver_interface_t	sdl_video_driver;

/* If the driver is optional, make sure to #ifdef it */
static driver_t drivers[] = {
	{ 0,	&osi_driver },
	{ 0,	&ioports_driver },
	{ 0,	&pci_driver },
	{ 0,	&pci_bridges_driver },
	{ 0,	&gc_driver },
	{ 0,	&pic_driver },
	{ 0,	&osi_pic_driver },
	{ 0,	&hostirq_driver },
	{ 0,	&pseudofs_driver },
	{ 0,	&rtas_driver },
	{ 0,	&keycodes_driver },
	{ 0,	&osi_kbd_driver },
	{ 0,	&dbdma_driver },
	{ 0,	&osi_mouse_driver },
	{ 0,	&via_cuda_driver },
	{ 0,	&nvram_driver },
	{ 0,	&usb_driver },
	{ 0,	&escc_driver },
#ifdef CONFIG_TTYDRIVER
	{ 0,	&tty_driver },
#endif
#ifdef CONFIG_X11
	{ 0,	&x11_common_driver },
#endif
#ifdef CONFIG_SDL
	{ 0, 	&sdl_driver },
#ifdef CONFIG_SDL_VIDEO
	{ 0,	&sdl_video_driver },
#endif /* SDL_VIDEO */
#endif /* SDL */
	{ 0,	&video_driver },
	{ 0,	&mac_enet_driver },
	{ 0,	&enet2_driver },
	{ 0,	&osi_sound_driver },
	{ 0,	&blkdev_setup },
	{ 0,	&scsi_driver },
	{ 0,	&ablk_driver },
#ifdef CONFIG_PCIPROXY
	{ 0,	&pciproxy_driver },
#endif
};

/* OW Drivers are seperate because they are only loaded if an oldworld
 * boot is done, not in general */
#ifdef OLDWORLD_SUPPORT
static driver_t ow_drivers[] = {
	{ 0,	&old_scsi_driver },
	{ 0,	&s53c94_driver },
	{ 0,	&mesh_driver },
	{ 0,	&hacks_driver },
	{ 0,	&swim3_driver },
	{ 0,	&awacs_driver },
	{ 0,	&hammerhead_driver },
};
#endif

int
driver_mgr_init( void )
{
	driver_t *d;
	int i;
	
	for( i=0, d=drivers; i < sizeof(drivers)/sizeof(drivers[0]); i++, d++ )
		d->initted = d->interface->init ? d->interface->init() : 1;

#ifdef OLDWORLD_SUPPORT
	if( is_oldworld_boot() )
		for( i=0, d=ow_drivers; i < sizeof(ow_drivers)/sizeof(ow_drivers[0]); i++, d++ )
			d->initted = d->interface->init ? d->interface->init() : 1;
#endif
	return 1;
}


void
driver_mgr_cleanup( void )
{
	driver_t *d;
	int i;
	
#ifdef OLDWORLD_SUPPORT
	if( is_oldworld_boot() )
		for( i=sizeof(ow_drivers)/sizeof(ow_drivers[0])-1; i >= 0 ; i-- ) {
			d = &ow_drivers[i];			
			if( d->initted && d->interface->cleanup )
				d->interface->cleanup();
			d->initted = 0;
		}
#endif

	for( i=sizeof(drivers)/sizeof(drivers[0])-1; i >=0 ; i-- ) {
		d = &drivers[i];
		if( d->initted && d->interface->cleanup )
			d->interface->cleanup();
		d->initted = 0;
	}
}
