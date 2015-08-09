/* 
 *   Creation Date: <2002/05/29 22:54:07 samuel>
 *   Time-stamp: <2002/06/10 18:45:41 samuel>
 *   
 *	<mtable_dbg.c>
 *	
 *	mtable debug
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef DBG_FIRST	/* this file is included twice from mtable.c */
#define DBG_FIRST

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define printk		printf
#define kmalloc(a,b)	malloc( a )
#define kmalloc_mol(a)	malloc( a )
#define kfree(a)	free( a )
#define kfree_mol(a)	free( a )
#define vmalloc_mol(a)	malloc( a )
#define vfree_mol(a)	free( a )
#define panic(s)	do { printf("%s", s ); exit(1); } while(0)
#define pent_inserted	_pent_inserted
#define BUMP(s)		do { } while(0)
typedef struct {
	struct {
		struct vsid_info *vsid_info;
	} mmu;
} kernel_vars_t;

extern int		alloc_mol_context( int n );
extern int 		munge_mol_context( int mol_vsid );
extern void		clear_vsid_refs( kernel_vars_t *kv );

ulong 			*hw_hash_base;
ulong 			hw_pte_offs_mask;

#define MOL_BIT(n)		(1U<<(31-(n)))

static inline void __tlbie( ulong ea ) {
	/* printk("tlbie %08lX\n", ea ); */
}

#define ZERO_PTE(pent)	do { /*printk("ZERO_PTE %08X %d\n", pent, (pent & PENT_INDEX_MASK) ); */} while(0)

#else

#define pte_inserted( kv,b,c,d,e,f,g ) \
	do { mtable_memory_check(kv); pte_inserted(kv,b,c,d,e,f,g); } while(0)

/************************************************************************/
/*	D E B U G							*/
/************************************************************************/

#define DBG_HASH_SIZE	(1024 * 64)
#define PTENUM( n ) 	(ulong*)((char*)hw_hash_base + ((n*8) & (DBG_HASH_SIZE-1)) )
#define LVBASE		((char*)0x40000000)
#define LVNUM( n )	((char*)LVBASE + (n)*0x1000 )
#define LVSIZE		(0x100000 * 64)

static void
ea_check( vsid_ent_t *r )
{
	pent_table_t *t;
	pterec_t *pr;
	uint pent;
	int i,j;
	
	for( i=0; i<64; i++ ){
		if( !(t=r->lev2[i]) )
			continue;

		for( j=0; j<32; j++ ) {
			if( !(pr=t->pelist[j]) )
				continue;
			printk("EA: ");
			do {
				pent = pr->pent;
				printk(" %c%c %08X", 
				       (pent & PENT_SV_BIT) ? 's' : 'u',
				       (pent & PENT_LV_HEAD) ? 'h' : '_',
				       (j<<12) | (i<<(12+5)) | (pent & PENT_TOPEA_MASK));

				pr=pr->ea_next;
			} while( !(pent & PENT_EA_LAST) );
			printk("\n");
		}
	}
}

static void
lv_check( pte_lvrange_t *lvr )
{
	int pent, n, i, nel = (lvr->size >> 12);
	pterec_t *pr, *pr2;
	
	for( i=0; i<nel; i++ ){
		pr2 = pr = &lvr->pents[i];
		pent = pr->pent;
		
		if( !(pent & PENT_LV_HEAD) ) {
			printk("BUG: lv_check, LV_HEAD inconsistency\n");
		}
		if( pr->lv_next == pr2 && (pent & PENT_UNUSED) )
			continue;

		if( pent & PENT_UNUSED ) {
			n=0;
			printk("LV [Free] ");
		} else {
			n=1;
			printk("LV [Used] ");
		}
		printk("%d: ", i );

		for( pr=pr->lv_next; pr!=pr2 ; pr=pr->lv_next ) {
			pent = pr->pent;

			if( pent & PENT_UNUSED )
				printk("BUG: PENT_UNUSED set\n");
			if( pent & PENT_LV_HEAD )
				printk("BUG: PENT_LV_HEAD set\n");
			n++;
		}
		printk("%d entries\n", n );
	}
}


static int
dbg_count_all( vsid_ent_t *r )
{
	int i,j, n=0;
	pent_table_t *t;
	pterec_t *pr;
	uint pent;
	
	for( i=0; i<64; i++ ){
		if( !(t=r->lev2[i]) )
			continue;

		for( j=0; j<32; j++ ) {
			if( !(pr=t->pelist[j]) )
				continue;
			do {
				pent = pr->pent;
				pr=pr->ea_next;
				n++;
			} while( !(pent & PENT_EA_LAST) );
		}
	}
	return n;
}

static int
dbg_count_free( vsid_info_t *vi )
{
	pterec_t *r=vi->free_pents;
	int n;
	
	for( n=0; r; r=r->lv_next, n++ )
		;
	return n;
}

static void
dbg_count( kernel_vars_t *kv, vsid_ent_t *r  )
{
	printk("count = %d (%d,%d)\n", dbg_count_all(r) + dbg_count_free(MMU.vsid_info),
	       dbg_count_all(r), dbg_count_free(MMU.vsid_info) );
}

static inline void
check( kernel_vars_t *kv, vsid_ent_t *r, pte_lvrange_t *lvr )
{
	printk("------------------\n");
	ea_check(r);
	lv_check(lvr);
	dbg_count( kv,r );
	printk("------------------\n");
}

void
dbg_main2( kernel_vars_t *kv, vsid_ent_t *r, pte_lvrange_t *lvr )
{
	//lv_check(lvr);
	pte_inserted( kv, 0x00001000, LVNUM(0), lvr, r, PTENUM(1), 0 );
	pte_inserted( kv, 0x00002000, LVNUM(1), lvr, r, PTENUM(2), 0 );
	pte_inserted( kv, 0x00001000, LVNUM(2), lvr, r, PTENUM(2), 0 );
	pte_inserted( kv, 0x00001000, LVNUM(2), lvr, r, PTENUM(2), 0 );
	pte_inserted( kv, 0x00003000, LVNUM(3), lvr, r, PTENUM(2), 0 );
	check( kv, r, lvr );
	pte_inserted( kv, 0x00003000, LVBASE, lvr, r, PTENUM(3), 0 );
	pte_inserted( kv, 0x00002000, LVBASE, lvr, r, PTENUM(4), 1 );
	pte_inserted( kv, 0xfffff000, LVBASE, lvr, r, PTENUM(4), 1 );
	pte_inserted( kv, 0x00003000, LVBASE+0x1000, lvr, r, PTENUM(5), 1 );

	flush_ea_range( kv, 0, 0x1000000 );
}

void
dbg_random( kernel_vars_t *kv, vsid_ent_t *r, pte_lvrange_t *lvr )
{
	int i,b,c,d,e;
#if 1
	for( i=0; i<1000000; i++ ) {
		b = random() & 0xfff;
		c = random() & 0xff;
		d = random() & 0xf;
		e = random() & 1;
		pte_inserted( kv, 0x1000 * b, LVNUM(c), lvr, r, PTENUM(d), e );
	}
#else 
	for( i=0; i<12; i++ ) {
		pterec_t *p;
		b = random() & 0x1;
		c = random() & 0x1;
		d = random() & 0;
		e = random() & 1;
		pte_inserted( kv, 0x1000 * b, LVNUM(c), lvr, r, PTENUM(d), e );
		printk("\nea: %d lv %d, sv %d\n", b,c,e );
		printk("%d---------------------------\n", i);
		ea_check(r);
		lv_check(lvr);
		p = &lvr->pents[0];
		do {
			p=p->lv_next;
			printk("%p ", p );
		} while( p != &lvr->pents[0] );
		printk("\n");
		fflush(stdout);
	}
#endif
	flush_ea_range( kv, 0, 0x1000000 );
	//check( kv, r, lvr );
}


void
dbg_main( kernel_vars_t *kv, vsid_ent_t *r, pte_lvrange_t *lvr )
{
#if 0
	int i;
	for( i=1; i<128; i++ )
		pte_inserted( kv, 0x00001000 *i, LVBASE, lvr, r, PTENUM(i), 0 );
	for( i=1; i<5; i++ )
		pte_inserted( kv, 0x00001000 *i, LVBASE, lvr, r, PTENUM(i), 1 );
	for( i=1; i<5; i++ )
		pte_inserted( kv, 0x00001000 *i, LVBASE+0x1000*i, lvr, r, PTENUM(i), 1 );
	for( i=1; i<5; i++ )
		pte_inserted( kv, 0x01001000 *i, LVBASE, lvr, r, PTENUM(i), 0 );
	for( i=1; i<5; i++ )
		pte_inserted( kv, 0x02001000 *i, LVBASE, lvr, r, PTENUM(i), 0 );
#endif
	ea_check(r);
	lv_check(lvr);

	flush_lvptr( kv, (ulong)LVBASE + 0x0000 );
#if 0
	printk("------------\n");
	lv_check(lvr);

	for( i=1; i<5; i++ )
		flush_vsid_ea( kv, 0x1234, 0x00001000 *i );
	for( i=1; i<5; i++ )
		pte_inserted( kv, 0x00001000 *i, LVBASE, lvr, r, PTENUM(i), 0 );
	for( i=1; i<5; i++ )
		flush_vsid_ea( kv, 0x1234, 0x00001000 *i );

	ea_check(r);
	lv_check(lvr);
//#if 0
	pte_inserted( kv, 0x01001000, NULL, NULL, r, PTENUM(2), 0 );
	pte_inserted( kv, 0x00002000, NULL, NULL, r, PTENUM(3), 0 );
	pte_inserted( kv, 0x00002000, NULL, NULL, r, PTENUM(4), 0 );
	flush_vsid_ea( kv, 0x1234, 0x1000 );
	flush_vsid_ea( kv, 0x1234, 0x01001000 );
	flush_vsid_ea( kv, 0x1234, 0x01001000 );
#endif
}

int
main( int argc, char **argv )
{
	pte_lvrange_t *lvrange;
	kernel_vars_t kv;
	vsid_ent_t *r;
      
	init_mtable( &kv );
	lvrange = register_lvrange( &kv, (char*)LVBASE, LVSIZE );
	r = alloc_vsid_ent( &kv, 0x1234 );

	// Fake a hash table
	hw_hash_base = malloc( DBG_HASH_SIZE );	/* 64 K */
	hw_pte_offs_mask = (DBG_HASH_SIZE - 1) & ~0x7;
	memset( hw_hash_base, 0, DBG_HASH_SIZE );

	ea_check(r);
	dbg_count( &kv, r);
	printk("-------------------------------------------\n");

	dbg_random( &kv, r, lvrange );
	printk("main-------------------------------------------\n");
	dbg_main( &kv, r, lvrange );
//	printk("main2-------------------------------------------\n");
//	dbg_main2( &kv, r, lvrange );
	printk("-------------------------------------------\n");
	dbg_count( &kv, r );

	cleanup_mtable( &kv);

	free( hw_hash_base );
	return 0;
}

int
alloc_mol_context( int n )
{
	static int next=1;
	int ret = next;
	next += n;
	return ret;
}
int
munge_mol_context( int n )
{
	return (~n) & 0xffffff;
}

void
clear_vsid_refs( kernel_vars_t *kv )
{
	/* All vsid entries have been flushed; clear dangling pointers */
	printk("clear_vsid_refs\n");
}

#endif
