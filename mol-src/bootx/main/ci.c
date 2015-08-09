/* 
 *   Creation Date: <2002/07/19 14:18:46 samuel>
 *   Time-stamp: <2003/07/30 15:31:39 samuel>
 *   
 *	<osi.c>
 *	
 *	Linux OSI calls
 *   
 *   Copyright (C) 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *
 */

#include "ci.h"
#include "osi_calls.h"
#include "partition_table.h"
#include "scsi_sh.h"

int
CloseProm( void )
{
	return OSI_PromIface( kPromClose, 0 );
}

/* ret: 0 no more peers, -1 if error */
mol_phandle_t
Peer( mol_phandle_t phandle )
{
	return OSI_PromIface( kPromPeer, phandle );
}

/* ret: 0 no child, -1 if error */
mol_phandle_t
Child( mol_phandle_t phandle )
{
	return OSI_PromIface( kPromChild, phandle );
}

/* ret: 0 if root node, -1 if error */
mol_phandle_t
Parent( mol_phandle_t phandle )
{
	return OSI_PromIface( kPromParent, phandle );
}

/* ret: -1 error */
int
PackageToPath( mol_phandle_t phandle, char *buf, long buflen )
{
	return OSI_PromIface2( kPromPackageToPath, phandle, (int)buf, buflen );
}

/* ret: -1 error */
int
GetPropLen( mol_phandle_t phandle, char *name )
{
	return OSI_PromIface1( kPromGetPropLen, phandle, (int)name );
}

/* ret: len or -1 (error) */
int
GetProp( mol_phandle_t phandle, char *name, char *buf, long buflen )
{
	return OSI_PromIface3( kPromGetProp, phandle, (int)name, (int)buf, buflen );
}

/* ret: -1 error, 0 last prop, 1 otherwise */
int
NextProp( mol_phandle_t phandle, char *prev, char *buf )
{
	return OSI_PromIface2( kPromNextProp, phandle, (int)prev, (int)buf );
}

/* ret: -1 if error */
int
SetProp( mol_phandle_t phandle, char *name, char *buf, long buflen )
{
	return OSI_PromIface3( kPromSetProp, phandle, (int)name, (int)buf, buflen );
}

mol_phandle_t
CreateNode( char *path )
{
	return OSI_PromPathIface( kPromCreateNode, path );
}

/* ret: -1 if not found */
mol_phandle_t
FindDevice( char *path )
{
	return OSI_PromPathIface( kPromFindDevice, path );
}


/************************************************************************/
/*	low-level device ops						*/
/************************************************************************/

typedef struct vol_desc vol_desc_t;

typedef struct {
	int	(*open)( struct vol_desc *vd );
	int	(*read)( struct vol_desc *vd, unsigned int blk, char *dest, long length );
} vol_ops_t;

struct vol_desc {
	long long	seek_pos;
	int 		unit, channel;
	short		in_use;
	short		is_cd;
	long long	par_offs;
	vol_ops_t	*ops;
	int		blocksize;
};

static int
ops_ablk_read( vol_desc_t *v, unsigned int blk, char *dest, long length )
{
	if( OSI_ABlkSyncRead( v->channel, v->unit, blk, (int)dest, length ) < 0 ) {
		printm("SyncRead: error\n");
		return -1;
	}
	return 0;
}

static int
ops_ablk_open( vol_desc_t *v )
{
	char buf[512];

	v->is_cd = 0;	/* fixme */

	if( OSI_ABlkSyncRead(v->channel, v->unit, 0, (ulong)buf, sizeof(buf)) < 0 )
		return -1;
	return 0;
}


//------------------ SCSI ---------------------------

typedef struct {
	int		initialized;
	int		blocksize;
	int		is_cd;
} scsi_unit_t;

#define MAX_NUM_SCSI_UNITS	32
static scsi_unit_t scsi_devs[ MAX_NUM_SCSI_UNITS ];

static int
scsi_cmd_( vol_desc_t *v, const char *cmd, int cmdlen, char *dest, int len, int prelen, int postlen )
{
	scsi_req_t r[2];	/* the [2] is a hack to get space for the sg-list */
	char sb[32];
	char prebuf[4096], postbuf[4096];

	//memset( dest, 0, len );

	if( (uint)prelen > sizeof(prebuf) || (uint)postlen > sizeof(postbuf) ) {
		printm("bad pre/post len %d %d\n", prelen, postlen );
		return 1;
	}

	memset( r, 0, sizeof(r[0]) );
	r->lun = 0;
	r->target = v->unit;
	r->is_write = 0;
	memcpy( r->cdb, cmd, cmdlen );
	r->client_addr = (int)&r;
	r->cdb_len = cmdlen;
	r->sense[0].base = (int)&sb;
	r->sense[0].size = sizeof(sb);
	r->size = prelen + len + postlen;
	r->n_sg = 3;
	r->sglist.n_el = 3;
	r->sglist.vec[0].base = (int)prebuf;
	r->sglist.vec[0].size = prelen;
	r->sglist.vec[1].base = (int)dest;
	r->sglist.vec[1].size = len;
	r->sglist.vec[2].base = (int)postbuf;
	r->sglist.vec[2].size = postlen;

	if( OSI_SCSISubmit((int)&r) ) {
		printm("OSI_SCSISubmit: error!\n");
		return 1;
	}
	while( !OSI_SCSIAck() )
		OSI_USleep( 10 );

	if( r->adapter_status )
		return -1;
	if( r->scsi_status )
		return ((sb[2] & 0xf) << 16) | (sb[12] << 8) | sb[13];
	return 0;
}

static int
scsi_cmd( vol_desc_t *v, const char *cmd, int cmdlen )
{
	return scsi_cmd_( v, cmd, cmdlen, NULL, 0, 0, 0 );
}


static int
ops_scsi_read( vol_desc_t *v, unsigned int blk, char *dest, long len )
{
	int xsize, size, prelen, postlen, bfactor = v->blocksize / 512;
	char cmd[10];

	memset( dest, 0, len );

	/* if not a CD/DVD or disk, just read zeros */
	if( v->channel < 0 )
		return -1;

	/* printm("READ: blk: %d length %d\n", blk, len ); */

	/* deblocker */
	prelen = (blk % bfactor) * 512;
	blk /= bfactor;

	while( len ) {
		size = 20*v->blocksize - prelen;
		if( size > len )
			size = len;
		len -= size;

		postlen = (size + prelen) % v->blocksize;
		if( postlen )
			postlen = v->blocksize - postlen;
		xsize = prelen + postlen + size;

		memset( cmd, 0, sizeof(cmd) );
		cmd[0] = 0x28; /* READ_10 */
		cmd[2] = blk >> 24;
		cmd[3] = blk >> 16;
		cmd[4] = blk >> 8;
		cmd[5] = blk;
		cmd[7] = (xsize/v->blocksize) >> 8;
		cmd[8] = (xsize/v->blocksize);

		if( scsi_cmd_(v, cmd, 10, dest, size, prelen, postlen) ) {
			printm("read: scsi_cmd failed\n");
			return -1;
		}
		blk += (size + prelen + postlen) / v->blocksize;

		prelen = 0;
		dest += size;
	}
	return 0;
}

static int
ops_scsi_open( vol_desc_t *v )
{
	static int once = 0;
	scsi_unit_t *unit = &scsi_devs[v->unit];

	if( !once ) {
		once++;
		OSI_SCSIControl( SCSI_CTRL_INIT, 0 );
	}
	if( (uint)v->unit >= MAX_NUM_SCSI_UNITS )
		return -1;
	
	/* the should only be done once since it typically spins down the disk motor */
	if( !unit->initialized ) {
		char inquiry_cmd[6] = { 0x12, 0, 0, 0, 32, 0 };
		char start_stop_unit_cmd[6] = { 0x1b, 0, 0, 0, 1, 0 };
		char test_unit_ready_cmd[6] = { 0x00, 0, 0, 0, 0, 0 };
		char set_cd_speed_cmd[12] = { 0xbb, 0, 0xff, 0xff, 0xff, 0xff, 0, 0, 0, 0, 0, 0 };
		char prev_allow_medium_removal[6] = { 0x1e, 0, 0, 0, 1, 0 };
		char ret[32];
		int sense;

		if( (sense=scsi_cmd_(v, inquiry_cmd, 6, ret, 2, 0, 0)) ) {
			if( sense < 0 )
				return -1;
			printm("INQUIRY failed\n");
			return -1;
		}

		unit->is_cd = 0;
		unit->blocksize = 512;

		if( ret[0] == 5 /* CD/DVD */ ) {
			unit->blocksize = 2048;
			unit->is_cd = 1;

			scsi_cmd( v, test_unit_ready_cmd, 6 );
			scsi_cmd( v, prev_allow_medium_removal, 6 );
			scsi_cmd( v, set_cd_speed_cmd, 12 );
			scsi_cmd( v, start_stop_unit_cmd, 6 );

		} else if( ret[0] == 0 /* DISK */ ) {
			scsi_cmd( v, test_unit_ready_cmd, 6 );
			scsi_cmd( v, start_stop_unit_cmd, 6 );
		} else {
			/* don't boot from this device (could be a scanner :-)) */
			return -1;
		}

		/* wait for spin-up (or whatever) to complete */
		do {
			sense = scsi_cmd( v, test_unit_ready_cmd, 6 );
		} while( (sense & 0xf0000) == 0x20000 );

		unit->initialized = 1;
	}
	v->blocksize = unit->blocksize;
	v->is_cd = unit->is_cd;
	return 0;
}


static vol_ops_t ablk_ops = {
	.open	= ops_ablk_open,
	.read	= ops_ablk_read,
};

static vol_ops_t scsi_ops = {
	.open	= ops_scsi_open,
	.read	= ops_scsi_read,
};


/************************************************************************/
/*	Generic File Ops						*/
/************************************************************************/

#define MAX_NUM_FDS	256
static vol_desc_t	fds[ MAX_NUM_FDS ];


static long long
find_partition_offs( int ih, int parnum )
{
	desc_map_t dmap;
	part_entry_t par;
	
	if( !parnum )
		return 0;
	
	memset( &dmap, 0, sizeof(dmap) );
	Seek( ih, 0 );
	Read( ih, (char *) &dmap, sizeof(dmap) );

	if( dmap.sbSig != DESC_MAP_SIGNATURE )
		return -1;

	Seek( ih, dmap.sbBlockSize );
	Read( ih, (char *) &par, sizeof(part_entry_t) );

	if( parnum > par.pmMapBlkCnt || par.pmSig != 0x504d /* 'PM' */ )
		return -1;

	Seek( ih, ((long long)dmap.sbBlockSize) * parnum );
	Read( ih, (char *) &par, sizeof(part_entry_t) );

	if( par.pmSig != 0x504d /* 'PM' */ )
		return -1;
	
	// printm("Success: offset = %d x %d\n", par.pmPyPartStart, dmap.sbBlockSize );
	return (long long)par.pmPyPartStart * dmap.sbBlockSize;
}


int
Open( char *devSpec )
{
	int i, chan, unit=0, part=0, channel=0;
	mol_phandle_t ph;
	char typebuf[24], *pp;
	vol_ops_t *ops;
	vol_desc_t *v;

	if( (ph=FindDevice(devSpec)) == -1 || (ph=Parent(ph)) == -1 ) {
		/* printm("Open: Could not open node %s\n", devSpec ); */
		return -1;
	}
	if( GetProp( ph, "channel", (char*)&chan, 4) == 4 )
		channel = chan;
	
	for( i=strlen(devSpec)-1 ; i >= 0 ; i-- ) {
		if( devSpec[i] == '@' ) {
			unit = strtol( devSpec+i+1, &pp, 16 );
			if( *pp == ':' )
				part = strtol( pp+1, NULL, 10 );
			break;
		}
	}
	for( i=1; i<MAX_NUM_FDS; i++ )
		if( !fds[i].in_use ) 
			break;

	if( i == MAX_NUM_FDS ) {
		printm("Too many file descriptors\n");
		return -1;
	}
	v = &fds[i];
	memset( v, 0, sizeof(*v) );
	
	v->seek_pos = 0;
	v->unit = unit;
	v->channel = channel;
	v->in_use = 1;
	v->par_offs = 0;
	v->blocksize = 512;
	
	// determine device type
	ops = &ablk_ops;
	if( GetProp(ph, "device_type", typebuf, sizeof(typebuf)) > 0 ) {
		if( !strcmp(typebuf, "scsi") )
			ops = &scsi_ops;
	}
	v->ops = ops;
	
	if( ops->open && (*ops->open)(v) ) {
		Close( i );
		return -1;
	}

	v->par_offs = find_partition_offs( i, part );
	if( v->par_offs < 0 ) {
		Close( i );
		return -1;
	}
	return i;
}

int
IsCD( int ih )
{
	vol_desc_t *v = &fds[ih];
	
	if( ih < 0 || ih >= MAX_NUM_FDS || !fds[ih].in_use )
		return 0;
	return v->is_cd;
}

void
Close( int ih )
{
	if( ih >= 0 && ih < MAX_NUM_FDS )
		fds[ih].in_use = 0;
}

void
Read( int ih, char * addr, long length )
{
	vol_desc_t *v = &fds[ih];
	long long pos;
	
	if( ih < 0 || ih >= MAX_NUM_FDS || !fds[ih].in_use )
		return;
	pos = v->seek_pos + v->par_offs;
	
	if( pos & 511 ) {
		printm("SyncRead: Bad alignment\n");
		return;
	}

	/* Handle non-512 byte aligned reads
	 * Added because the compressed disks expect 512 byte chunks */
	if(length & 511) {
		char buf[512];
		int sectors = length / 512;

		/* Read all of the normal 512 byte chunks */
		if (sectors > 0)
			(*v->ops->read)(v, pos / 512, addr, sectors * 512);

		/* Read the partial sector and copy only the valid part */
		(*v->ops->read)(v, (pos / 512) + sectors, buf, 512);
		memcpy(addr + sectors * 512, buf, length - (sectors * 512));
	}
	else {
		(*v->ops->read)( v, pos / 512, addr, length );
	}

//	v->seek_pos += length;
}

void
Seek( int ih, long long position )
{
	if( ih < 0 || ih >= MAX_NUM_FDS || !fds[ih].in_use )
		return;

	fds[ih].seek_pos = position;
}
