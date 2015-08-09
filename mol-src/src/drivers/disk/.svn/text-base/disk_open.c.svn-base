/* 
 *   Creation Date: <1999/12/05 18:26:08 samuel>
 *   Time-stamp: <2004/03/24 02:18:14 samuel>
 *   
 *	<disk_open.c>
 *	
 *	Open block device to be mounted in MacOS
 *   
 *   Copyright (C) 1999, 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include <sys/ioctl.h>
#include <sys/file.h>
#include <sys/stat.h>
#ifdef __linux__
#include <scsi/scsi.h>
#endif

#include "verbose.h"
#include "debugger.h"
#include "disk.h"
#include "llseek.h"

SET_VERBOSE_NAME("disk_open");

typedef struct dev_entry {
	int			is_subdevice;
	char 			*dev_name;
	struct dev_entry	*next;
} dev_entry_t;

/* XXX these are never released */
static dev_entry_t		*devs;		


/* reserve makes sure we haven't mounted a volume r/w already. It also handles
 * the special case where the user tries to trick us to mount /dev/hda and
 * /dev/hdaX simultaneously.
 *
 * Note: Only paths with the /dev/ prefix are handled here.
 */
static int
reserve_dev( const char *name ) 
{
	dev_entry_t *dp;
	int match=0;

	for( dp=devs; !match && dp; dp=dp->next ) {
		if( dp->is_subdevice )
			match = !strcmp( dp->dev_name, name );
		else
			match = !strncmp( dp->dev_name, name, strlen(dp->dev_name) );
		if( match ) {
			printm("-------> %s is already mounted with write privileges.\n", dp->dev_name );
			return 1;
		}
	}

	/* add device to our list */
	if( !match && !strncmp(name, "/dev/", 5) ) {
		dp = malloc( strlen(name)+1 + sizeof(*dp) );
		memset( dp, 0, sizeof(*dp) );

		dp->dev_name = (char*)dp + sizeof(dev_entry_t);
		strcpy( dp->dev_name, name );
		if( strlen(name) != 8 ) {
			dp->is_subdevice = 1;	/* /dev/sdaX type device */
			if( strlen(name) > 8 )
				dp->dev_name[8] = 0;
		}
		dp->next = devs;
		devs = dp;
	}
	return 0;
}

/* examine the contents of /proc/mounts to make sure the volume
 * 'name' is not mounted with r/w privileges.
 */
static int
linux_mount_check( char *name, int rwall )
{
	char buf[81], *s1, *s2;
	int rw_mounted = 0;
	FILE *file;
	
	/* Is there a way to examine if a block device is mounted except checking /proc/mounts? */
	file = fopen("/proc/mounts", "r");
	while( file && fgets(buf, 80, file) ) {
		s2 = &buf[0];
		s1 = strsep( &s2, " " );
		if( strncmp(s1, "/dev/", 5) )
			continue;
		if( !strcmp(s1, name) ) { 
			/* complete match */
			if( strstr( s2, " rw" ) != NULL ) {
				printm("------> %s is linux-mounted with write privileges.\n", name );
				rw_mounted = 1;
				break;
			}
		} else if( !strncmp(s1, name, 8) && !name[8] ) {
			
			/* partial match (/dev/sda for instance) */
			if( strstr(s2, " hfs ") && strstr( s2, " rw") ) {
				printm("------> %s contains a linux-mounted HFS volume (%s).\n", 
				       name, s1 );
				rw_mounted = 1;
				break;
			} else if( strstr(s2, " rw") && rwall ) {
				printm("------> %s contains a r/w linux-mounted volume (%s).\n"
				       "Use the -rw flag instead of -rwall (allows write access only\n"
				       "to the HFS partitions).\n",
				       name, s1 );
				rw_mounted = 1;
				break;
			}
		}
	}
	if( file )
		fclose( file );

	return rw_mounted;
}

/* ret: fd, -1 == error. CDs are opened for ioctl-only access */
int
disk_open( char *name, int flags, int *ro_fallback, int silent )
{
	int rw = (flags & BF_ENABLE_WRITE) ? 1:0;
	int rwall = (flags & BF_FORCE) && rw;
	int cd = (flags & BF_CD_ROM);
	int fd;

	if( rw && (reserve_dev(name) || linux_mount_check(name, rwall)) ) {
		/* force read only */
		if( !*ro_fallback ) {
			printm("Could not open '%s' with read-write permissions\n", name);
			return -1;
		}
		rw = 0;
	}

	for( ;; ) {
		char buf[16];
		int mode = cd ? O_NONBLOCK : 0;

		/* for CDs, ioctl-only access is specified though the usage of O_NONBLOCK */
		if( (fd=open64(name, mode | (rw ? O_RDWR : O_RDONLY))) < 0 )
			if( errno == EROFS && *ro_fallback ) {
				fd = open64( name, mode | O_RDONLY );
				rw = 0;
			}
		if( fd < 0 && !silent ) {
			LOG_ERR("Opening %s", name );
			return -1;
		}
		if( cd )
			break;

		/* make sure the size of this device is nonzero */
		if( lseek(fd, 0, SEEK_SET) || read(fd, buf, sizeof(buf)) != sizeof(buf) ) {
			LOG_ERR("Device %s is zero length, refusing to use it.", name );
			close( fd );
			return -1;
		}

		/* even more safety checking */
		if( rw && flock(fd, LOCK_EX | LOCK_NB) < 0 && errno == EWOULDBLOCK ) {
			printm("------> The volume '%s' is locked\n", name );
			close( fd );
			if( !*ro_fallback )
				return -1;
			
			rw = 0;
			continue;
		}
		break;
	}

	*ro_fallback = !rw;
	return fd;
}
