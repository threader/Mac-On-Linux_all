/* 
 *   Creation Date: <97/07/27 14:45:03 samuel>
 *   Time-stamp: <2003/06/02 13:45:43 samuel>
 *   
 *	<swim3.c>
 *	
 *	Driver for the SWIM3 (Super Woz Integrated Machine 3)
 *	floppy-controller.
 *
 *	Information about the SWIM3 chip has been obtained
 *	from /drivers/block/swim3.c and Inside Macintosh vol III
 *
 *	WANTED: A description of the hardware.
 *   
 *   Copyright (C) 1998, 1999, 2000, 2002, 2003, 2004 Samuel Rydh
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <pthread.h>
  
#include "memory.h"
#include "ioports.h"
#include "promif.h"
#include "dbdma.h"
#include "res_manager.h"
#include "debugger.h"
#include "thread.h"
#include "pic.h"
#include "extralib.h"
#include "driver_mgr.h"
#include "gc.h"
#include "booter.h"
#include "swim_hw.h"


/* IMPLEMENTATION NOTES:
 *    - write is not implemented
 *    - use of drive A & drive B simultaneously doesn't work
 */

/* #define VERBOSE_CYLINDER */
/* #define DEBUG_SWIM   */

static char *reg_names[] = {
	"DATA  ", "USECS ", "ERROR ", "MODE  ", "SELECT", "REG5  ", "CNTRL ",
	"STATUS", "INTR  ", "NSEEK ", "CTRACK", "CSECT ", "SSIZE ", "SECTOR",
	"NSECT ", "INT_EN"
};
static char *control_bit_names[] = {
	"INTR_ENABLE", "DRIVE_ENABLE", "SEC_DRIVE_ENABLE(?)", "SCAN_TRACK",
	"WRITE_SECTORS", "SELECT", "**BIT 1**", "DO_SEEK"
};
static char *intr_bit_names[] = {
	"**BIT 7**", "SEEK_DONE", "SEEN_SECTOR", "TRANSFER_DONE",
	"DATA_CHANGED", "ERROR", "**BIT 2**", "**BIT 1**"
};
static char *read_status_names[] = {
	"STEP_DIR(?)", "STEPPING(?)", "MOTOR_ON(?)", "RELAX",
	"READ_DATA_0(?)", "--5--(?)", "SINGLE_SIDED", "DRIVE_PRESENT",
	"DISK_IN", "WRITE_PROT", "TRACK_ZERO", "TACH0", 
	"READ_DATA_1", "--13--(?)", "SEEK_COMPLETE", "HIGH_DENSITY"
};
static char *action_names[] = {
	"*SEEK_POSITIVE", "*STEP", "*MOTOR_ON", "*--3--(?)",
	"*SEEK_NEGATIVE", "*--5--(?)", "*MOTOR_OFF", "*EJECT"
};

typedef struct floppy_drive {			/* FLOPPY DRIVE STATE */
	int		fd;

	/* abs_sector = (ctrack * secpercyl + secpertrack * cur_head + sector ) */
	int		cur_head;		/* head (0,1) */
	int		cur_track;
	
	int		write_prot;		/* 1 if write protected */
	int		disk_in;		/* 1 if disk in drive (?) */
	int		single_sided;		/* 1 if single_sided */
	int		motor_on;
	int		seek_direction;		/* +1 or -1 */
	
	int		relaxed;		/* idle flag */
} floppy_drive_t;

static struct {					/* CONTROLLER STATE */
	/* interrupt and range information */
	int		swim_irq, dma_irq;
	gc_range_t	*regs_range;

	pthread_mutex_t	lock_mutex;
	int		control_pipe[2];
	volatile int	exited;
	int		xfering;		/* 1 when xfering data */
	
	/* disk geometry information */
	int		secpercyl;		/* 36 */
	int		secpertrack;		/* 18 */
	int		total_secs;		/* 2880 */

	/* drives */
	floppy_drive_t 	*drive;			/* always points to one of the drivers */
	const char	*drive_name;
	floppy_drive_t	drive_table[2];
	
	/* registers */
	unsigned char	reg[16];
} fs[1];

#define LOCK		pthread_mutex_lock( &fs->lock_mutex );
#define UNLOCK		pthread_mutex_unlock( &fs->lock_mutex );


static void
init_drive( floppy_drive_t *dr, char *image_name, int drive_num )
{
	/* close old image */
	if( dr->fd && dr->fd !=-1 )
		close( dr->fd );
	
	memset( dr, 0, sizeof(floppy_drive_t) );

	/* Defaults for an empty drive */
	dr->write_prot = 1;
	dr->relaxed = 1;
	dr->fd = -1;

	if( !image_name )
		return;
	if( (dr->fd = open(image_name, O_RDONLY)) == -1  ) {
		printm("Could not open '%s'\n", image_name );
		return;
	}
	printm("Floppy image '%s' used for drive %s.\n",image_name, drive_num ? "B" : "A" );
	dr->disk_in = 1;

	dr->write_prot = 1;	/* write not implemented... */
	dr->relaxed = 1;
	dr->seek_direction = 1;
	dr->single_sided = 0;
}



/************************************************************************/
/*	register I/O							*/
/************************************************************************/

static void 
fix_irq( void )
{
	/* completely untested... */

	if( fs->reg[ r_intr ] & fs->reg[ r_intr_enable ] ) {
#ifdef DEBUG_SWIM
		printm("Raising SWIM interrupt\n");
#endif
		irq_line_hi( fs->swim_irq ); 
	} else {
#ifdef DEBUG_SWIM
		printm("Lowering SWIM interrupt\n");
#endif
		irq_line_low( fs->swim_irq );
	}
}

static void 
set_cur_drive( void )
{
	if( fs->reg[r_control] & SEC_DRIVE_ENABLE ) {
		fs->drive = &fs->drive_table[1];
		fs->drive_name = "B";
	} else {
		fs->drive = &fs->drive_table[0];
		fs->drive_name = "A";
	}
}

static void 
reg_io_write( ulong addr, ulong data, int len, void *usr )
{
	int val, reg = (addr - get_mbase(fs->regs_range))>>4;
	char old;

	if( len!=1 || addr & 0xf ) {
		printm("SWIM3: Unsupporten access of SWIM\n");
		return;
	}
	LOCK;

	old = fs->reg[reg];
	fs->reg[reg] = data;

	switch( reg ) {
	case r_data:
		break;
	case r_usecs:		/* counts down at 1MHz */
		break;		
	case r_error:
	case r_mode:
		break;
	case r_select:		/* controls CA0, CA1, CA2 and LSTRB signals */
		/* if LSTRB goes LO -> HI, then we do an action */
		/* specified by CA0 -> CA2 */
		if( (old^data) & data & LSTRB ) {
			switch( data & CA_MASK ) {
			case SEEK_POSITIVE: /* 0 */
				fs->drive->seek_direction = 1;
				break;
			case STEP: /* 1 */
				break;
			case MOTOR_ON: /* 2 */
				fs->drive->motor_on = 1;
				break;
			case 3:  /* (?) */
				break;
			case SEEK_NEGATIVE: /* 4 */
				fs->drive->seek_direction = -1;
				break;
			case 5:	/* (?) */
				break;
			case MOTOR_OFF: /* 6 */
				fs->drive->motor_on = 0;
				break;
			case EJECT: /* 7 */
				printm("*** FLOPPY %s EJECTED ***\n",fs->drive_name );
				fs->drive->disk_in = 0;
				break;
			}
		} 
		/* should we only do this when LSTRB is zero? */
		val = data & CA_MASK;
		if( fs->reg[r_control] & SELECT )
			val |= 0x8;
		switch( val ) {
		case READ_DATA_0:
			fs->drive->cur_head=0;
			break;
		case READ_DATA_1:
			fs->drive->cur_head=1;
			break;
		default:  /* more relevant bits? */
			break;
		}
		break;
	case r_reg5:
		break;
	case r_control:		/* writing bits clears them */
		fs->reg[r_control] = old & ~data;
		set_cur_drive();
		break;
	case r_status:		/* writing bits sets them in control */
		fs->reg[r_control] |= data;	/* was = old | data (????) */
		fs->reg[r_status] = old;
		set_cur_drive();

		if( data & DO_SEEK ) {
			int new_track = fs->reg[r_ctrack];
			new_track += fs->reg[r_nseek] * fs->drive->seek_direction;
			if( new_track<0 )
				new_track=0;
			if( new_track >= fs->total_secs / fs->secpercyl ) {
				/* possibly we should set 0xff instead */
				new_track=fs->total_secs / fs->secpercyl -1;
			}
			fs->reg[r_ctrack] = new_track;
			fs->drive->cur_track = new_track;
			fs->reg[r_nseek] = 0;
			fs->reg[r_intr] |= SEEK_DONE;
			fix_irq();
		}
		if( data & SELECT ) {
		}
		if( data & WRITE_SECTORS ) {
		}
		if( data & SCAN_TRACK ) {
			fs->reg[r_ctrack] = fs->drive->cur_track;
			fs->xfering = (fs->reg[r_nsect] > 0)? 1:0;
			fs->reg[r_intr] |= SEEN_SECTOR;
			fix_irq();
			if( fs->xfering ) {
				char ch=1;
				write( fs->control_pipe[1], &ch, 1 );
			}
		} else {
			fs->xfering = 0;
		}
		if( data & DRIVE_ENABLE ) {
		}
		if( data & INTR_ENABLE ) {
		}
		break;
	case r_intr:
		/* read only register (I think) */
		fs->reg[r_intr] = old;
		break;		
	case r_ctrack:		/* current track number */
	case r_csect:		/* current sector number */
		printm("SWIM: Write to read only register\n");
		fs->reg[reg] = old;
		break;
	case r_nseek:	   	/* # tracks to seek */
	case r_sector:		/* sector # to read or write */
	case r_nsect:		/* # sectors to read or write */
		break;
	case r_ssize:		/* sector size code?? */
		/* MacOS writes 0 to this register... but it still reads 2 */
		fs->reg[r_ssize] = old;
		break;
	case r_intr_enable:
		fix_irq();
		break;
	}
	UNLOCK;
}

static ulong 
reg_io_read( ulong addr, int len, void *usr )
{
	static int csect_next=0;
	int reg = (addr - get_mbase(fs->regs_range))>>4;
	int ret, val, data;

	if( len!=1 || addr & 0xf ) {
		printm("SWIM3: Unsupporten access of SWIM\n");
		return 0;
	}
	LOCK;
	
	ret = fs->reg[reg];
	
	switch( reg ) {
	case r_data:
		break;
	case r_usecs:		/* counts down at 1MHz */
		/* We don't need any delays */
		ret = 0;
		break;
	case r_error:
		/* A read clears the register and the the ERROR interrupt */
		fs->reg[r_error] = 0;
		fs->reg[r_intr] &= ~ERROR;
		fix_irq();
		break;
	case r_mode:
	case r_select:		/* controls CA0: CA1: CA2 and LSTRB signals */
	case r_reg5:
	case r_control:
		break;
	case r_status:
		val = (fs->reg[r_select] & CA_MASK);
		val |= (fs->reg[r_control] & SELECT ) ? 0x8 : 0;
		data = 0;
		ret = 0;

		switch( val ) {
		case DISK_IN:
			data = fs->drive->disk_in;
			break;
		case WRITE_PROT:
			data = fs->drive->write_prot;
			break;
		case TRACK_ZERO:
			data = fs->drive->cur_track == 0;
			break;
		case SEEK_COMPLETE:
			data = 1;
			break;
		case RELAX:
			data = fs->drive->relaxed;
			break;
		case DRIVE_PRESENT:
			data = 1; /* both drives are present */
			break;
		case HIGH_DENSITY:
			data = 1;
			/* if no disk is in the drive this should still read 1 */
			break;
		case SINGLE_SIDED:
			data = fs->drive->single_sided;
			break;
		case STEPPING:
			data = 0;
			break;
		case MOTOR_ON:
			data = fs->drive->motor_on;
			break;
		case READ_DATA_0: /* read data 0 (?) */
			data = 0;
			break;
		case READ_DATA_1: /* read data 1 (?) */
			data = 0;
			break;
		case 5:		/* ? */
			data = 0;
			break;
		case 13:	/* ? */
			/* if zero... action *5 is issued... */
			data = 1;
			break;
		default:
			data = 1;
			printm("SWIM: Unimplemented status %d\n", val );
			break;
		}
		/* NOTE: 0=true, 1=false for DATA bit  */
		/* bit 4 seems essential - Mac OS reads it */
		ret |= data ? 0 : (DATA | 0x4);
		ret |= 0x2;	/* empirically */
		break;
	case r_intr:
		/* reading this register clears it */
		fs->reg[r_intr]=0;
		fix_irq();
		break;
	case r_ctrack:		/* current track number */
		/* ret = fs->drive->cur_track;*/
		if( fs->drive->cur_head )
			ret |= 0x80;
		break;
	case r_nseek:	   	/* # tracks to seek */
	case r_ssize:		/* sector size code?? */
	case r_sector:		/* sector # to read or write */
	case r_nsect:		/* # sectors to read or write */
	case r_intr_enable:
		break;
	case r_csect:		/* sector which just passed the head */
		ret = csect_next++ % fs->secpertrack;
		break;
	}
	
	UNLOCK;
	return ret;
}


/************************************************************************/
/*	DMA								*/
/************************************************************************/

static void
dma_read( void )
{
	printm("SWIM3: Output to disk is not implemented\n");
}

static void 
dma_write( void )
{
	int abs_sector, num_sect, count;
	floppy_drive_t *dr = fs->drive;
	struct dma_write_pb pb;
	ssize_t	ret;
	char *buf;

#if defined(VERBOSE_CYLINDER)
	static int lasttrack=-1;
	if( dr->cur_track != lasttrack ) {
		lasttrack = dr->cur_track;
		printm("*** FLOPPY %s, CYL: %d ***\n", fs->drive_name, lasttrack );
	}
#endif
	if( dma_write_req( fs->dma_irq, &pb ) )
		return;
	
#if defined(DEBUG_SWIM)
	printm("SWIM, dma_write. req_count %04X\n", pb.req_count );
	printm("*** CYLINDER: %d, SECTOR: %d, HEAD: %d ***\n",
	       dr->cur_track, fs->reg[r_sector], dr->cur_head );
#endif	
	if( fs->reg[r_sector] == 0) {
		printm("SWIM ERROR: SECTOR == 0!\n");
		fs->reg[r_sector] = 1;
	}

	abs_sector = dr->cur_track * fs->secpercyl
			+ fs->secpertrack * dr->cur_head + fs->reg[r_sector] -1;
	num_sect = fs->reg[r_nsect];
	count = num_sect * 512;

#ifdef DEBUG_SWIM
	printm("READING %d sectors starting at sector %d\n", num_sect, abs_sector );
#endif
	if( dr->fd != -1 ) {
		if( lseek( dr->fd, abs_sector * 512, SEEK_SET )==-1) {
			perrorm("swim3, seek");
			return;
		}

		buf = malloc( count );
		ret = read( dr->fd, buf, count );

		if( ret==-1 )
			perrorm("read");

		dma_write_ack( fs->dma_irq, buf, count, &pb );
		free( buf );

		fs->reg[ r_nsect ] -= num_sect;
		if( !fs->reg[r_nsect] ) {  /* was pb.is_last */
			fs->xfering = 0;
			fs->reg[ r_intr ] |= TRANSFER_DONE;
			fix_irq();
		}
	}
}

static void
dma_entry( void *dummy )
{
	int val=0;
	char ch;

	LOCK;
	for( ;; ) {
		/* first... wait upon SWIM controller to assign us a task */
		while( !fs->xfering ) {
			UNLOCK;
			while( read(fs->control_pipe[0], &ch, 1) < 0 )
				;
			if( !ch ) {
				fs->exited = 1;
				return;
			}
			LOCK;
			val = dma_wait(fs->dma_irq, DMA_RW, NULL);
		}
		if( val == DMA_READ )
			dma_read();
		else if( val == DMA_WRITE )
			dma_write();
	}
	UNLOCK;
	fs->exited = 1;
}


/************************************************************************/
/*	verbose output							*/
/************************************************************************/

static void 
dump_control( int ctrl  )
{
	int i=0;
	
	while( ctrl ) {
		if( ctrl & 0x1 )
			printm( " %s ", control_bit_names[i] );
		i++;
		ctrl = ctrl>>1;
	}
}

static void 
reg_io_print( int isread, ulong addr, ulong data, int len, void *usr )
{
	int reg = (addr - get_mbase(fs->regs_range))>>4;

	/* suppress usecs */
	if(reg == r_usecs )
		return;

	printm("swim3: %s %s %02lX", reg_names[reg],
	       isread ? "read: " : "write:", data );

	if( !isread && (reg == r_status || reg == r_control ) ) {
		printm( "   %s ", (reg==r_status) ? "+" : "-" );
		dump_control( data );
		printm( "     [");
		dump_control( fs->reg[r_control] );
		printm( "] ");
	}
	
	if( isread && (reg == r_status )) {
		int val = fs->reg[r_select] & CA_MASK;
		if( fs->reg[r_control] & SELECT )
			val |= 0x8;
		printm( "    %s %d", read_status_names[val], data&DATA ? 0:1 );
	}

	if( isread && (reg == r_intr) ) {
		int i=0, d=data & 0xff;
		while( d ) {
			if( d & 0x1 )
				printm( " %s ", intr_bit_names[i] );
			i++;
			d = d>>1;
		}
	}

	/* action - when LSTRB of r_select goes hi (I think) */
	if( !isread && reg==r_select && (fs->reg[r_select] & LSTRB ) )
		printm(" %s ", action_names[ fs->reg[r_select] & CA_MASK ]);

	printm("\n");
}


/************************************************************************/
/*	CMDs & DEBUG							*/
/************************************************************************/

static void __dcmd
dump_registers( void )
{
	int i;

	dump_control( fs->reg[r_control] );
	printm("\n");
	for(i=0; i<NUM_SWIM_REGS; i++ ) {
		printm("%s %02X  ", reg_names[i], fs->reg[i] );
		if( i%8 == 7 )
			printm("\n");
	}
}

/* display registers */
static int __dcmd
cmd_swim_dr( int numargs, char **args ) 
{
	if( numargs!=1 )
		return 1;
	dump_registers();
	return 0;
}

static int __dcmd
cmd_swim_irq( int argc, char **argv )
{	
	if( argc!=2 )
		return -1;
	if( string_to_ulong( argv[1] ) )
		irq_line_hi( fs->swim_irq );
	else
		irq_line_low( fs->swim_irq );
	return 0;
}

/* swim insert floppy */
static int __dcmd
cmd_ins_floppy( int argc, char **argv )
{
	floppy_drive_t *dr;
	int ind =0;
	if( argc!=2 && argc!=3 )
		return -1;
	if( argc==3 && !strcasecmp( argv[2], "B" ))
		ind=1;
	dr = &fs->drive_table[ind];

	if( dr->disk_in ) {
		printm("There is already a disk in drive %s\n",ind ? "B":"A");
		return 0;
	}
	init_drive( dr, argv[1], ind );
	return 0;
}


/************************************************************************/
/*	init								*/
/************************************************************************/

static io_ops_t ops = {
	.read	= reg_io_read,
	.write	= reg_io_write,
	.print	= reg_io_print
};

static int
swim3_init( void ) 
{
	mol_device_node_t *dn;
	gc_bars_t bars;
	irq_info_t irqinfo;
	
	/* floppy doesn't work with newworld ROMs... */
	if( !is_oldworld_boot() )
		return 0;

	pthread_mutex_init( &fs->lock_mutex, NULL );

	if( !(dn=prom_find_devices("swim3")) && !(dn=prom_find_type("swim3")) ) {
		printm("No swim3 floppy controller found\n");
		return 0;
	}
	if( !is_gc_child(dn,&bars) || bars.nbars != 2 || prom_irq_lookup(dn,&irqinfo) != 2 ) {
		printm("swim3: Expecting 2 addrs and 2 intrs\n");
		return 0;
	}

	pipe( fs->control_pipe );
	fs->swim_irq = irqinfo.irq[0];
	fs->dma_irq = irqinfo.irq[1];
	fs->regs_range = add_gc_range( bars.offs[0], NUM_SWIM_REGS * SWIM_REG_SIZE,
				       "swim3_regs", 0, &ops, NULL );

	allocate_dbdma( bars.offs[1], "swim3_dma", fs->dma_irq, NULL, 0 );

	init_drive( &fs->drive_table[0], get_filename_res("floppy1"), 0 );
	init_drive( &fs->drive_table[1], get_filename_res("floppy2"), 1 );
	set_cur_drive();

	fs->total_secs = 2880;
       	fs->secpercyl = 36;
	fs->secpertrack = 18;

	fs->reg[r_ctrack]=0;
	fs->reg[r_ssize]=0x2;		/* should be 2 for normal floppies (?)  */
	
	add_cmd( "swim_dr", "swim_dr \ndisplay swim-registers\n", -1, cmd_swim_dr );
	add_cmd( "swim_irq", "swim_irq val \nset swim IRQ\n", -1, cmd_swim_irq );
	add_cmd( "insert_floppy", "ins_floppy imagename [B] "
		"\nUse image 'imagename' for floppy drive A [B].\n", -1, cmd_ins_floppy );

	/* XXX: When do we start outputing data? For now we let the
	 * DMA channel control the data flow (this is not the way to
	 * do it, but it works for now). Also, we should not dedicate a
	 * thread.
	 */
	create_thread( (void*)dma_entry, NULL, "SWIM-thread" );

	printm("Swim3 floppy controller\n");
	return 1;
}

static void
swim3_cleanup( void )
{
	char ch=0;
	int i;
	struct timespec tv;
	tv.tv_sec = 0;
	tv.tv_nsec = 100000000;
	

	write( fs->control_pipe[1], &ch, 1 );

	for( i=0; i<10 && !fs->exited; i++ ) {
		sched_yield();
		nanosleep(&tv, NULL);
	}
	close( fs->control_pipe[0] );
	close( fs->control_pipe[1] );
	
	release_dbdma( fs->dma_irq );

	if( fs->drive_table[0].fd != -1 )
		close( fs->drive_table[0].fd );
	if( fs->drive_table[1].fd != -1 )
		close( fs->drive_table[1].fd );

	pthread_mutex_destroy( &fs->lock_mutex );
}

driver_interface_t swim3_driver = {
	"swim3", swim3_init, swim3_cleanup
};
