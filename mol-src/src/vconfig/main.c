/* 
 *   Creation Date: <2000/10/18 20:30:30 samuel>
 *   Time-stamp: <2004/01/11 19:22:28 samuel>
 *   
 *	<main.c>
 *	
 *	Video mode prober
 *   
 *   Copyright (C) 2000, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include <stdarg.h>
#include <dirent.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/vt.h>
#include <linux/fb.h>
#include <linux/kd.h>

#include "res_manager.h"
#include "vmodeparser.h"

#define MODE_DIR	"vmodes"
#define ETC_FBMODES	"/etc/fb.modes"

/************************************************************************/
/*	S T A T I C							*/
/************************************************************************/

typedef struct {
	char 		*buf;
	int 		nlines;

	int		max_lines;
	int		buf_size;
} cbuf_t;

typedef struct {
	int		depth;
	int 		rowbytes;
	int		page_offs;
} depthinfo_t;

typedef struct console_vmode {
	struct fb_var_screeninfo var;
	int		ndepths;
	int		refresh;
	depthinfo_t	di[3];

	struct console_vmode *next;
} console_vmode_t;

static cbuf_t 		cbuf;
static int		vt_orig;
static int		vt_no;
static int		console_fd;
static int		fb_fd;
static console_vmode_t	*new_root;
static console_vmode_t	*old_root;
int			in_security_mode = 1;

static struct fb_var_screeninfo startup_var;

/* Ansi colors */
#define C_RED		"\33[1;31m"
#define C_GREEN 	"\33[1;32m"
#define C_YELLOW	"\33[1;33m"
#define C_NORMAL	"\33[0;39m"
#define C_COL_48	"\33[300D\33[61C"
#define C_COL_50	"\33[300D\33[62C"
#define C_COL_26	"\33[300D\33[33C"
#define C_COL_0		"\33[200D"

/************************************************************************/
/*	Console functions						*/
/************************************************************************/


static void 
release_vt( void )
{
	ioctl( console_fd, VT_ACTIVATE, vt_orig );
	ioctl( console_fd, VT_WAITACTIVE, vt_orig );
	close( console_fd );
}

static int 
grab_vt( void )
{
	struct vt_stat vts;
	int fd;
	char vtname[32];
	char vtname2[32];
	struct termios 	tio;	

	if( (fd=open("/dev/tty0", O_WRONLY)) < 0 &&
	     (fd=open("/dev/vc/0", O_WRONLY)) < 0 ) {
		fprintf(stderr, "failed to open /dev/tts0 or /dev/vc/0: %s\n", strerror(errno));
		return 1;
	}

	/* find original vt */
	if( ioctl(fd, VT_GETSTATE, &vts) < 0)
		return 1;
	vt_orig = vts.v_active;

	/* find free console? */
	if( (ioctl(fd, VT_OPENQRY, &vt_no) < 0) || (vt_no == -1) ) {
		fprintf(stderr, "Cannot find a free VT\n");
		return 1;
	}
	close(fd);

	/* detach old tty */
	setpgrp();
       	if( (fd=open("/dev/tty",O_RDWR)) >= 0 ) {
		ioctl(fd, TIOCNOTTY, 0);
		close(fd);
	}

	/* open our vt */
	sprintf(vtname,"/dev/tty%d",vt_no);
	if( (console_fd=open(vtname, O_RDWR) ) < 0 ) {
		sprintf(vtname2,"/dev/vc/%d",vt_no);
		if( (console_fd=open(vtname2, O_RDWR)) < 0 ) {
			fprintf(stderr, "Cannot open %s or %s (%s)\n",vtname, vtname, strerror(errno));
			return 1;
		}
	}
	printf("Running MOL video configurator on VT%d\n", vt_no );
#if 0
	/* set controlling tty */
	if( ioctl(console_fd, TIOCSCTTY, 1) < 0 ) {
		perror("Error setting controlling tty");
		return 1; 
	}
#endif

	/* No line buffering or echoing */
	tcgetattr( console_fd, &tio );
	tio.c_lflag &= ~(ICANON | ECHO);
	tcsetattr( console_fd, TCSANOW, &tio );

	/* Switch VT */
	ioctl( console_fd, VT_ACTIVATE, vt_no );
	ioctl( console_fd, VT_WAITACTIVE, vt_no );

	atexit( release_vt );
	return 0;
}


/************************************************************************/
/*	Console printing						*/
/************************************************************************/

static void
cprintf_(const char *fmt,... ) 
{
        va_list args;
	int len;
	char buf[512];

	if( console_fd == -1 )
		return;

	va_start( args, fmt );
	if( (len=vsnprintf(buf, sizeof(buf), fmt, args)) > 0 ) {
		if(write( console_fd, buf, len ) != len) {
			printf("Unable to write to the console fd\n");
			return;
		}
	}

	va_end( args );
}

static void 
crefresh( void ) 
{
	cprintf_("\33[H\33[2J" );
	if(write( console_fd, cbuf.buf, strlen(cbuf.buf) ) != strlen(cbuf.buf)) {
		printf("Unable to write to the console fd\n");
		return;
	}
}

static void
init_cprintf( void )
{
	cbuf.max_lines = startup_var.yres / 16;
	cbuf.buf_size = cbuf.max_lines * 300;
	cbuf.buf = malloc( cbuf.buf_size );
	cbuf.buf[0] = 0;
}

static void
cprintf(const char *fmt,... )
{
        va_list args;
	int len;
	char buf[512];
	int newoffs;
	
	if( console_fd == -1 || !cbuf.buf ) {
		fprintf(stderr, "cprintf called too early!\n");
		return;
	}

	va_start( args, fmt );
	newoffs = strlen( cbuf.buf );
	if( (len=vsnprintf(buf, sizeof(buf), fmt, args)) > 0 ) {
		char *p;
		int lines=0;
		
		buf[ sizeof(buf)-1 ] = 0;
		for( p=buf; (p=strchr(p, '\n')); lines++, p++ )
			;

		len++;
		while( len >= cbuf.buf_size - newoffs || cbuf.nlines + lines  > cbuf.max_lines ) {
			if( !(p=strchr(cbuf.buf, '\n')) ) {
				printf("Error\n");
				exit(1);
				/* error */
				newoffs = 0;
				buf[0] = 0;
				cbuf.nlines = 0;

			} else {
				/* drop first line */
				*p = 0;
				memmove( cbuf.buf, p+1, newoffs - ((int)p - (int)cbuf.buf) +1 );
				if( cbuf.nlines > 0 )
					cbuf.nlines--;
				newoffs = strlen(cbuf.buf);
				crefresh();
			}
		}
		cbuf.nlines += lines;

		strcpy( cbuf.buf + newoffs, buf );
		if(write(console_fd, cbuf.buf + newoffs, strlen(cbuf.buf + newoffs)) != strlen(cbuf.buf + newoffs)) {
			printf("Error writing to console fd!\n");
			exit(1);
		}
	}
	va_end( args );
}


static int 
yn_question_( char *query, int def, int o_opt )
{
	int again =1;
	char ch;
		
	cprintf("%s " C_YELLOW "[%c/%c%s]? ", query? query : "", def>0? 'Y' : 'y', !def? 'N' :'n',
		o_opt? ((def==-1)? "/Old" : "/old") : "");

	while( again-- ) {
		if( read(console_fd, &ch, 1) < 0 ) {
			perror("console read");
			exit(1);
		}

		switch( ch ) {
		case 'Y':
		case 'y':
			def = 1;
			break;
		case 'N':
		case 'n':
			def = 0;
			break;
		case '\n':
			break;
		case 'o':
		case 'O':
			if( o_opt ) {
				def = -1;
				break;
			}
		case 'Q':
		case 'q':
			cprintf(C_RED "\n*** Configure aborted ***\n");
			fprintf(stderr, "Configure aborted\n");
			sleep(1);
			exit(1);
			break;
		default:
			again = 1;
			break;
		}
	}
	ch = 'O';
	if( def > 0 )
		ch = 'Y';
	if( !def )
		ch = 'N';
	cprintf("%s%c", def? C_GREEN : C_RED, ch );
	return def;
}

static int
yno_question( char *query, int def )
{
	return yn_question_( query, def, 1 );
}

static int
yn_question( char *query, int def )
{
	return yn_question_( query, def, 0 );
}

static int
yn_question_nl( char *query, int def )
{
	int ret = yn_question_( query, def, 0 );
	cprintf("\n");
	return ret;
}



static int
accept_mode( void )
{
	char ch;
	if( read(console_fd, &ch, 1) < 0 ) {
		perror("console read");
		exit(1);
	}
	if( ch == 'Y' || ch == 'y' || ch == ' ' || ch== '\n' )
		return 1;
	if( ch == 'Q' || ch == 'q' )
		return -1;
	return 0;
}


/************************************************************************/
/*	Framebuffer functions						*/
/************************************************************************/

static void
close_fb( void )
{
	close( fb_fd );
}

static int
open_fb( void )
{
	char *name;

	if( !(name=getenv("FRAMEBUFFER")) )
		name = "/dev/fb0";
	if( (fb_fd=open(name, O_RDWR)) < 0 ) {
		printf("Error opening frame buffer device '%s'\n",name );
		return 1;
	}
	if( ioctl(fb_fd, FBIOGET_VSCREENINFO, &startup_var) ) {
		close(fb_fd);
		printf("Error obtaining current resolution\n");
		exit(1);
	}

	atexit( close_fb );
	return 0;
}

static inline int
std_depth( int depth )
{
        switch( depth ) {
        case 16:
                return 15;
        case 24:
                return 32;
        }
        return depth;
}

static void
pattern_8( char *p, int h, int v, int rb )
{
	char *p2;
	int x,y;
	
	for( y=0; y<v; y++, p += rb ) {
		char val = (float)y/(float)v * (float)0xff;
		for(p2=p, x=0; x<h; x++ )
			*p2++ = val;
	}
}
static void
pattern_15( char *p, int h, int v, int rb )
{
	short *p2;
	int x,y;
	
	for( y=0; y<v; y++, p += rb ) {
		short val = (char)((float)y/(float)v*0x1f) * 0x0421;
		for( p2=(short*)p, x=0; x<h; x++ )
			*p2++ = val;
	}
}
static void
pattern_32( char *p, int h, int v, int rb )
{
	ulong *p2;
	int x,y;
	
	for( y=0; y<v; y++, p += rb ) {
		ulong val = (char)((float)y/(float)v*0xff) * 0x010101;
		for( p2=(ulong*)p, x=0; x<h; x++ )
			*p2++ = val;
	}
}


static void
test_pattern( struct fb_var_screeninfo *var, struct fb_fix_screeninfo *fix, int depth )
{
	int pmask = getpagesize()-1;
	char *map_base;
	int offs = (int)fix->smem_start & pmask;
	int len = fix->smem_len + offs;
	int i, fd;
	short *ctable;
	struct fb_cmap cmap;	

	depth = std_depth( depth );
	if( len & pmask )
		len = (len & ~pmask) + (pmask+1);

	fd = open( "/dev/mem", O_RDWR );
	if( fd == - 1 ) {
		perror("open /dev/mem");
		return;
	}
	map_base = mmap( NULL, len, PROT_READ | PROT_WRITE,
			 MAP_SHARED, fd, ((int)fix->smem_start & ~pmask) );
	if( map_base == MAP_FAILED ) {
		perror("mmap");
		goto out;
	}

	/* palette */
	cmap.len = 256;
	ctable = (short*)calloc( cmap.len * 3, sizeof(short) );
	cmap.red = (unsigned short *)&ctable[0];
	cmap.green = (unsigned short *)&ctable[256];
	cmap.blue = (unsigned short *)&ctable[512];
	cmap.transp = NULL;
	cmap.start = 0;

	for( i=0; i<256; i++ ) {
		switch( depth ) {
		case 8:
			cmap.blue[i] = i * 0x100;
			break;
		case 15:
			cmap.red[i] = (i<<3) * 0x100;
			break;
		case 32:
			cmap.green[i] = i * 0x100;
			break;
		}
	}
	if( ioctl(fb_fd, FBIOPUTCMAP, &cmap) )
		perror("FBIOPUTCMAP");
	free( ctable );

	switch( depth ) {
	case 15:
		pattern_15( map_base + offs, var->xres, var->yres, fix->line_length );
		break;
	case 32:
		pattern_32( map_base + offs, var->xres, var->yres, fix->line_length );
		break;
	case 8:
		pattern_8( map_base + offs, var->xres, var->yres, fix->line_length );
		break;
	}
 out:
	munmap( map_base, len );
	close(fd);
}


/************************************************************************/
/*	Mode Utils							*/
/************************************************************************/

static int 
cmp_var( struct fb_var_screeninfo *v1, struct fb_var_screeninfo *v2 )
{
	#define CMP( field )	(v1->field == v2->field)
	int ret = 
		CMP(xres) && CMP(yres) 
		&& CMP(xres_virtual) && CMP( yres_virtual ) &&
		CMP(xoffset) && CMP(yoffset) &&
		CMP(pixclock) && CMP(left_margin) && CMP(right_margin) && 
		CMP(upper_margin) && CMP(lower_margin) && CMP(hsync_len) && CMP(vsync_len) && 
		CMP(sync) && CMP(vmode);
	return !ret;
}

static console_vmode_t *
find_video_mode( struct fb_var_screeninfo *var, console_vmode_t *root )
{
	console_vmode_t *vm;
	
	for( vm=root; vm; vm=vm->next )
		if( !cmp_var( var, &vm->var ) )
			return vm;
	return NULL;
}

static void
free_mode( console_vmode_t *mode )
{
	console_vmode_t **cv2;
	int i;
	
	for(i=0; i<2; i++ ) {
		cv2 = i? &new_root : &old_root;
		for( cv2 = &old_root; *cv2; cv2=&(**cv2).next ) {
			if( *cv2 == mode ) {
				*cv2 = mode->next;
				free( mode );
				return;
			}
		}
	}
	printf("free_mode: mode missing\n");
}

static void
add_video_mode( struct fb_var_screeninfo *var, int ndepths, depthinfo_t di[3], ulong refresh )
{
	console_vmode_t *vm;
	int i;

	vm = calloc( 1, sizeof(console_vmode_t) );
	vm->var = *var;
	vm->ndepths = ndepths;
	for(i=0; i<ndepths; i++ )
		vm->di[i] = di[i];
	vm->refresh = refresh;

	vm->next = new_root;
	new_root = vm;

	/* delete mode from old list if present */
	while( (vm=find_video_mode(var, old_root)) )
		free_mode( vm );
}

static int
save_new_modes( char *file )
{
	FILE *f;
	int i;
	console_vmode_t *cv;
	
	if( !(f=fopen(file, "w+")) ) {
		fprintf(stderr, "Failed to create the video configuration file '%s'\n", file );
		cprintf("Failed to create the video configuration file '%s'\n", file );
		return 1;
	}
	fprintf( f, "# Version 1\n");
	fprintf( f, "# Mac-on-Linux Video Mode Configuration\n#\n");
	fprintf( f, "# DO NOT EDIT THIS FILE BY HAND\n#\n");
	fprintf( f, "# Created by 'molvconfig'\n");
	
	for( cv=new_root; cv; cv=cv->next ) {
		struct fb_var_screeninfo *v = &cv->var;
		fprintf( f, "#\n");
		fprintf( f, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d\n",
			 v->xres, v->yres, v->xres_virtual, v->yres_virtual, v->xoffset, v->yoffset,
			 v->pixclock, v->left_margin, v->right_margin, v->upper_margin, v->lower_margin,
			 v->hsync_len, v->vsync_len, v->sync, v->vmode );
		fprintf( f, "%d %d %d %d %d %d %d %d %d %d %d\n", cv->ndepths, cv->refresh, 
			 cv->di[0].depth, cv->di[0].rowbytes, cv->di[0].page_offs,
			 cv->di[1].depth, cv->di[1].rowbytes, cv->di[1].page_offs,
			 cv->di[2].depth, cv->di[2].rowbytes, cv->di[2].page_offs );

		/* User info */
		printf(" %4d * %4d,  %02d.%02d Hz,  Depths: ", v->xres, v->yres, cv->refresh>>16, ((cv->refresh*10)>>16)%10 );
		for(i=0; i<cv->ndepths; i++ )
			printf(" %d", cv->di[i].depth );
		printf("\n");
	}

	fclose(f);
	return 0;
}

static int
read_def_modes( char *file )
{
	struct fb_var_screeninfo v;
	console_vmode_t cv;
	int done=0, err, line;
	char buf[200];
	FILE *f;
	
	if( !(f=fopen(file, "r")) )
		return 1;

	for( line=0; !done; ) {
		do {
			if( !fgets(buf, sizeof(buf), f) )
				done=1;
			line++;
		} while( !done && buf[0] == '#' );
		if( done )
			break;

		err = sscanf(buf, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d", 
			     &v.xres, &v.yres, &v.xres_virtual, &v.yres_virtual, &v.xoffset, &v.yoffset,
			     &v.pixclock, &v.left_margin, &v.right_margin, &v.upper_margin, &v.lower_margin,
			     &v.hsync_len, &v.vsync_len, &v.sync, &v.vmode ) != 15;
		if( !err ) {
			line++;
			if( !fgets( buf, sizeof(buf), f ))
				err = 1;
		}
		if( !err ) {
			err = sscanf(buf, "%d %d %d %d %d %d %d %d %d %d %d", &cv.ndepths, &cv.refresh, 
				     &cv.di[0].depth, &cv.di[0].rowbytes, &cv.di[0].page_offs,
				     &cv.di[1].depth, &cv.di[1].rowbytes, &cv.di[1].page_offs,
				     &cv.di[2].depth, &cv.di[2].rowbytes, &cv.di[2].page_offs ) != 11;
		}
		if( !err ) {
			console_vmode_t *new_cv = malloc(sizeof(console_vmode_t));
			cv.var = v;
			*new_cv = cv;
			new_cv->next = old_root;
			old_root = new_cv;
		}
		if( err )
			fprintf(stderr, "Line %d in '%s' is malformed.\n", line, file );
	}

	fclose( f );
	return 0;
}


/************************************************************************/
/*	Mode Probing							*/
/************************************************************************/

static int
probe_mode( struct fb_var_screeninfo *org_var, ulong refresh )
{
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	int depths[] = {8,15,32};
	int d, i, success=1;
	depthinfo_t di[3];
	int ndepths = 0;
	int ret;

	/* Set video mode  */
	ioctl( console_fd, KDSETMODE, KD_GRAPHICS );

	cprintf("    " C_COL_50);
	d = depths[0];
	for( i=0; i<sizeof(depths)/sizeof(int); i++ ) {
		if( !success ) {
			cprintf(C_RED" BAD " );
			success=0;
		}
		d = depths[i];
		var = *org_var;
		var.bits_per_pixel = depths[i];

		var.activate = FB_ACTIVATE_NOW;

		success=0;
		if( ioctl(fb_fd, FBIOPUT_VSCREENINFO, &var) < 0 )
			continue;
		
		if( org_var->xres != var.xres || org_var->yres != var.yres 
		    || std_depth(depths[i]) != std_depth(var.bits_per_pixel) )
			continue;

		if( ioctl(fb_fd, FBIOGET_FSCREENINFO, &fix) < 0 ) {
			perror("FBIOGET_FSCREENINFO");
			continue;
		}
		/* the PBIOPUT above seems to do modify var anyway... */
		if( ioctl(fb_fd, FBIOGET_VSCREENINFO, &var) < 0 )
			continue;
		if( org_var->xres != var.xres || org_var->yres != var.yres 
		    || std_depth(depths[i]) != std_depth(var.bits_per_pixel) )
			continue;

		/* Does the user accept this mode? */
		test_pattern( &var, &fix, depths[i] );
		ret = accept_mode();
		if( ret > 0 ) {
			di[ndepths].depth = depths[i];
			di[ndepths].page_offs = (int)fix.smem_start & 0xfff;
			di[ndepths].rowbytes = fix.line_length;
			ndepths++;
			success=1;
			cprintf(C_GREEN" OK  " );
		}
		if( ret < 0 ) {
			cprintf( C_RED );
			for( ; i<sizeof(depths)/sizeof(int); i++ )
				cprintf(C_COL_50"------- ABORT -------");
			success = ret = -1;
			ndepths = 0;
			goto out;
		}
	}
	if( !success )
		cprintf(C_RED" BAD ");
	ret = 0;
 out:
	cprintf("\n");
	if( ioctl(fb_fd, FBIOPUT_VSCREENINFO, &startup_var) )
		fprintf(stderr, "Could not set original resolution!\n");
	ioctl( console_fd, KDSETMODE, KD_TEXT );

	if( ndepths > 0 )
		add_video_mode( org_var, ndepths, di, refresh );
	return ret;
}

static void
probe_file( char *file, int automatic ) 
{
	struct fb_var_screeninfo var;
	ulong refresh;
	char *name;
	int i, ponce=0;

	if( vmodeparser_init(file) ) {
		cprintf(C_RED"\nCould not read mode file\n"C_NORMAL);
		return;
	}
	for( i=0; !vmodeparser_get_mode(&var, i, &refresh, &name); i++ ) {
		console_vmode_t *oldvm = find_video_mode( &var, old_root );
		int depths[] = {8,15,32};
		int j,k, probe=0;

		if( find_video_mode(&var, new_root) )
			continue;
		if( automatic && !oldvm )
			continue;

		if( !ponce++ ) {
			cprintf( C_YELLOW C_COL_50 " 8    15   24\n" );
			cprintf( "================================================================================\n");
		}
		/* print old modes */
		if( oldvm ) {
			cprintf(C_YELLOW C_COL_50" ");
			for( j=0; j<sizeof(depths)/sizeof(int) ; j++ ) {
				int ok=0;
				if( j!=0 )
					cprintf("  ");
				for(k=0; k<oldvm->ndepths; k++ ) {
					if( oldvm->di[k].depth == depths[j] ) {
						cprintf(C_GREEN"OK ");
						ok=1;
					}
				}
				if(!ok)
					cprintf(C_RED"BAD");
			}
			cprintf(C_YELLOW C_COL_0);
		}
		
		cprintf(C_YELLOW " %s" C_NORMAL C_COL_26"%dx%d ", name, var.xres, var.yres  );
		cprintf("%d.%d Hz ", refresh>>16, ((refresh*10)>>16)%10 );

		if( oldvm ) {
			probe = automatic? -1 : yno_question( NULL, -1 );
		} else
			probe = yn_question( NULL, 0 );

		/* remove discarded mode from old list */
		if( !probe && oldvm ) {
			free_mode( oldvm );
			cprintf( C_COL_48 "                 ");
		}
		if( probe > 0 ) {
			probe_mode( &var, refresh );
			crefresh();
		} else {
			cprintf("\n");
		}
		if( probe < 0 )
			add_video_mode( &var, oldvm->ndepths, oldvm->di, refresh ); 
	}
	if( ponce )
		cprintf( C_YELLOW "================================================================================");
	cprintf("\n");
	vmodeparser_cleanup();
}

/************************************************************************/
/*	F U N C T I O N S						*/
/************************************************************************/


static int 
suffixmatch( char *str, char *suffix ) 
{
	char *p;
	p = strstr( str, suffix );
	if( p && !*(p + strlen(suffix)) )
		return 1;
	return 0;
}

int
main( int argc, char **argv )
{
	char *config_file, *mode_dir, *etc_file;
	struct dirent *de;
	int tmpfd;
	char libdir[100];
	DIR *dir;
	
	if( !getuid() )
		in_security_mode = 0;
	if( seteuid(0) != 0 ) {
		fprintf( stderr, "The MOL video configurator must be run as root!\n");
		exit(1);
	}

	res_manager_init( 0, argc, argv );
	atexit( res_manager_cleanup );
	getcwd( libdir, sizeof(libdir) );

	memset( &cbuf, sizeof(cbuf), 0 );

	if( !(config_file=get_filename_res("fbdev_prefs")) )
		missing_config_exit("fbdev_prefs");

	mode_dir = MODE_DIR;
	if( !(etc_file=get_filename_res("fb_modes")) )
		etc_file = ETC_FBMODES;
#if 0
	if( fork() != 0 ) {
		sleep(10);
		return 0;
	}
	if( setsid() < 0 )
		perror("setsid failed!\n");
#endif
	if( grab_vt() || open_fb() )
		return 1;
	init_cprintf();

	cprintf("\33]R");		/* reset palette */
	cprintf("\33[H\33[2J" );      	/* clear screen */

	cprintf("\n" C_RED
		"********************************************************************************\n"
		"*               Mac-on-Linux Fullscreen Video Configurator                     *\n"
		"********************************************************************************\n"
		C_NORMAL );
	cprintf("\nWhen a video mode is tested, press '" C_GREEN "Y" C_NORMAL"' if the mode "
		"is working and '" C_RED "N" C_NORMAL"' otherwise.\n"
		"The test screen is a color gradient. To return to this screen, three key\n"
		"presses are necessary since all depths are tested consecutively. Press '"C_YELLOW"Q"C_NORMAL"' at\n"
		"any time to abort.\n\n" );
	if( !read_def_modes( config_file ) ) {
		if( yn_question_nl(C_YELLOW "Remove previously configured modes?", 0 ) ) {
			console_vmode_t *next;
			while( old_root ) {
				next = old_root->next;
				free( old_root );
				old_root = next;
			}
		}
	}

	if( !(dir=opendir(mode_dir)) ) {
		fprintf(stderr, "opendir '%s': %s\n", mode_dir, strerror(errno) );
		cprintf(C_RED "opendir '%s': %s\n", mode_dir, strerror(errno) );
	} else {
		cprintf("Reading video mode files in %s\n\n", mode_dir );
	}
	/* As a last fallback, probe the startup resolution */
	if( yn_question(C_GREEN "Probe current mode?", 1) ) {
		probe_mode( &startup_var, 0 );
		crefresh();
	}
	cprintf("\n");

	while( dir && (de=readdir(dir)) != NULL ) {
		char buf[ strlen(de->d_name) + strlen(mode_dir) + 2];
		if( !suffixmatch(de->d_name, ".mode") && !suffixmatch(de->d_name, ".modes") )
			continue;
		strcpy( buf, mode_dir );
		strcat( buf, "/" );
		strcat( buf, de->d_name );

		cprintf( C_GREEN "Probe modes in '%s'" C_NORMAL, de->d_name );
		if( yn_question(NULL, 1) )
			probe_file( buf, 0 );
		else
			probe_file( buf, 1 );
	}
	if( dir )
		closedir( dir );

	if( (tmpfd=open(etc_file, O_RDONLY)) > 0 ) {
		close(tmpfd );

		cprintf( C_GREEN "Probe video modes in '%s'" C_NORMAL, etc_file );
		if( yn_question(NULL, 1) )
			probe_file( etc_file, 0 );
		else
			probe_file( etc_file, 1 );
		cprintf("\n");
	}

	/* Add any video modes still in the old list (unprobed) */
	if( old_root ) {
		console_vmode_t *vp;
		cprintf("Additional modes configured earlier:\n");
		for( vp=old_root ;; vp=vp->next) {
			cprintf(C_YELLOW " %dx%d ", vp->var.xres, vp->var.yres  );
			cprintf("%d.%d Hz  \n", vp->refresh>>16, ((vp->refresh*10)>>16)%10 );
			if( !vp->next )
				break;
		}
		vp->next = new_root;
		new_root = old_root;
		old_root = NULL;
	}
	cprintf("\n");

	cprintf(C_GREEN "Writing config file %s\n", config_file);
	fprintf(stderr, "Writing config file %s\n", config_file);
	save_new_modes( config_file );

	cprintf("\n" C_RED
		"********************************************************************************\n"
		"*                                    D O N E                                   *\n"
		"********************************************************************************\n"
		C_NORMAL );
	sleep(2);

	return 0;
}
