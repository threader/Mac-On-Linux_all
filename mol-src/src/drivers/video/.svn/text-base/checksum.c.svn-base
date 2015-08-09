/* 
 *   Creation Date: <2002/04/27 19:17:28 samuel>
 *   Time-stamp: <2004/06/13 00:26:37 samuel>
 *   
 *	<checksum.c>
 *	
 *	Checksum calculations
 *   
 *   Copyright (C) 2002, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"

#include "checksum.h"
#include "video_module.h"
#include "mol_assert.h"

/* Defined in checksum_asm.S too... */
#define X_BLOCK_SIZE		64

typedef void vcksum_func( u32 *ctab, int ctab_add, int height, int fbadd, 
			  char *fbdata, u32 *dirtytable, u32 dirtybit, int block_width );

extern vcksum_func 	vchecksum_4, vchecksum_2, vchecksum_1;

struct video_cksum {
	video_desc_t 	*vmode;		/* video mode */

	int		n_blks;		/* width/X_BLOCK_SIZE + residue */
	int		n_residue;	/* equals 1 if width is not a multiple of X_BLOCK_SIZE */
	int		pixel_residue;	/* equals 1 if width is not a multiple of X_BLOCK_SIZE */

	int		bpp_shift;	/* bytes per pixel shift */
	
	u32 		*t;		/* table of size lines*n_blks */
	u32		*dirty;		/* indexed by lines */

	vcksum_func	*func;		/* lowlevel checksum func */
};

#ifndef __ppc__
void
vchecksum_1( u32 *ctab, int ctab_add, int height, int fbadd, 
	     char *fbdata, u32 *dirtytable, u32 dirtybit, int block_width )
{
	u32 *p = (u32*)fbdata;
	int i, j;
	
	for( i=0; i<height; i++, p=(u32*)((char*)p + fbadd) ) {
		u32 csum = 13;
		for( j=0 ; j < block_width/4; j+=4 )
			csum = (p[j] + p[j+1] + p[j+1] + ~(csum-p[j+2]) + (csum <<1)) ^ p[j+3];
		if (csum != *ctab) {
			dirtytable[i] |= dirtybit;
			*ctab = csum;
		}
		ctab = (u32*)((char*)ctab + ctab_add);
	}
}

void
vchecksum_2( u32 *ctab, int ctab_add, int height, int fbadd, 
	     char *fbdata, u32 *dirtytable, u32 dirtybit, int block_width )
{
	u32 *p = (u32*)fbdata;
	int i, j;
	
	for( i=0; i<height; i++, p=(u32*)((char*)p + fbadd) ) {
		u32 csum = 13;
		for( j=0 ; j < block_width/2; j+=4 )
			csum = (p[j] + p[j+1] + p[j+1] + ~(csum-p[j+2]) + (csum <<1)) ^ p[j+3];
		if (csum != *ctab) {
			dirtytable[i] |= dirtybit;
			*ctab = csum;
		}
		ctab = (u32*)((char*)ctab + ctab_add);
	}
}

void
vchecksum_4( u32 *ctab, int ctab_add, int height, int fbadd, 
	     char *fbdata, u32 *dirtytable, u32 dirtybit, int block_width )
{
	u32 *p = (u32*)fbdata;
	int i, j;
	
	for( i=0; i<height; i++, p=(u32*)((char*)p + fbadd) ) {
		u32 csum = 13;
		for( j=0 ; j < block_width; j+=4 )
			csum = (p[j] + p[j+1] + p[j+1] + ~(csum-p[j+2]) + (csum <<1)) ^ p[j+3];
		if (csum != *ctab) {
			dirtytable[i] |= dirtybit;
			*ctab = csum;
		}
		ctab = (u32*)((char*)ctab + ctab_add);
	}
}
#endif


video_cksum_t *
alloc_vcksum( video_desc_t *vmode )
{
	video_cksum_t *csum = calloc( 1, sizeof(video_cksum_t) );

	switch( vmode->depth ) {
	case 32:
	case 24:
		csum->func = &vchecksum_4;
		csum->bpp_shift=2;
		break;
	case 15:
	case 16:
		csum->func = &vchecksum_2;
		csum->bpp_shift=1;
		break;
	case 8:
		csum->func = &vchecksum_1;
		csum->bpp_shift=0;
		break;
	}
	csum->n_blks = vmode->w / X_BLOCK_SIZE;
	csum->pixel_residue = vmode->w % X_BLOCK_SIZE;

	/* The assembly implementation requires pixel_residue * bpp to be at least 16 */
	if( (csum->pixel_residue << csum->bpp_shift) < 16 )
		csum->pixel_residue = 0;

	csum->n_residue = csum->pixel_residue? 1:0;
	csum->n_blks += csum->n_residue;

	csum->t = malloc( csum->n_blks * vmode->h * sizeof(ulong) );
	csum->dirty = malloc( vmode->h * sizeof(ulong) );
	csum->vmode = vmode;

	memset( csum->dirty, 0, vmode->h * sizeof(ulong) );
	return csum;
}

void
free_vcksum( video_cksum_t *csum )
{
	assert( csum );

	free( csum->t );
	free( csum->dirty );
	free( csum );
}

void
vcksum_redraw( video_cksum_t *csum, int y, int height, cksum_redraw_func *redraw_func ) 
{
	u32 *d = &csum->dirty[y];
	ulong mask, b;
	uint x, j, yy, w;

	for( yy=0; yy < height; ) {
		if( !d[yy] ) {
			yy++;
			continue;
		}

		/* Start of block */
		for( x=0, mask=1; !(mask & d[yy]) && mask ; mask=mask<<1, x++ )
			;

		/* length of block */
		b = mask << 1;
		w = 1;
		while( b & d[yy] ) {
			mask |= b;
			w++;
			b = b << 1;
		}

		/* height of block */
		for( j=yy; j<height && (mask & d[j]) == mask ; j++ )
			d[j] &= ~mask;

		w *= X_BLOCK_SIZE;
		x *= X_BLOCK_SIZE;

		/* Last block is not necessary of X_BLOCK_SIZE... */
		if( x+w > csum->vmode->w )
			w = csum->vmode->w - x;

		(*redraw_func)( x, yy+y, w, j-yy );
	}
}

void
vcksum_calc( video_cksum_t *csum, int y, int height )
{
	video_desc_t *vmode = csum->vmode;
	char *s = vmode->lvbase + vmode->offs + vmode->rowbytes * y;
	uint n_blks = csum->n_blks;
	u32 *t = &csum->t[y*n_blks];
	uint limit = n_blks - csum->n_residue;
	uint b, x;
	
	if( !height )
		return;

	/* checksum for complete X_BLOCK_SIZE blocks */
	for( b=1, x=0; x<limit; x++, b=b<<1 ) {
		csum->func( t, (n_blks<<2), height, vmode->rowbytes, s,
			    &csum->dirty[y], b, X_BLOCK_SIZE );
		s += (X_BLOCK_SIZE<<csum->bpp_shift);
		t++;
	}
	if( csum->n_residue ) {
		csum->func( t, (n_blks<<2), height, vmode->rowbytes, s,
			    &csum->dirty[y], b, csum->pixel_residue );
	}
}
