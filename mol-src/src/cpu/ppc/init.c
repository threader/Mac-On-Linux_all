/* 
 *   Creation Date: <2001/10/14 04:05:37 samuel>
 *   Time-stamp: <2004/03/03 13:09:32 samuel>
 *   
 *	<main.c>
 *	
 *	User process/kernel module interface 
 *   
 *   Copyright (C) 2001, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include <sys/mman.h>
#include <sys/time.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

#include "molcpu.h"
#include "version.h"
#include "memory.h"
#include "timer.h"
#include "res_manager.h"
#include "wrapper.h"

/* assembly export (from mainloop_asm.S) */
extern void register_asm_rvecs( void );

static int moldev_fd = -1;

int
mol_ioctl( int cmd, int p1, int p2, int p3 )
{
	mol_ioctl_pb_t pb;
	int ret;
	pb.arg1 = p1;
	pb.arg2 = p2;
	pb.arg3 = p3;
	ret = ioctl( moldev_fd, cmd, &pb );
#ifdef __darwin__ 
	if( !ret )
		ret = pb.ret;
#endif
	return ret;
}

int
mol_ioctl_simple( int cmd )
{
	return ioctl( moldev_fd, cmd, NULL );
}


void
wrapper_init( void )
{
#ifdef __linux__
	char *name = "/dev/mol";
#else
	char name[32];
	sprintf( name, "/dev/mol%d", g_session_id );
#endif
	if( (moldev_fd=open(name, O_RDWR)) < 0 )
		fatal_err("opening %s", name );

	fcntl( moldev_fd, F_SETFD, FD_CLOEXEC );
}

void
wrapper_cleanup( void )
{
	close( moldev_fd );
	moldev_fd = -1;
}

ulong
get_cpu_frequency( void )
{
	static ulong clockf = 0;
	char buf[80], *p;
	FILE *f;

	if( !clockf ) {
		/* Open /proc/cpuinfo to get the clock */
		if( (f=fopen("/proc/cpuinfo", "ro")) ) {
			while( !clockf && fgets(buf, sizeof(buf), f) )
				if( !strncmp("clock", buf, 5 ) && (p=strchr(buf,':')) )
					clockf = strtol( p+1, NULL, 10 );
			fclose(f);
		}

		/* If /sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq 
		 * exists, read the cpu speed from there instead */
		if( (f=fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "ro")) ) {
			if(fgets(buf, sizeof(buf), f) == 0)
				clockf = strtol(buf, NULL, 10) / 1000;

			fclose(f);
		}

		if( clockf < 30 || clockf > 4000 ) {
			printm("Warning: Using hardcoded clock frequency (350 MHz)\n");
			clockf = 350;
		}
		clockf *= 1000000;
	}
	return clockf;
}

/* Tries to read the bus frequency from /proc/device-tree/cpus/<CPU>@0/bus-frequency
 * Returns the frequency or 0 on error 
 */
ulong
get_bus_frequency( void ) {
	static ulong busf = 0;
	int name_len;
	char buf[80] = "/proc/device-tree/cpus/";
	FILE *f;
	DIR *d;
	struct dirent *procdir;

	if( !busf ) {
		/* Open /proc/device-tree/cpus */
		d = opendir(buf);
		if ( d == NULL ) {
			printm ("Warning: Unable to open %s\n",buf);
			return 0;
		}

		/* Each directory is a cpu, find @0 */
		while ( (procdir = readdir(d)) != NULL ) {
			name_len = strlen(procdir->d_name);
			if ( name_len > 2 &&
			     (procdir->d_name)[name_len - 1] == '0' &&
			     (procdir->d_name)[name_len - 2] == '@')
			    	break;
		}
		closedir(d);

		/* Open bus-frequency in that directory */
		if (procdir != NULL) {
			strncat(buf, procdir->d_name, 79);
			strncat(buf, "/bus-frequency", 79);
			f = fopen(buf, "r");
			if ( f == NULL) {
				printm ("Warning: Couldn't open the cpu device tree node!\n");
				return 0;
			}
			if (fread(&busf, 4, 1, f) != 1) {
				printm ("Warning: Couldn't read from the cpu device tree node!\n");
				fclose(f);
				return 0;
			}
			fclose(f);
		}
		else {
			printm ("Warning: Couldn't find cpu in device tree!\n");
			return 0;
		}
	}
	return busf;
}

mol_kmod_info_t *
get_mol_kmod_info( void ) 
{
	static mol_kmod_info_t info;
	static int once=0;
	
	if( !once ) {
		memset( &info, 0, sizeof(info) );
		_get_info( &info );
	}
	return &info;
}


/************************************************************************/
/*	initialization / cleanup					*/
/************************************************************************/

void 
molcpu_arch_init( void )
{
	register_asm_rvecs();
}

void 
molcpu_arch_cleanup( void )
{
}

#ifdef __linux__
static inline void
do_map_mregs( void )
{
	mregs = (mac_regs_t*)map_phys_mem( 0, _get_mregs_phys(), NUM_MREGS_PAGES * 0x1000,
					   PROT_READ | PROT_WRITE );
}

static inline void
do_unmap_mregs( void )
{
	if( mregs )
		unmap_mem( (char*)mregs, sizeof(mac_regs_t) );
}
#endif /* __linux__ */

#ifdef __darwin__
static inline void
do_map_mregs( void )
{
	_get_mregs_virt( &mregs );
}

static inline void
do_unmap_mregs( void )
{
	/* do nothing, mregs are unmapped automatically */
}
#endif /* __darwin__ */

static void
map_mregs( void )
{
	do_map_mregs();
	if( !mregs )
		fatal_err("map_kmem:");
	memset( mregs, 0, sizeof(mac_regs_t) );
}

static void
unmap_mregs( void )
{
	do_unmap_mregs();
	mregs = NULL;
}


/************************************************************************/
/*	create MOL session						*/
/************************************************************************/

static void
check_kmod_version( void )
{
	int kvers = _get_mol_mod_version();

	if( !kvers )
		fatal("The MOL kernel module is not loaded\n");

	if( kvers != MOL_VERSION )
		fatal("The MOL kernel module version %d.%d.%d does not match the binary.\n"
		      "Please unload the kernel module (rmmod mol) and try again\n",
		      (kvers>>16), (kvers>>8)&0xff, kvers&0xff );

	if( is_a_601() )
		fatal("MOL does not supports the PowerPC 601 processor\n");
}

int
open_session( void )
{
	struct timeval tv;
	
	check_kmod_version();

	gettimeofday( &tv, NULL );
	srandom( tv.tv_usec + get_tbl() );

	if( _create_session(g_session_id) < 0 ) {
		switch( errno ) {
		case EMOLINUSE:
			printm("---> Session %d is already running (needs to be killed?)\n", g_session_id);
			break;

		case EMOLINVAL:
			printm("The session number (%d) is out of bounds\n", g_session_id );
			break;

		case EMOLSECURITY:
			printm("*****************************************************\n"
			       "* SECURITY ALERT. Somebody (other than MOL) has\n"
			       "* tried to utilize the MOL infrastructure. Due to\n"
			       "* security reasons, a reboot is necessary in order to\n"
			       "* get MOL working again.\n"
			       "*****************************************************\n");
			break;

		case EMOLGENERAL:
			printm("_create_session() failed due to an unspecified error\n");
			break;
			
		default:
			perrorm("_create_session");
			break;
		}
		return 1;
	}
	g_session_magic = _get_session_magic( random() );
	map_mregs();

	return 0;
}

void
close_session( void )
{
	unmap_mregs();
}
