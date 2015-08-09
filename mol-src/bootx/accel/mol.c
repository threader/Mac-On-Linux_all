/* 
 *   Creation Date: <2002/08/10 21:23:04 samuel>
 *   Time-stamp: <2003/08/24 17:31:02 samuel>
 *   
 *	<mol.c>
 *	
 *	Mol specific stuff
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *
 */

#include "sl.h"
#include "osi_calls.h"
#include "mac_registers.h"
#include "mol.h"
#include "emuaccel_sh.h"
#include "boothelper_sh.h"

mac_regs_t	*mregs;
ulong		gEmuAccelPage;

static int	gDryRun;

/************************************************************************/
/*	splitmode code replacement					*/
/************************************************************************/

typedef struct {
	char 	*key;
	ulong	func;
} accel_desc_t;

extern char		accel_start[], accel_end[];
extern accel_desc_t	accel_table__start[], accel_table__end[];


static void
splitmode_fixes( void )
{
	mol_phandle_t ph;
	accel_desc_t *t;
	char buf[128];
	ulong molmem, size, addr;

	size = (int)accel_end - (int)accel_start;
	size = (size + 0xfff) & ~0xfff;

	// allocate memory
	molmem = AllocateKernelMemory( size );
	AllocateMemoryRange( "Kernel-Mol", molmem, size );
	//printm("mol owned memory at %08lX (%x)\n", molmem, size );
	memcpy( (char*)molmem, accel_start, (int)accel_end - (int)accel_start );

	ph = CreateNode("/mol-accel");
	
	for( t=accel_table__start ; t < accel_table__end ; t++ ) {
		sprintf( buf, "accel-%s", t->key );
		addr = molmem + t->func - (int)accel_start;
		SetProp( ph, buf, (char*)&addr, sizeof(addr) );
		//printm("Acceleration function: %s @ %08lX\n", t->key, addr );
	}
}


/************************************************************************/
/*	privileged instruction replacement 				*/
/************************************************************************/

#define REC_ZERO	1		/* require bit31 to be zero */

#define FD		1		/* field is dest reg */
#define FS		2		/* field is source register */
#define Fz		8		/* field must be zero */
#define Fx		16		/* field can be anything */

struct encode_info;
typedef int 		encode_inst_fn( ulong *d, struct encode_info *info );

typedef struct encode_info {
	encode_inst_fn	*encoder;
	int		mregs_offs;
	int		r;		/* destination (or source) reg */
	ulong		*inst_ptr;
	ulong		inst;		/* inst opcode */
} encode_info_t;

typedef struct inst_info {
	int		prim_opc;	/* primary opcode (bit 0-5) */
	int		sec_opc;	/* secondary opcode (bit 21-30) */
	int		f1,f2,f3;
	int		flags;
	int		(*inst_handler)( struct inst_info *rec, encode_info_t *p );
} inst_t;

#define OFFS(x)		offsetof( mac_regs_t, x )

#define inst_lis_alg( dr, addr ) \
	((15*MOL_BIT(5)) + (MOL_BIT(10)*dr) + hi_alg(addr))
#define inst_lis_log( dr, addr ) \
	((15*MOL_BIT(5)) + (MOL_BIT(10)*dr) + (((ulong)addr)>>16))
#define inst_ori( dr, sr, addr)	\
	((24*MOL_BIT(5)) + (MOL_BIT(10)*sr) + (MOL_BIT(15)*dr) + ((addr) & 0xffff))
#define inst_lwz( dr, sr, addr) \
	((32*MOL_BIT(5)) + (MOL_BIT(10)*dr) + (MOL_BIT(15)*sr) + ((addr) & 0xffff))
#define inst_b( from, to ) \
	((18*MOL_BIT(5)) + ((((ulong)(to)-(ulong)(from)) & 0x03fffffc)))
#define inst_lwzx( dr, ar, br) \
	((31*MOL_BIT(5)) + (MOL_BIT(10)*dr) + (MOL_BIT(15)*ar) + (MOL_BIT(20)*br) + (23 << 1))

static ulong
hi_alg( ulong addr )
{
	ulong l = (addr & 0x8000) ? (0xffff8000 | addr) : (addr & 0x7fff);
	return (addr - l) >> 16;
}

static void
flush_inst( ulong addr )
{
	__asm__ volatile("dcbst 0,%0 ; sync ; icbi 0,%0 ; sync ; isync" :: "r" (addr));
}

/* 0: OK, -1: error */
static int
add_emuaccel_branch( int emuaccel_flags, int param, ulong *inst_ptr ) 
{
	int target;

	if( gDryRun )
		return 0;

       	target = OSI_EmuAccel( emuaccel_flags, param, (int)inst_ptr );
	if( !target ) {
		printm("OSI_EmuAccel failed\n");
		return -1;
	}
	*inst_ptr = inst_b( inst_ptr, target );
	flush_inst( (ulong)inst_ptr );
	return 0;
}

static int
encode_mregs_read( ulong *d, encode_info_t *info )
{
	ulong maddr = (ulong)mregs + info->mregs_offs;
	int c=0, dr=info->r;

	if( !dr ) {
		d[c++] = inst_lis_log( dr, maddr );
		d[c++] = inst_ori( dr, dr, maddr );
		d[c++] = inst_lwzx( dr, 0, dr );
	} else {
		d[c++] = inst_lis_alg( dr, maddr );
		d[c++] = inst_lwz( dr, dr, maddr );
	}
	return c;
}

static int
encode_mfdec( ulong *d, encode_info_t *info )
{
	ulong maddr = (ulong)mregs + info->mregs_offs;
	int c=0, dr=info->r;

	if( !dr ) {
		d[c++] = inst_lis_log( dr, maddr );
		d[c++] = inst_ori( dr, dr, maddr );
		if( add_emuaccel_branch( EMUACCEL_UPDATE_DEC, 0, &d[c++] ) )
			return 0;
		d[c++] = inst_lwzx( dr, 0, dr );
	} else {
		d[c++] = inst_lis_alg( dr, maddr );
		if( add_emuaccel_branch( EMUACCEL_UPDATE_DEC, 0, &d[c++] ) )
			return 0;
		d[c++] = inst_lwz( dr, dr, maddr );
	}
	return c;
}


/************************************************************************/
/*	instruction handlers						*/
/*	   ret: 0==done, 1==replace_glue				*/
/************************************************************************/

static int
handle_mfmsr( inst_t *rec, encode_info_t *p )
{
	p->mregs_offs = OFFS(msr);
	p->encoder = encode_mregs_read;
	return 1;
}

static int
handle_mfsr( inst_t *rec, encode_info_t *p ) 
{
	uint sr = (p->inst >> 16) & 0x1f;

	// opcode check
	if( sr & 0x10 )
		return 0;
	p->mregs_offs = OFFS(segr) + sr*4;
	p->encoder = encode_mregs_read;
	return 1;
}

static int
handle_mtsr( inst_t *rec, encode_info_t *p ) 
{
	uint sr = (p->inst >> 16) & 0x1f;

	// opcode check
	if( sr & 0x10 )
		return 0;
	add_emuaccel_branch( EMUACCEL_MTSR | EMUACCEL_HAS_PARAM,
			     p->inst, p->inst_ptr );
	return 0;
}

static int
handle_mfspr( inst_t *rec, encode_info_t *p )
{
	uint spr = ((p->inst >> 16) & 0x1f) | ((p->inst >> 6) & 0x3e0);

	if( !(spr & 0x10) )
		return 0;		/* user spr */

	p->mregs_offs = OFFS(spr[0]) + spr * sizeof(ulong);
	p->encoder = encode_mregs_read;

	switch( spr ) {
	case S_SRR0: case S_SRR1:
	case S_SPRG0: case S_SPRG1: case S_SPRG2: case S_SPRG3:
	case S_PVR:
	case S_SDR1:
	case S_IBAT0U: case S_IBAT0L: case S_IBAT1U: case S_IBAT1L: case S_IBAT2U: case S_IBAT2L:
	case S_IBAT3U: case S_IBAT3L: case S_DBAT0U: case S_DBAT0L: case S_DBAT1U: case S_DBAT1L: 
	case S_DBAT2U: case S_DBAT2L: case S_DBAT3U: case S_DBAT3L:
	case S_DAR: case S_DSISR:
	case S_HID0: case S_HID1: case S_HID2: case S_HID4: case S_HID5:
	case S_ICTRL: case S_MSSCR0: case S_MSSCR1: case S_LDSTCR: 
	case S_L2CR: case S_L3CR: case S_ICTC: case S_THRM1: case S_THRM2: case S_THRM3: case S_PIR:
	case S_MMCR2: case S_MMCR0: case S_PMC1: case S_PMC2: case S_SIAR: case S_MMCR1: case S_PMC3: case S_PMC4:
	case S_SDAR:
		break;
	case S_DEC:
		p->encoder = encode_mfdec;
		break;
	default:
		return 0;
	}
	return 1;
}

static int
handle_mtspr( inst_t *rec, encode_info_t *p )
{
	uint spr = ((p->inst >> 16) & 0x1f) | ((p->inst >> 6) & 0x3e0);
	int i;
	static struct { int spr, eflag; } table[] = {
		{ S_SPRG0,	EMUACCEL_MTSPRG0 },
		{ S_SPRG1,	EMUACCEL_MTSPRG1 },
		{ S_SPRG2,	EMUACCEL_MTSPRG2 },
		{ S_SPRG3,	EMUACCEL_MTSPRG3 },
		{ S_SRR0,	EMUACCEL_MTSRR0	 },
		{ S_SRR1,	EMUACCEL_MTSRR1	 },
		{ S_HID0,	EMUACCEL_MTHID0	 },
	};
	for( i=0; i<sizeof(table)/sizeof(table[0]) ; i++ ) {
		if( table[i].spr != spr )
			continue;
		//printm("Emuaccel: mtspr r%d (NIP %08lx)\n", p->r, (int)p->inst_ptr );
		add_emuaccel_branch( table[i].eflag + p->r, 0, p->inst_ptr );
		return 0;
	}
	return 0;
}

static int
handle_mtmsr( inst_t *rec, encode_info_t *p )
{
	add_emuaccel_branch( EMUACCEL_MTMSR + p->r, 0, p->inst_ptr );
	return 0;
}

static int
handle_rfi( inst_t *rec, encode_info_t *p )
{
	add_emuaccel_branch( EMUACCEL_RFI, 0, p->inst_ptr );
	return 0;
}

static int
handle_nopout( inst_t *rec, encode_info_t *p )
{
	*p->inst_ptr = 0x60000000;	/* nop */
	return 0;
}

static inst_t priv_insts[] = {	
	{ 31, 83,	FD,Fz,Fz,	REC_ZERO,     	handle_mfmsr },	/* mfmsr */
	{ 31, 339,	FD,Fx,Fx,	REC_ZERO,	handle_mfspr },	/* mfspr */
	{ 31, 146,	FS,Fz,Fz,	REC_ZERO,	handle_mtmsr },	/* mtmsr */
	{ 31, 467,	FS,Fx,Fx,	REC_ZERO,	handle_mtspr }, /* mtspr */
	{ 19, 50,	Fz,Fz,Fz,	REC_ZERO,	handle_rfi },	/* rfi */
	{ 31, 595,	FD,Fx,Fz,	REC_ZERO,	handle_mfsr },	/* mfsr */
	{ 31, 210,	FS,Fx,Fz,	REC_ZERO,	handle_mtsr },	/* mtsr */

	// nop-out tlbie and eieio (MOL intercepts hash-table writes)
	{ 31, 854, 	Fz,Fz,Fz,	REC_ZERO, 	handle_nopout },/* eieio */
	{ 31, 306,	Fz,Fz,FS,	REC_ZERO,	handle_nopout },/* tlbie */
};


/************************************************************************/
/*	instruction lookup / replacement				*/
/************************************************************************/

static int
lookup_inst( ulong *inst_ptr, encode_info_t *info )
{
	ulong inst = *inst_ptr;
	ulong prim = OPCODE_PRIM(inst);
	ulong sec = OPCODE_EXT(inst);
	int i, j, r;

	info->inst_ptr = inst_ptr;
	info->inst = inst;
	
	for( i=0; i<sizeof(priv_insts)/sizeof(priv_insts[0]); i++ ) {
		inst_t *p = &priv_insts[i];
		ulong mask;
	
		// opcode matching
		if( p->prim_opc != prim || (p->sec_opc >=0 && p->sec_opc != sec) )
			continue;
		if( ((p->f1 & Fz) && B1(inst)) || ((p->f2 & Fz) && B2(inst)) || ((p->f3 & Fz) && B3(inst)) )
			continue;
		if( (p->flags & REC_ZERO) && (inst & 1) )
			continue;
		
		// get reg
		mask =  ((p->f1 & (FD|FS))? 0x03e00000 : 0) |
			((p->f2 & (FD|FS))? 0x001f0000 : 0) |
			((p->f3 & (FD|FS))? 0x0000f800 : 0);
		r = inst & mask;
		for( j=0; j<31 && !(mask & 1); j++ ) {
			mask = mask >> 1 ;
			r = r >> 1;
		}
		info->r = r;

	        return (p->inst_handler)( p, info );
	}
	return -1;
}

static void
inst_replacements( void )
{
	encode_info_t info;
	ulong size, *p, *d=NULL;

	// allocate mregs
	size = (sizeof(mac_regs_t) + 0xfff) & ~0xfff;
	mregs = (mac_regs_t*)AllocateKernelMemory( size );
	AllocateMemoryRange( "Kernel-MREGS", (ulong)mregs, size );
	OSI_MapinMregs( (ulong)mregs );

//	printm("mregs mapped @ %08lx (%x)\n", (ulong)mregs, size );

	// allocate emuaccel page
	gEmuAccelPage = AllocateKernelMemory( 0x1000 );
	AllocateMemoryRange( "Kernel-EMUACCEL", gEmuAccelPage, 0x1000 );
	if( !OSI_EmuAccel( EMUACCEL_MAPIN_PAGE, 0, gEmuAccelPage ) ) {
		printm("Failed to mapin emuaccel page!\n");
		gEmuAccelPage = 0;
	}
	
	for( gDryRun=1; gDryRun >=0 ; gDryRun-- ) {
		ulong dummybuf[16];
		int c, i;
		
		if( !gDryRun ) {
			char buf[128];
			size = (size + 0xfff) & ~0xfff;
			d = (ulong*)AllocateKernelMemory( size );
			sprintf( buf, "Kernel-inst-%08lx\n", (ulong) d );
			AllocateMemoryRange( buf, (ulong)d, size );
		}
		size = 0;
		for( p=NULL; (ulong)p < (ulong)mregs ; p++ ) {
			if( gDryRun )
				d = dummybuf;
			
			if( lookup_inst( p, &info ) != 1 )
				continue;
			if( (c=(*info.encoder)( d, &info )) <= 0 )
				continue;

			// add return branch to emulated instruction + 4
			d[c] = inst_b( &d[c], p+1 );
			c++;

			// flush caches
			for( i=0; i<c ; i++ )
				flush_inst( (ulong)&d[i] );
		
			// ...and replace instruction with branch
			if( !gDryRun ) {
				*p = inst_b( p, &d[0] );
				flush_inst( (ulong)&p[0] );
			}
			d += c;
			size += c * sizeof(ulong);
		}
	}
//	OSI_Debugger(0);
}


/************************************************************************/
/*	entry								*/
/************************************************************************/

void
setup_mol_acceleration( void )
{
	splitmode_fixes();
	if( BootHResPresent("no_inst_replacements") ) {
		printm("Warning instruction replacement DISABLED!\n");
		return;
	}
	inst_replacements();
}
