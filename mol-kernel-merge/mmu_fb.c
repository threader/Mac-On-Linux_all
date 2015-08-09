/* 
 *   Copyright (C) 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   Offscreen framebuffer acceleration
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "kernel_vars.h"
#include "performance.h"
#include "tlbie.h"

typedef struct line_entry {
	short y1, y2;
	int dirty;
	unsigned long *slot;
	unsigned long pte0;
	unsigned long pte1;
	unsigned long ea;
} line_entry_t;

typedef struct fb_data {
	line_entry_t *line_table;
	int nrec;
	char *lv_base;		/* linux virtual of first entry in table */
} fb_data_t;

#define MMU		(kv->mmu)
#define DECLARE_FB	fb_data_t *fb = MMU.fb_data

int init_mmu_fb(kernel_vars_t * kv)
{
	/* setup_fb_acceleration does the initialization */
	return 0;
}

void cleanup_mmu_fb(kernel_vars_t * kv)
{
	DECLARE_FB;
	if (!fb)
		return;
	if (fb->line_table)
		vfree(fb->line_table);

	kfree(fb);
	MMU.fb_data = NULL;
}

void
video_pte_inserted(kernel_vars_t * kv, unsigned long lvptr, unsigned long * slot, unsigned long pte0,
		   unsigned long pte1, unsigned long ea)
{
	DECLARE_FB;
	int i;

	if (!fb)
		return;

	i = (lvptr - (unsigned long) fb->lv_base) >> 12;
	if (i >= 0 && i < fb->nrec) {
		line_entry_t *p = &fb->line_table[i];

		/* allow at most one video PTE to be mapped at any time */
		if (p->slot && (p->slot != slot || p->pte0 != pte0)) {
			BUMP(video_pte_reinsert);
			if (p->slot != slot)
				p->slot[0] = 0;
			__tlbie(p->ea);
			p->dirty = 1;
		}

		p->slot = slot;
		p->pte0 = pte0;
		p->pte1 = pte1 & ~PTE1_C;
		p->ea = ea;
	} else {
		printk("Warning: video_page outside range, %lx %p\n", lvptr,
		       fb->lv_base);
	}
}

/* setup/remove framebuffer acceleration */
void
setup_fb_acceleration(kernel_vars_t * kv, char *lvbase, int bytes_per_row,
		      int height)
{
	DECLARE_FB;
	int i, offs = (unsigned long) lvbase & 0xfff;
	line_entry_t *p;

	if (fb)
		cleanup_mmu_fb(kv);
	if (!lvbase)
		return;
	if (!(fb = kmalloc(sizeof(fb_data_t), GFP_KERNEL)))
		return;
	memset(fb, 0, sizeof(fb_data_t));
	MMU.fb_data = fb;

	fb->nrec = (bytes_per_row * height + offs + 0xfff) >> 12;
	if (!(p = (line_entry_t *) vmalloc(sizeof(line_entry_t) * fb->nrec))) {
		cleanup_mmu_fb(kv);
		return;
	}
	memset(p, 0, sizeof(line_entry_t) * fb->nrec);
	fb->line_table = p;

	fb->lv_base = (char *)((unsigned long) lvbase & ~0xfff);
	for (i = 0; i < fb->nrec; i++, p++) {
		p->y1 = (0x1000 * i - offs) / bytes_per_row;
		p->y2 = (0x1000 * (i + 1) - 1 - offs) / bytes_per_row;
		if (p->y1 < 0)
			p->y1 = 0;
		if (p->y2 >= height)
			p->y2 = height - 1;

		/* we should make sure the page is really unmapped here! */
		p->slot = NULL;
	}
}

/* return format is {startline,endline} pairs */
int get_dirty_fb_lines(kernel_vars_t * kv, short *userbuf, int num_bytes)
{
	DECLARE_FB;
	int i, n, s, start;
	line_entry_t *p;

	s = num_bytes / sizeof(short[2]) - 1;

	if (!fb || (uint) s <= 0)
		return -1;

	p = fb->line_table;
	for (start = -1, n = 0, i = 0; i < fb->nrec; i++, p++) {
		if (p->slot) {
			if (p->slot[0] != p->pte0) {
				/* evicted FB PTE */
				p->slot = NULL;
				p->dirty = 1;
				__tlbie(p->ea);
			} else if (p->slot[1] & BIT(24)) {	/* C-BIT */
				p->dirty = 1;
				__store_PTE(p->ea, p->slot, p->pte0, p->pte1);
				BUMP(fb_ptec_flush);
			}
		}
		if (p->dirty && start < 0)
			start = p->y1;
		else if (!p->dirty && start >= 0) {
			__put_user(start, userbuf++);
			__put_user(p->y2, userbuf++);
			start = -1;
			if (++n >= s)
				break;
		}
		p->dirty = 0;
	}
	if (start >= 0) {
		__put_user(start, userbuf++);
		__put_user(fb->line_table[fb->nrec - 1].y2, userbuf++);
		n++;
	}
	return n;
}
