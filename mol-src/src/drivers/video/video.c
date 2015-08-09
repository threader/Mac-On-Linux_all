/* 
 *   Creation Date: <1999/03/13 23:09:47 samuel>
 *   Time-stamp: <2004/02/07 18:15:15 samuel>
 *   
 *	<video.c>
 *	
 *	MOL video driver
 *
 *   Copyright (C) 2002, 2003, 2004 Samuel Rydh <samuel@ibrium.se>
 *   Copyright (C) 1999, 2000, 2001 Benjamin Herrenschmidt <bh40@calva.net>
 *
 *   1999/12/31: Modularized to support X/console switching,
 *		 Samuel Rydh <samuel@ibrium.se> 
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <sys/mman.h>
#include <pthread.h>

#include "booter.h"
#include "verbose.h"
#include "promif.h"
#include "debugger.h"
#include "pci.h"
#include "memory.h"
#include "wrapper.h"
#include "pic.h"
#include "timer.h"
#include "res_manager.h"
#include "driver_mgr.h"
#include "video.h"
#include "video_module.h"
#include "os_interface.h"
#include "mac_registers.h"
#include "console.h"
#include "osi_driver.h"
#include "session.h"
#include "input.h"
#include "video_sh.h"
#include "hacks.h"


#define DEFAULT_VBL_HZ		85

#define COLOR_NUM_TO_PTR( video_st, col_num ) \
	( (video_st)->color_table + ((col_num)%3)*256 + (col_num/3) )

#define RED(ind)		color_table[ (ind)*3+0 ]
#define GREEN(ind)		color_table[ (ind)*3+1 ]
#define BLUE(ind)		color_table[ (ind)*3+2 ]


/************************** Video state ****************************************/

#define MAX_NUM_DEPTHS 		5

typedef struct {
	int			num_depths;
	video_desc_t		*vm[ MAX_NUM_DEPTHS ];
} mode_table_rec_t;

typedef struct video_state {
	video_module_t		*all_modules;

	int			irq;		/* irq# */

	int			n_mmodes;	/* # mac modes in table */
	mode_table_rec_t	*mode_table;	/* table containing mac-modes */

	ulong			mbase;		/* page aligned mac address of FB */
	video_desc_t		cur_vmode;
	video_module_t		*cur_module;
	int			cur_vmode_index;
	int			cur_depth_mode;

	int			def_vmode;	/* startup parameters */
	int			def_depth_mode;

	int			fb_mapped;	/* if "switched" in through PCI base mechanism */
	struct mmu_mapping	fbmap;		/* MMU mapping of framebuffer */

	char			*color_table;	/* always 3*256 entries */
	int			cmap_dirty;

	int			macside_inited;	/* set once we know the mac is running */

	int			vbl_usec;	/* interrupt period in usecs */
	int			vbl_on;		/* generate IRQs (vbl timer runs regardless) */
	int			timer_id;

	int			events;		/* VIDEO_EVENT_xxx */
	int			use_hw_cursor;

	struct osi_driver	*osi_driver;

	int			initialized;
} video_state_t;

static video_state_t 		vc[1];

static struct { int w, h; } std_res[] = {
	{ 640, 480 }, { 800, 600 }, { 1024, 768 }, { 1152, 864 },
	{ 1280, 1024 }, { 1440, 960 }, { 1600, 1024 }, { 1600, 1200 },
	{ 1680, 1050 }
};

static video_module_t *video_mods[] = {
#ifdef CONFIG_X11
	&xvideo_module,
#endif
#ifdef CONFIG_VNC
	&vnc_module,
#endif
#ifdef CONFIG_XDGA
	&xdga_module,
#endif
#ifdef CONFIG_FBDEV
	&fbdev_module,
#endif
};


/************************************************************************/
/*	determine available vmodes					*/
/************************************************************************/

/* Find index:th mode matching w, h, depth and refresh */
static int
find_vmode( video_desc_t *mode, int index )
{
	video_desc_t *vm;
	int i;

	for(i=0; mode && i<vc->n_mmodes; i++ ){
		if( vc->mode_table[i].num_depths <= 0 )
			continue;
		vm = vc->mode_table[i].vm[0];
		if( vm->w != mode->w || vm->h != mode->h )
			continue;
		if( mode->refresh && vm->refresh != mode->refresh  )
			continue;
		if( !index-- )
			return i;
	}
	return -1;
}

/* fill in undefined fields in the cur_vmode field in "the most compatible" way */
static void
complete_vmode( video_desc_t *vm )
{
	int offs=-1, rowbytes=-1;
	video_module_t *m;
	video_desc_t *vm2;

	/* first, make sure basic fields like width, height and depth are filled in */
	vm->w = (vm->w == -1) ? 640 : vm->w;
	vm->h = (vm->h == -1) ? 480 : vm->h;
	vm->depth = (vm->depth == -1) ? 8 : vm->depth;
	vm->refresh = (vm->refresh == -1) ? 0 : vm->refresh;

	/* find "compatible" values for offs and rowbytes */
	for( m=vc->all_modules; m; m=m->next ) {
		if( m == vc->cur_module )
			continue;

		for( vm2=m->modes; vm2; vm2=vm2->next ){
			if( vm2->w == vm->w && vm2->h == vm->h && vm2->depth == vm->depth ) {
				if( vm2->rowbytes != -1 )
					rowbytes = vm2->rowbytes;
				if( vm2->offs != -1 )
					offs = vm2->offs;
			}
		}
	}
	if( rowbytes == -1 ) {
		if( vm->depth > 1 )
			rowbytes = vm->w * ((vm->depth > 16)? 4 : ((vm->depth > 8)? 2 : 1));
		else
			rowbytes = vm->w/8;	/* B&W */
	}
	if( offs == -1 )
		offs = 0;

	/* fill in offs and rowbytes value */
	vm->offs = (vm->offs == -1)? offs : vm->offs;
	vm->rowbytes = (vm->rowbytes == -1)? rowbytes : vm->rowbytes;
}

/* return true if the the module m supports the specified video mode */
static int
supports_vmode_( video_module_t *m, video_desc_t *mode, void **module_data )
{
	video_desc_t *vm, *best;
	
	if( m->modes == NULL )
		return 1;

	best = NULL;

	for( vm=m->modes; vm ; vm=vm->next ) {
		if( vm->depth != -1 && std_depth(vm->depth) != std_depth(mode->depth) )
			continue;
		if( vm->w != -1 && vm->w != mode->w )
			continue;
		if( vm->h != -1 && vm->h != mode->h )
			continue;
		if( vm->rowbytes != -1 && vm->rowbytes != mode->rowbytes )
			continue;
		if( vm->offs != -1 && vm->offs != mode->offs )
			continue;
		if( !best || abs(vm->refresh - mode->refresh) <  abs(best->refresh - mode->refresh))
			best = vm;
	}
	if( !best )
		return 0;

	if( module_data != NULL ) 
		*module_data = best->module_data;
	return 1;
}

int
supports_vmode( video_module_t *m, video_desc_t *vm )
{
	if( !vm )
		vm = &vc->cur_vmode;
	return supports_vmode_( m, vm, NULL );
}

static int
vmode_available( video_desc_t *mode )
{
	video_module_t *m;
	video_desc_t mcpy = *mode;
	
	complete_vmode( &mcpy );
	for( m=vc->all_modules; m; m=m->next ) {
		if( supports_vmode( m, &mcpy ))
			return 1;
	}
	return 0;
}


/* add video mode to mac-modes table */
static void
add_vmode( video_desc_t *new_mode )
{
	video_desc_t mode=*new_mode;
	int ind,i,j, found;
	mode_table_rec_t *mt;
	
	if( mode.w == -1 || mode.h == -1 || mode.depth == -1 )
		return;
	complete_vmode( &mode );

	for( ind=0 ;; ind++ ) {
		if( (i=find_vmode( &mode, ind)) < 0 )
			break;
		found=0;
		for( j=0; j<vc->mode_table[i].num_depths; j++ ) {
			video_desc_t *vm = vc->mode_table[i].vm[j];
			if( vm->depth == mode.depth )
				found=1;
			if( vm->depth != mode.depth || vm->rowbytes != mode.rowbytes || vm->offs  )
				continue;
			return;	/* exact match - drop */
		}
		if( !found )
			break;
	}
	/* add extra video mode if necessary */
	if( i< 0 ){
		mode.next = NULL;
		i = vc->n_mmodes++;
		vc->mode_table = realloc( vc->mode_table, vc->n_mmodes * sizeof(mode_table_rec_t) );
		memset( &vc->mode_table[i], 0, sizeof(mode_table_rec_t) );
	}
	/* insert into mode i */
	if(vc->mode_table[i].num_depths >= MAX_NUM_DEPTHS-1 ) {
		printm("Error: More than 5 different depths?!\n");
		return;
	}
	mt = &vc->mode_table[i];
	for(j=0; j<mt->num_depths; j++ )
		if( mt->vm[j]->depth >= mode.depth )
			break;
	memmove( &mt->vm[j+1], &mt->vm[j], (mt->num_depths-j)*sizeof(video_desc_t*));
	mt->num_depths++;
	mt->vm[j] = malloc( sizeof(video_desc_t) );
	*(mt->vm[j]) = mode;
}

static int 
qsort_vmode_cmpar( const void *el1, const void *el2 )
{
	video_desc_t *vm1, *vm2;
	vm1=((mode_table_rec_t*)el1)->vm[0];
	vm2=((mode_table_rec_t*)el2)->vm[0];

	if( vm1->w < vm2->w || vm1->h < vm2->h )
		return -1;
	else if( vm1->w == vm2->w && vm1->h == vm2->h )
		if( vm1->refresh < vm2->refresh )
			return -1;
	return 1;
}

static void
prepare_vmodes( void )
{
	int count, i,j, w, h, depth, refresh, depths[] = {8,15,32};
	video_module_t *m;
	video_desc_t *best, *vm, smode;
	char *str;
	float fref;
	
	for( m=vc->all_modules; m; m=m->next )
		for( vm=m->modes; vm; vm=vm->next )
			add_vmode( vm );

	/* add standard modes not previously in the list */
	for( i=0; i<sizeof(std_res)/sizeof(std_res[0]); i++ ){
		for( j=0; j<sizeof(depths)/sizeof(int); j++ ) {
			memset( &smode, 0, sizeof( video_desc_t ) );
			smode.w = std_res[i].w;
			smode.h = std_res[i].h;
			smode.depth = std_depth(depths[j]);
			smode.rowbytes = -1;
			smode.offs = -1;
			smode.refresh = -1;

			complete_vmode( &smode );
			if( vmode_available( &smode ) )
				add_vmode( &smode );
		}
	}
	if( !vc->n_mmodes )
		return;
	
	qsort( vc->mode_table, vc->n_mmodes, sizeof(mode_table_rec_t), qsort_vmode_cmpar );

	/* print mode information */
	printm("\n");
	for(i=0; i<vc->n_mmodes; i++ ){
		vm = vc->mode_table[i].vm[0];
		printm("    %4d*%4d, depth ", vm->w, vm->h );
		for( j=0; j<vc->mode_table[i].num_depths; j++ )
			printm("%s%d", (j? ",":"") , vc->mode_table[i].vm[j]->depth );

		printm("   { ");
		for( j=i; j<vc->n_mmodes; j++ ){
			video_desc_t *vm2 = vc->mode_table[j].vm[0];
			int k, equal; 

			equal = vc->mode_table[i].num_depths == vc->mode_table[j].num_depths;
			equal = equal && (vm->w == vm2->w && vm->h == vm2->h );
			for( k=0; equal && k<vc->mode_table[i].num_depths; k++ )
				equal = (vc->mode_table[i].vm[k]->depth == vc->mode_table[j].vm[k]->depth);

			if( equal )
				printm("%s%d.%d", (i==j)?"":", ", (vm2->refresh>>16), ((vm2->refresh*10)>>16)%10 );
			if( !equal || j==vc->n_mmodes -1 ) {
				printm(" } Hz\n");
				i = !equal ? j-1 : j;
				break;
			}
		}
	}
	printm("\n");

	/* Find a good startup resolution */
	count = 0;
	if( (str = get_str_res("resolution")) != NULL )
		if( (count = sscanf(str, "%d/%d/%f", &w, &h, &fref)) < 2 )
			printm("parser error: 'resolution' entry malformed\n");
	if( count <= 2 )
		fref = 75; 
	depth = get_numeric_res("depth");	/* -1 if nonexistent */
	refresh = (int)(fref*65536.0);
	if( count < 2 )
		w=640, h=480;
	best = NULL;
	for( i=0; i<vc->n_mmodes; i++ ){
		for(j=0; j<vc->mode_table[i].num_depths; j++ ) {
			vm = vc->mode_table[i].vm[j];
			if( vm->w == w && vm->h == h && (depth == -1 || std_depth(vm->depth) == std_depth(depth)) ) {
				if( !best || abs(vm->refresh - refresh) <  abs(best->refresh - refresh)) {
					best = vm;
					vc->def_vmode = i;
					vc->def_depth_mode = j;
				}
			}
		}
	}
	if( !best ) {
		printm("No video mode match the default one.\n");
		best = vc->mode_table[0].vm[0];
		vc->def_vmode = 0;
		vc->def_depth_mode = 0;
	}
}


/************************************************************************/
/*	VBL								*/
/************************************************************************/

static void
do_vbl_interrupt( int id, void *dummy, int lost_ticks )
{
	if( vc->cur_module->vbl )
		vc->cur_module->vbl();
	
	if( vc->vbl_on && vc->irq != -1 )
		irq_line_hi( vc->irq );
}

static void
restart_vbl( void )
{
	set_ptimer( vc->timer_id, vc->vbl_usec );
}


/************************************************************************/
/*	framebuffer mapping						*/
/************************************************************************/

/* map framebuffer */
static void
map( void )
{
	video_desc_t *cm = &vc->cur_vmode;

	if( !vc->fb_mapped )
		return;

	vc->fbmap.mbase = vc->mbase;
	vc->fbmap.size = (cm->rowbytes * cm->h + cm->offs + 0xfff) & ~0xfff;
	vc->fbmap.lvbase = cm->lvbase;
	if( cm->map_base )
		vc->fbmap.lvbase = (char*)cm->map_base;
	vc->fbmap.flags	 = MAPPING_FB | cm->mmu_flags;
	
	VPRINT("mbase: %lX, size %x\n", vc->fbmap.mbase, vc->fbmap.size );
	VPRINT("lvbase: %p, flags %x\n", vc->fbmap.lvbase, vc->fbmap.flags );

	/* add the mmu-mapping of the framebuffer */
	_add_mmu_mapping( &vc->fbmap );
}

static void 
map_base_proc( int regnum, ulong base, int add_map, void *usr )
{
	video_state_t *st = (video_state_t*)usr;

	VPRINT("map_base_proc, regnum: %d, base: 0x%08lx, add:%d\n",
	       regnum, base, add_map);

	if( regnum ) {
		printm("map_base_proc: bad range\n");
		return;
	}	
	if( st->fb_mapped ) {
		_remove_mmu_mapping(&st->fbmap);
		st->fb_mapped = 0;
	}
	if( add_map ) {
		st->mbase = base;
		st->fb_mapped = 1;
		map();
	}
}

static int
set_vmode( video_desc_t *new_vm, video_module_t *m )
{
	int i, autoswitch=0, ref;
	
	if( vc->cur_module ) {
		if( vc->fb_mapped )
			_remove_mmu_mapping( &vc->fbmap );
		vc->cur_module->vclose();
	}
	vc->cur_vmode = *new_vm;
	ref = vc->cur_vmode.refresh;
	if( ref < (49<<16 ))
		ref = (DEFAULT_VBL_HZ << 16);

	vc->vbl_usec =( 65536.0 * 1000000.0 / (double)ref + 0.5 );

	/* try opening the video mode with the current/specified module */
	if( !m && vc->cur_module != &offscreen_module )
		m=vc->cur_module;
	if( m && supports_vmode_( m, &vc->cur_vmode, &vc->cur_vmode.module_data )) {
		if( m->vopen( &vc->cur_vmode ) )
			m = NULL;
	} else {
		m = NULL;
	}

	/* use one of the other modules */
	if( !m ) {
		for( m=vc->all_modules; m; m=m->next ){
			if( !supports_vmode_( m, &vc->cur_vmode, &vc->cur_vmode.module_data ))
				continue;
#ifdef CONFIG_FBDEV
			/* autoswitch to MOL console? */
			if( m == &fbdev_module && get_bool_res("autoswitch_console")==1 )
				autoswitch=1;
#endif
			if( !m->vopen( &vc->cur_vmode ) )
				break;
		}
	}

	/* last resort, use offscreen video */
	if( !m ) {
		m = &offscreen_module;
		m->vopen( &vc->cur_vmode );
	}

	/* fill in a good colortable (other fields in st->cmap are already filled in) */
	vc->cmap_dirty = 0;
	for( i=0; i<256; i++ ) {
		int d = (std_depth(vc->cur_vmode.depth) == std_depth(15))? i<<3 : i;
		vc->RED(i) = d;
		vc->GREEN(i) = d;
		vc->BLUE(i) = d;
	}
	if( m->setcmap )
		m->setcmap( vc->color_table );

	/* finally, map the new mode */
	vc->cur_module = m;
	map();

	/* Safest to do this after the offscreen buffer is established */
	if( autoswitch && !debugger_enabled() ) {
#ifdef __linux__
		printm("Autoswitching to console\n");
		console_to_front();
#endif
	}

	/* change VBL frequency */
	if( vc->timer_id )
		restart_vbl();
	return 0;
}


/************************************************************************/
/*	video module switching						*/
/************************************************************************/

/* transfer framebuffer control to module m (always different from cur_mod) */
static int
module_transition( video_module_t *m, void *module_data  )
{
	struct mmu_mapping oldmap = vc->fbmap;
	int was_mapped = vc->fb_mapped;
	video_desc_t vm = vc->cur_vmode;
	video_module_t *old_mod = vc->cur_module;

	VPRINT("transition\n");

	vm.module_data = module_data;
	if( m->vopen( &vm ) ) {
		VPRINT("transition-open failed\n");
		return 1;
	}

	/* Set cmap */
	if( m->setcmap )
		m->setcmap( vc->color_table );

	/* copy framebuffer data */
	if( vm.lvbase ) {
		size_t size = vm.h * vm.rowbytes;
		if( vc->cur_vmode.lvbase ){
			memcpy( vm.lvbase + vm.offs, vc->cur_vmode.lvbase + vm.offs, size );
			if( m->vrefresh )
				m->vrefresh();
		} else {
			/* Note: memset doesn't work sinze it uses dcbz! */
			/* memset( vm.lvbase + vm.offs, 0, size); */
			long *p = (long*)(vm.lvbase + vm.offs);
			size /= sizeof(long);
			while( size-- )
				*p++ = 0;
		}

	}

	/* change active module */
	vc->cur_vmode = vm;
	vc->cur_module = m;
	map();
	if( was_mapped )
		_remove_mmu_mapping( &oldmap );     /* it is safe to remove a mapping never inserted */

	old_mod->vclose();
	return 0;
}

/*
 * The yielder video module is no longer available (due
 * to a console switch for instance). This routine must be
 * callable from any thread.
 */
int
video_module_yield( video_module_t *yielder, int can_fail )
{
	video_module_t *m;
	void *module_data;
	
	if( yielder != vc->cur_module ) {
		printm("video_module_yield call from non-owner\n");
		return 0;
	}
	
	for( m=vc->all_modules; m; m=m->next ) {
		if( m==yielder )
			continue;
		if( !supports_vmode_( m, &vc->cur_vmode, &module_data ) )
			continue;
		if( !module_transition( m, module_data ) )
			return 0;
	}
	if( can_fail )
		return 1;

	/* if everthing else fail, use an offscreen buffer */
	m = &offscreen_module;
	if( module_transition( m, NULL )) {
		printm("Error: The dummy_module may never refuse a transfer!\n");
	}
	return 0;
}

/* A video module asks to become the active module (e.g. VT switch to console) */
void
video_module_become( video_module_t *m )
{
	video_desc_t *vm = &vc->cur_vmode;
	void *module_data;

	/* make sure the video module is installed */
	if( !supports_vmode_( m, vm, &module_data ) ) {
		printm("'%s' does not support mode %d*%d, depth %d (offs %d, rb %d)\n", 
		       m->name, vm->w, vm->h, vm->depth, vm->offs, vm->rowbytes );
		return;
	}
	if( module_transition( m, module_data ) )
		printm("Video transition failed\n");
}


/************************************************************************/
/*	misc. external functions					*/
/************************************************************************/

int
mac_video_initialized( void )
{
	return vc->macside_inited;
}

char * 
get_vmode_str( video_desc_t *vm, char *buf, int size )
{
	if( !vm )
		vm = &vc->cur_vmode;
	snprintf( buf, size, "%d*%d, depth %d, %d.%d Hz  [offs:%d, rb:%d]", 
		  vm->w, vm->h, vm->depth, (vm->refresh>>16), ((vm->refresh*10)>>16)%10,
		  vm->offs, vm->rowbytes  );
	return buf;
}

static int
get_video_info( ulong *mphys, int *w, int *h, int *rb, int *depth)
{
	video_desc_t *vm = &vc->cur_vmode;
	*mphys = *w = *h = *rb = *depth = 0;

	if( !vc->initialized )
		return 1;	/* probably running without video */
	
	*w = vm->w;
	*h = vm->h;
	*depth = vm->depth;
	*rb = vm->rowbytes;
	*mphys = vc->mbase + vm->offs;
	return 0;
}

void
use_hw_cursor( int flag )
{
	if( vc->use_hw_cursor != flag ) {
		vc->events |= VIDEO_EVENT_CURS_CHANGE;
		mouse_hwsw_curs_switch();
	}

	vc->use_hw_cursor = flag;
}

int
switch_to_fullscreen( void )
{
	int ret1, ret2;

	if( !(ret1=switch_to_fbdev_video()) )
		return 0;
	if( ret1 && !(ret2=switch_to_xdga_video()) )
		return 0;
	if( ret1 < 0 && ret2 < 0 )
		return -1;
	return 1;
}

/************************************************************************/
/*	OSI interface							*/
/************************************************************************/

static int
check_mode_depth( int vmode, int depth_mode )
{
	/* both parameters starting at 0 */
	if( vmode < 0 || vmode >= vc->n_mmodes ) {
		VPRINT("Bad vmode %d\n", vmode );
		return 1;
	}
	if( depth_mode < 0 || depth_mode >= vc->mode_table[vmode].num_depths ) {
		VPRINT("Bad depth mode %d\n", depth_mode );
		return 1;
	}
	return 0;
}

static int
osip_video_ack_irq( int sel, int *params )
{
	if( !irq_line_low(vc->irq) )
		return 0;

	mregs->gpr[4] = vc->events;
	vc->events = 0;
	return 1;
}

static int
osip_video_cntrl( int sel, int *params )
{
	switch( params[0] ) {
	case kVideoStartVBL:
		vc->vbl_on = 1;
		break;
	case kVideoStopVBL:
		vc->vbl_on = 0;
		break;
	case kVideoRouteIRQ:
		oldworld_route_irq( params[1], &vc->irq, "video" );
		break;
	case kVideoUseHWCursor:
		return vc->use_hw_cursor;
	default:
		return -1;
	}
	return 0;
}


/* vmode depth_mode */
static int
osip_set_vmode( int sel, int *params )
{
	int vmode = params[0]-1;	/* 0,1,...,n_mmodes-1 */
	int depth_mode = params[1];

	VPRINT("set_vmode %d,%d\n", vmode, depth_mode );
	if( check_mode_depth( vmode, depth_mode ) )
		return 1;

	vc->cur_vmode_index = vmode;
	vc->cur_depth_mode = depth_mode;
	set_vmode( vc->mode_table[vmode].vm[depth_mode], NULL );
	return 0;
}

static int
osip_get_vmode_info( int sel, int *params )
{
	int vmode = params[0]-1;
	int depth_mode = params[1];
	osi_get_vmode_info_t *pb = (void*)&mregs->gpr[4];
	video_desc_t *vm;

	/* if params[0] == 0, then return default vmode/depth */
	if( vmode == -1 ){
		vmode = vc->def_vmode;
		depth_mode = vc->def_depth_mode;
	}

	/* This seem to be a good spot for this flag */
	vc->macside_inited = 1;

	VPRINT("osip_get_vmode_info: vmode=%d, depth_mode %d\n", vmode, depth_mode );
	if( check_mode_depth( vmode, depth_mode ) )
		return 1;
	vm = vc->mode_table[vmode].vm[depth_mode];
	pb->num_vmodes = vc->n_mmodes;
	pb->num_depths = vc->mode_table[vmode].num_depths;
	pb->cur_vmode = vmode+1;
	pb->cur_depth_mode = depth_mode;
	pb->w = vm->w;
	pb->h = vm->h;
	pb->depth = vm->depth;
	pb->refresh = !vm->refresh ? (DEFAULT_VBL_HZ << 16) : vm->refresh;
	pb->offset = vm->offs;
	pb->row_bytes = vm->rowbytes;

	/* printm("%d*%d, %d [rb %d, offs %d]\n", pb->w, pb->h, pb->depth, pb->row_bytes, pb->offset ); */
	return 0;
}

static int
osip_get_fb_info( int sel, int *params )
{
	osi_fb_info_t *p = (osi_fb_info_t*)&mregs->gpr[4];	/* return in r3-r8 */
	return get_video_info( &p->mphys, &p->w, &p->h, &p->rb, &p->depth );
}

/* int osip_set_color( int index, int color ) */
static int
osip_set_color( int sel, int *params )
{
	int ind=params[0], color=params[1];
	
	if( ind >= 256 )
		return -1;
	if( ind < 0 ) {
		if( vc->cmap_dirty && vc->cur_module->setcmap )
			vc->cur_module->setcmap( vc->color_table );
		vc->cmap_dirty=0;
		return 0;
	}
	vc->RED(ind) = ((color>>16) & 0xff);
	vc->GREEN(ind) = ((color>>8) & 0xff);
	vc->BLUE(ind) = (color & 0xff);
	vc->cmap_dirty = 1;

	return 0;
}

static int
osip_get_color( int sel, int *params )
{
	uint ind = params[0];
	if( ind < 256 )
		return ((int)vc->RED(ind) << 16) | ((int)vc->GREEN(ind) << 8) | vc->BLUE(ind);
	return -1;
}

static int
osip_set_powermode( int sel, int *params )
{
	if( vc->cur_module && vc->cur_module->vpowermode )
		vc->cur_module->vpowermode( params[0] );
	return 0;
}

static const osi_iface_t osi_iface[] = {
	{ OSI_SET_VMODE,	osip_set_vmode		},
	{ OSI_GET_VMODE_INFO,	osip_get_vmode_info	}, 
	{ OSI_SET_COLOR,	osip_set_color		}, 
	{ OSI_GET_COLOR,	osip_get_color		}, 
	{ OSI_GET_FB_INFO,	osip_get_fb_info	},
	{ OSI_VIDEO_ACK_IRQ,	osip_video_ack_irq	},
	{ OSI_VIDEO_CNTRL,	osip_video_cntrl	},
	{ OSI_SET_VIDEO_POWER,	osip_set_powermode	},
};

/************************************************************************/
/*	debugger CMDs							*/
/************************************************************************/

static int __dcmd
cmd_vbl( int numargs, char **args ) 
{
	if( numargs !=1 )
		return 1;

	vc->vbl_on = !vc->vbl_on;
	printm("VBL %s\n", vc->vbl_on ? "started" : "stopped" );
	return 0;
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static int
save_video( void )
{
	video_desc_t *vm = &vc->cur_vmode;
	size_t size = vm->h * vm->rowbytes;

	if( vm->lvbase )
		if( write_session_data( "fbuf", 0, vm->lvbase, size ) )
			return 1;
	return write_session_data( "VID", 0, (char*)&vc, sizeof(vc) );
}

static void
load_video( void )
{
	video_state_t saved_state;
	video_desc_t *vm;

	if( read_session_data( "VID", 0, (char*)&saved_state, sizeof(saved_state) ) )
		session_failure("Could not read video state\n");

	vc->cur_vmode_index = saved_state.cur_vmode_index;
	vc->cur_depth_mode = saved_state.cur_depth_mode;
	vc->vbl_on = saved_state.vbl_on;
	
	if( check_mode_depth( vc->cur_vmode_index, vc->cur_depth_mode ) )
		session_failure("Could not restore old video mode\n");
	
	set_vmode( vc->mode_table[vc->cur_vmode_index].vm[vc->cur_depth_mode], NULL );

	vm = &vc->cur_vmode;
	if( vm->lvbase ) {
		ssize_t s = get_session_data_size( "fbuf", 0 );
		if( s == vm->h * vm->rowbytes ) {
			read_session_data( "fbuf", 0, vm->lvbase, s );
		} else {
			session_failure("Framebuffer size mismatch\n");
		}
	}

	vc->macside_inited = saved_state.macside_inited;
}

static int
video_init( void )
{
	static pci_dev_info_t pci_config = { 0x6666, 0x6666, 0x00, 0x00, 0x0300 };
	video_state_t *st = &vc[0];
	video_module_t *p, **vmp;
	int i;

	memset( st, 0, sizeof(video_state_t) );

	if( get_bool_res("no_video")==1 )
		return 0;

	if( !is_linux_boot() )
		if( !(st->osi_driver=register_osi_driver( "video", "MacOnLinuxVideo", &pci_config, &st->irq )))
			return 0;

	if( is_classic_boot() || is_osx_boot() ) {
		pci_addr_t addr;

		if( (addr=get_driver_pci_addr(st->osi_driver)) < 0 ) {
			printm("Could not find video PCI-slot!\n");
			return 0;
		}
		set_pci_dev_usr_info( addr, st );
		set_pci_base( addr, 0, 0xff000000, map_base_proc );
	}
	
	/* module initialization */
	offscreen_module.vinit( &offscreen_module );
	vmp = &st->all_modules;
	for( i=0; i<sizeof(video_mods)/sizeof(video_mods[0]); i++ ){
		if( !video_mods[i]->vinit( video_mods[i] ) ) {
			*vmp = video_mods[i];
			vmp = &(*vmp)->next;
		} else {
			// printm( "Video module '%s' not installed.\n", video_mods[i]->name );
		}
	}
	
	/* initialize the color table */
	st->color_table = malloc( 256 * sizeof(char) * 3 );

	printm("Video driver(s): ");
	for( p=st->all_modules; p; p=p->next )
		printm("[%s] ", p->name );
	printm("\n");

	/* prepare modes and open startup video mode */
	prepare_vmodes();
	if( st->n_mmodes <= 0 ) {
		printm("No video modes at all was found - exiting\n");
		exit(1);
	}

	/* set default mode / initalize mode from session */
	session_save_proc( save_video, NULL, kDynamicChunk );
	if( !loading_session() ) {
		st->cur_vmode_index = st->def_vmode;
		st->cur_depth_mode = st->def_depth_mode;
		set_vmode( st->mode_table[st->cur_vmode_index].vm[st->cur_depth_mode], NULL );
	} else {
		load_video();
	}

	/* possibly switch to the console */
	if( get_bool_res("start_on_console") == 1 && !debugger_enabled() )
		switch_to_fbdev_video();

	register_osi_iface( osi_iface, sizeof(osi_iface) );

	st->initialized = 1;
	st->macside_inited = !(is_oldworld_boot() || is_newworld_boot());

	/* XXX: Hack */
	if( is_elf_boot() ) {
		st->mbase = 0x81000000;
		st->fb_mapped = 1;
		map();
		printm("HACK: Mapping framebuffer to %08lx\n", st->mbase );
	}
	st->timer_id = new_ptimer( do_vbl_interrupt, NULL, 1 /* autoresume */  );
	restart_vbl();

	add_cmd( "vbl", "vbl \nToogle VBL interrupts on/off\n", -1, cmd_vbl );
	return 1;
}

static void
video_cleanup( void ) 
{
	video_module_t *m;
	int i,j;
	
	free_ptimer( vc->timer_id );
	
	if( vc->fb_mapped )
		_remove_mmu_mapping( &vc->fbmap );
	if( vc->cur_module )
		vc->cur_module->vclose();

	if( vc->color_table )
		free( vc->color_table );
	
	for( m=vc->all_modules; m; m=m->next )
		m->vcleanup( m );
	offscreen_module.vcleanup( &offscreen_module );

	for( i=0; i<vc->n_mmodes; i++ )
		for( j=0; j<vc->mode_table[i].num_depths; j++ )
			free( vc->mode_table[i].vm[j] );
	if( vc->mode_table )
		free( vc->mode_table );
}


driver_interface_t video_driver = {
    "video", video_init, video_cleanup
};
