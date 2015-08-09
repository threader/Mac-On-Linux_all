/* 
 *   Creation Date: <97/07/25 23:36:28 samuel>
 *   Time-stamp: <2003/06/02 12:25:53 samuel>
 *   
 *	<nvram.c>
 *	
 *	Emulation of the non-volatile RAM
 *   
 *   Copyright (C) 1997-2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include <sys/param.h>
#include "ioports.h"
#include "promif.h"
#include "res_manager.h"
#include "debugger.h"
#include "gc.h"
#include "driver_mgr.h"
#include "nvram.h"
#include "booter.h"
#include "session.h"
#include "os_interface.h"

/* 
   Hardware description based upon arch/ppc/pmac/support.c:
   
   If there is only one I/O area:
            static volatile unsigned char *nvram_data;
            nvram_data[(addr & 0x1fff) << 4]

   If there is two I/O areas, one is an address port:
            static volatile unsigned char *nvram_addr;
            static volatile unsigned char *nvram_data;
            *nvram_addr = addr >> 5
	     nvram_data[(addr & 0x1f) << 4]

   On Powerbooks (all or some of them) nvram is accessed 
   through the via-pmu. See the kernel for details.
 */

#define	NVRAM_SIZE		0x2000
#define	ADDR_PORT_SIZE		0x10
#define	DATA_PORT_SIZE		0x200

#define	PRAM_SIZE		0x100
#define	PRAM_OFFS		0x1300

/* --- variables --- */
static struct nvram_state {
	int			has_hw;

	gc_range_t		*addr_range;		/* NULL (one range only) */
	gc_range_t		*data_range;		/* NULL (one range only) */

	unsigned char		addr_reg;
	unsigned char		*data;
} ns[1];


/************************************************************************/
/*	I/O								*/
/************************************************************************/

static void
addr_io_write( ulong ptr, ulong data, int len, void *usr )
{
	/* both byte and word access is common */
	if( ptr & 0xf )
		printm("nvram: Strange addr_write\n");

	ns->addr_reg = data >> (len-1)*8;
}

static ulong 
addr_io_read( ulong ptr, int len, void *usr ) 
{
	printm("nvram: Isn't nvram_addr read only?\n");

	return ns->addr_reg << (len-1)*8;
}

static void 
addr_io_print( int isread, ulong addr, ulong data, int len, void *usr )
{
	/* suppress printing address write */
}

static ulong
data_io_read( ulong ptr, int len, void *usr ) 
{
	int offs, ret;

	if( ptr & 0xf || len !=1 )
		printm("nvram: Strange data access, ADDR = %08lX\n",ptr );

	/* 1 range if addr_base == NULL, otherwise 2 */
	if( ns->addr_range )
		offs = ((ptr-get_mbase(ns->data_range))>>4) + (ns->addr_reg<<5);
	else
		offs = (ptr-get_mbase(ns->data_range))>>4;
	

	if( offs > NVRAM_SIZE ) {
		printm("nvram: Outside nvram!\n");
		return 0;
	}

	ret = ns->data[offs] << (len-1)*8;
	/* printm("nvram read, ADDR %04X DATA %04X\n",offs,ret); */

	return ret;
}

static void
data_io_write( ulong ptr, ulong data, int len, void *usr ) 
{
	int offs;

	if( ptr & 0xf || len !=1 )
		printm("nvram: Strange data access, ADDR = %08lX\n",ptr );

	/* 1 range if addr_range == NULL, otherwise 2 */
	if( ns->addr_range )
		offs = ((ptr-get_mbase(ns->data_range))>>4) + (ns->addr_reg<<5);
	else
		offs = (ptr-get_mbase(ns->data_range))>>4;

	if( offs > NVRAM_SIZE ) {
		printm("nvram: Outside nvram!\n");
		return;
	}
	
	ns->data[offs] = data >> (len-1)*8;
	/* printm("nvram write, ADDR %04X DATA %04X\n",offs,nvram[offs]); */
}

static void
data_io_print( int is_read, ulong addr, ulong data, int len, void *usr )
{
	int offs;

	if( ns->addr_range )
		offs = ((addr-get_mbase(ns->data_range))>>4) + (ns->addr_reg<<5);
	else
		offs = (addr-get_mbase(ns->data_range))>>4;

	printm("nvram %s %04X: %02lX\n", is_read? "read " : "write", offs, data );
}


/************************************************************************/
/*	external access functions (will be removed eventually)		*/
/************************************************************************/

int
nvram_write( int offs, char *buf, int len )
{
	int n=0;

	if( offs >= 0 )
		n = MIN( NVRAM_SIZE-offs, len );
	if( n>0 )
		memcpy( ns->data + offs, buf, n );
	return n;
}

int
nvram_read( int offs, char *buf, int len )
{
	int n=0;

	if( offs >= 0 )
		n = MIN( NVRAM_SIZE-offs, len );
	if( n>0 )
		memcpy( buf, ns->data + offs, n );
	return n;
}

int
nvram_size( void )
{
	return NVRAM_SIZE;
}


/************************************************************************/
/*	read / write image						*/
/************************************************************************/

static int 
read_image( const unsigned char *name ) 
{
	int 	fd, ret=1;
	ssize_t	size;

	fd = open( (char *) name, O_RDONLY );
	if( fd == -1 ) {
		perrorm("nvram, read_image: open");
		return ret;
	}
	size = read( fd, ns->data, NVRAM_SIZE );
	if( size == -1 )
		perrorm("nvram, read_image: read");
	else if( size != NVRAM_SIZE )
		printm("WARNING: Could only read 0x%X byte from nvram-image\n", (int) size );
	else 
		ret=0;

	close( fd );
	return ret;
}

static void 
write_image( const unsigned char *name, int start, int len ) 
{
	int	fd;
	ssize_t	size;
	
	fd = open( (char *) name, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR );
	if( fd == -1 ) {
		perrorm("nvram, write_image: open");
		return;
	}
	if( lseek( fd, start, SEEK_SET ) == -1 )
		perrorm("lseek");
	size = write( fd, ns->data+start, len );
	if( size == -1 )
		perrorm("nvram, write_image: write");
	else if( size != len )
		printm("WARNING: Could only write 0x%X bytes to file\n", (int) size );
	close(fd);
}

/* offs -- byte */
static int
osip_read_nvram_byte( int sel, int *params )
{
	int offs = params[0];
	if( ns->data && offs>=0 && offs < NVRAM_SIZE )
		return ns->data[offs];
	return 0;
}

/* offs, byte -- */
static int
osip_write_nvram_byte( int sel, int *params )
{
	int offs = params[0];
	if( ns->data && offs>=0 && offs < NVRAM_SIZE )
		ns->data[offs] = params[1];
	return 0;
}

static int
osip_nvram_size( int sel, int *params )
{
	return NVRAM_SIZE;
}


/************************************************************************/
/*	debugger commands						*/
/************************************************************************/

/* nvram read image */
static int __dcmd
cmd_nvramri( int numargs, char** args)
{
	char *name;
	if( numargs != 1 && numargs!=2 )
		return 1;

	name = (numargs==2)? args[1] : gPE.nvram_image_name;
	if( !name ) {
		printm("Please specify image name\n");
		return 0;
	}

	printm("Reading nvram image from file '%s'\n",name );
	read_image( (unsigned char *) name );
	return 0;
}

/* nvram write image */
static int __dcmd
cmd_nvramwi( int numargs, char** args)
{
	char	*name;

	if( numargs != 1 && numargs !=2 )
		return 1;
       	name = (numargs==2)? args[1] : gPE.nvram_image_name;
	if( !name ) {
		printm("Please specify image file name\n");
		return 0;
	}
       	printm("Writing nvram image to file '%s'\n",name );
	write_image( (unsigned char *)name, 0, NVRAM_SIZE );
	return 0;
}

/* This function zaps some parts of the nvram. It seems necessary
 * to do this on a MacOS prepared image - not even OF is bootable
 * otherwise.
 */
static int __dcmd
cmd_fix_pram( int numargs, char **args )
{
	if( numargs != 1 )
		return 1;

	memset( ns->data+0x1000, 0, 0x300 );
	return 0;
}


/* nvram zap */
static int __dcmd
cmd_nvramzap( int numargs, char** args ) 
{
	if( numargs!=1 )
		return 1;

	printm("nvram zapped\n");
	memset( ns->data, 0, NVRAM_SIZE );
	return 0;
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static io_ops_t addr_ops={
	.read	= addr_io_read,
	.write	= addr_io_write,
	.print	= addr_io_print
};

static io_ops_t data_ops={
	.read	= data_io_read,
	.write	= data_io_write,
	.print	= data_io_print
};

static int
save_session_nvram( void )
{
	return write_session_data( "NVRM", 0, (char*)ns->data, NVRAM_SIZE );
}

static int
nvram_init( void )
{
	mol_device_node_t *dn = prom_find_devices( "nvram" );
	gc_bars_t bars;
	
	if( !dn || !is_gc_child(dn, &bars) ) {
		if( is_classic_boot() )
			printm("No nvram found in device tree\n");
	} else {
		if( bars.nbars == 1 ) {
			/* printm( "NVRAM has only one address\n" ); */
			ns->data_range = add_gc_range( bars.offs[0], bars.size[0],
						       "nvram data", 0, &data_ops, NULL );
		} else if( bars.nbars == 2 ) {
			if( bars.size[0] != ADDR_PORT_SIZE || bars.size[1] != DATA_PORT_SIZE )
				printm("Warning: unexpected size of nvram ranges\n");

			ns->addr_range = add_gc_range( bars.offs[0], ADDR_PORT_SIZE, 
						       "nvram addr", 0, &addr_ops, NULL );
			ns->data_range = add_gc_range( bars.offs[1], DATA_PORT_SIZE, 
						       "nvram data", 0, &data_ops, NULL );
		} else {
			printm("Don't know how to access NVRAM with %d addesses\n", bars.nbars );
			return 0;
		}
	}

	if( !(ns->data=calloc(NVRAM_SIZE,1)) ) {
		printm("nvram_init: Out of memory!\n");
		exit(1);
	}

	if( gPE.nvram_image_name ) {
		/* printm("Using nvram-image '%s'\n", gPE.nvram_image_name ); */
		if( read_image((unsigned char *)gPE.nvram_image_name) ) {
			printm("Creating nvram-image '%s'\n", gPE.nvram_image_name );
			write_image((unsigned char *) gPE.nvram_image_name, 0, NVRAM_SIZE );
		}
	}

	session_save_proc( save_session_nvram, NULL, kDynamicChunk );
	if( loading_session() ){
		if( read_session_data( "NVRM", 0, (char*)ns->data, NVRAM_SIZE ) )
			session_failure("Could not read nvram\n");
	}

	os_interface_add_proc( OSI_WRITE_NVRAM_BYTE, osip_write_nvram_byte );
	os_interface_add_proc( OSI_READ_NVRAM_BYTE, osip_read_nvram_byte );
	os_interface_add_proc( OSI_NVRAM_SIZE, osip_nvram_size );

	add_cmd( "nvramri", "nvramri [file]\nread nvram-image from file\n", -1, cmd_nvramri );
	add_cmd( "nvramwi", "nvramwi [file]\nwrite nvram-image to file\n", -1, cmd_nvramwi );
	add_cmd( "nvramzap", "nvramzap \nzap nvram to zero\n", -1, cmd_nvramzap );
	add_cmd( "fix_pram", "fix_pram \nzap some parts of pram (temporary solution)\n", -1, 
		 cmd_fix_pram );
	
	return 1;
}

static void 
nvram_cleanup( void )
{
	if( gPE.nvram_image_name ) {
#if 1
		if( is_oldworld_boot() ) {
			write_image((unsigned char *)gPE.nvram_image_name, PRAM_OFFS, PRAM_SIZE );
			write_image((unsigned char *)gPE.nvram_image_name, 0x1400, 0xc00 );
		} else
			write_image((unsigned char *)gPE.nvram_image_name, 0, NVRAM_SIZE );
#else
		write_image( gPE.nvram_image_name, 0, NVRAM_SIZE );
#endif
	}
	if( ns->data )
		free( ns->data );
}

driver_interface_t nvram_driver = {
    "nvram", nvram_init, nvram_cleanup
};

