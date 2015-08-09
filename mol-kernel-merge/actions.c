/* 
 *   Copyright (C) 2001, 2002, 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   Handle assambly actions (relocations, exception vector
 *   hooking, lowmem relocations and other stuff)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include <linux/kernel.h>

#include "kernel_vars.h"
#include "map.h"
#include "actions.h"
#include "asmfuncs.h"
#include "misc.h"

/* globals */
int reloc_virt_offs;

static char *code_base;
static unsigned int code_size;

/* some opcodes */
#define OPCODE_ADDIS( dreg, sreg, hi_val )	((15<<26) | ((dreg)<<21) | ((sreg)<<16) | (hi_val))
#define OPCODE_LIS( dreg, hi_val )		OPCODE_ADDIS( dreg, 0, hi_val )
#define OPCODE_ORI( dreg, sreg, val )		((24<<26) | ((dreg)<<16) | ((sreg)<<21) | (val))
#define OPCODE_MTSPRG2( sreg )			(0x7c1243a6 + ((sreg)<<21))

/************************************************************************/
/*	lowmem allocations (allocates within the first 32 MB of RAM)	*/
/************************************************************************/

/* The low-level assembly code need to be located in memory which is
 * physically continuous. The kernel exception vector are patched
 * through pseudo symbols (action symbols).
 */

#define MAX_NUM_CLEANUP_HANDLERS	32

typedef struct {
	char *lowmem_addr;
	int alloc_size;
	int alloc_method;

	unsigned long *inst_addr;	/* These fields are used */
	unsigned long opcode;		/* by the hook code */
} cleanup_entry_t;

static int num_cleanup_entries;
static cleanup_entry_t cleanup_table[MAX_NUM_CLEANUP_HANDLERS];
static unsigned long lowmem_phys_cursor;

/* Memory mapping of exception vectors */
static unsigned long lowmem_phys_base;
static char *lowmem_virt_base;
static void *lowmem_mapping;

static inline unsigned long *lowmem_phys_to_virt(unsigned long paddr)
{
	return (unsigned long *) (lowmem_virt_base + (paddr - lowmem_phys_base));
}

static inline unsigned long lowmem_tophys(void *vaddr)
{
	return lowmem_phys_base + ((unsigned long) vaddr - (unsigned long) lowmem_virt_base);
}

static void lowmem_initialize(void)
{
	if (num_cleanup_entries) {
		printk("Internal error in lowmem_initialize\n");
		return;
	}
	lowmem_phys_cursor = 0x100;

	/* In Darwin, the mapping will fail if we put lowmem_phys_base to zero */
	/* FIXME - can we map this to 0 now in linux? */
	lowmem_phys_base = 0x100;
	lowmem_mapping =
	    map_phys_range(lowmem_phys_base, 0x4000, &lowmem_virt_base);
}

static char *lowmem_alloc(int size, cleanup_entry_t ** ret_ce)
{
	unsigned long *pstart;
	cleanup_entry_t ce;
	int found = 0;

	memset(&ce, 0, sizeof(ce));
	if (ret_ce)
		*ret_ce = NULL;

	if (num_cleanup_entries >= MAX_NUM_CLEANUP_HANDLERS) {
		printk("MOL: Need more cleanup slots!\n");
		return NULL;
	}

	/* Find big enough empty piece of memory */
	if (size < 0x10)
		size = 0x10;

	pstart = lowmem_phys_to_virt(lowmem_phys_cursor);
	pstart = (unsigned long *) (((unsigned long) pstart + 0xf) & ~0xf);
	for (; lowmem_phys_cursor < 0x3000; lowmem_phys_cursor += 4) {
		unsigned long *p = lowmem_phys_to_virt(lowmem_phys_cursor);
		if (((int)p - (int)pstart) >= size) {
			found = 1;
			break;
		}
		if (*p) {
			pstart =
			    (unsigned long *) (((unsigned long) p + sizeof(unsigned long) + 0xf) &
				       ~0xf);
			continue;
		}
	}
	if (!found) {
		printk("MOL: Did not find an empty piece of lowmem memory!\n");
		return NULL;
	}
	/* printk("lowmem alloc: %08lX\n", pstart ); */

	ce.lowmem_addr = (char *)pstart;
	ce.alloc_method = 0;
	ce.alloc_size = size;
	/* printk("lowmem-alloc SPACE %X bytes at %08lX\n", size, (unsigned long)pstart ); */

	cleanup_table[num_cleanup_entries] = ce;
	if (ret_ce)
		*ret_ce = &cleanup_table[num_cleanup_entries];
	num_cleanup_entries++;

	return ce.lowmem_addr;
}

static void lowmem_free_all(void)
{
	cleanup_entry_t *ce = &cleanup_table[0];
	int i;

	for (i = 0; i < num_cleanup_entries; i++, ce++)
		memset(ce->lowmem_addr, 0, ce->alloc_size);

	num_cleanup_entries = 0;

	if (lowmem_mapping) {
		/* unmap_phys_range(lowmem_mapping); */
		lowmem_mapping = NULL;
	}
}

/************************************************************************/
/*	helper functions						*/
/************************************************************************/

static action_pb_t *find_action(int action, int index)
{
	extern int r__actions_offs_section[], r__actions_offs_section_end[];
	extern char *r__actions_section[];
	const int n =
	    ((int)r__actions_offs_section_end -
	     (int)r__actions_offs_section) / sizeof(int);
	int i, *op = r__actions_offs_section;

	for (i = 0; i < n; i++) {
		action_pb_t *p =
		    (action_pb_t *) ((char *)r__actions_section + op[i]);

		if (p->action != action || index--)
			continue;
		return p;
	}
	return NULL;
}

static int relocate_inst(unsigned long * opc_ptr, unsigned long from, unsigned long to)
{
	unsigned long opcode = *opc_ptr;
	int offs = -1;

	/* XXX: UNTESTED if target instruction is a branch */

	/* Since we use this on the _first_ instruction of the
	 * exception vector, it can't touch LR/CR. Thus, we 
	 * only look for unconditional, relative branches.
	 */

	/* relativ branch b */
	if ((opcode & 0xfc000003) == (18 << 26)) {
		offs = (opcode & 0x03fffffc);
		/* sign extend */
		if (offs & 0x03000000)
			offs |= ~0x03ffffff;
	}
	/* unconditional, relativ bc branch (b 0100 001z1zz ...) */
	if ((opcode & 0xfe800003) == 0x42800000) {
		offs = (opcode & 0xfffc);
		if (offs & 0x8000)
			offs |= ~0xffff;
	}
	/* construct the absolute branch */
	if (offs != -1) {
		int dest = from + offs;
		if (dest < 0 || dest > 33554431) {
			printk
			    ("relocation of branch at %08lX to %08lX failed\n",
			     from, to);
			return 1;
		}
		/* absolute branch */
		*opc_ptr = ((18 << 26) + 2) | dest;
	}
	return 0;
}

/************************************************************************/
/*	actions								*/
/************************************************************************/

typedef int (*action_func_t) (int action, unsigned long * target, const int *pb);

static int action_li_phys(int action, unsigned long * target, const int *pb)
{
	int r = pb[0] & 0x1f;
	unsigned long addr = pb[1] + virt_to_phys(code_base);

	/* target[0] = addis r,0,addr@h ; target[1] = ori r,r,addr@l */
	target[0] = (15 << 26) | (r << 21) | (addr >> 16);
	target[1] = (24 << 26) | (r << 21) | (r << 16) | (addr & 0xffff);

	/* printk("ACTION_LI_PHYS %d %08lX\n", dreg, addr ); */
	return 0;
}

static int action_lwz_physaddr_r(int action, unsigned long * target, const int *pb)
{
	unsigned long addr = pb[1] + virt_to_phys(code_base);
	int dr = (pb[0] >> 5) & 0x1f;
	int r = pb[0] & 0x1f;
	short low = (addr & 0xffff);

	/* target[0] = addis dr,r,addr@h ; target[1] = lwz dr,addr@l(dr) */
	target[0] = (15 << 26) | (dr << 21) | (r << 16) | ((addr - low) >> 16);
	target[1] = (32 << 26) | (dr << 21) | (dr << 16) | ((int)low & 0xffff);

	/* printk("ACTION_LI_PHYS %d %08lX\n", dreg, addr ); */
	return 0;
}

static int action_specvar(int action, unsigned long * target, const int *pb)
{
	int r = pb[0] & 0x1f;
	unsigned long addr;

	switch (pb[1]) {
	case SPECVAR_SESSION_TABLE:
		addr = virt_to_phys(g_sesstab);
		break;
	default:
		return 1;
	}

	if (action == ACTION_LIS_SPECVAR_H) {
		/* target[0] = addis r,0,addr@h */
		target[0] = OPCODE_LIS(r, (addr >> 16) & 0xffff);
		return 0;
	}
	if (action == ACTION_ORI_SPECVAR_L) {
		/* target[0] = ori rD,rS,addr@l */
		int rD = (pb[0] >> 5) & 0x1f;
		target[0] = OPCODE_ORI(rD, r, (addr & 0xffff));
		return 0;
	}
	return 1;
}

/* Note: this only works under linux */
static int action_tophysvirt(int action, unsigned long * target, const int *pb)
{
	unsigned long addr = virt_to_phys(0);
	int dr = (pb[0] >> 5) & 0x1f;
	int sr = pb[0] & 0x1f;

	if (action == ACTION_TOVIRT)
		addr = -addr;

	/* target[0] = addis dr,sr,(tophys(0))@hi */
	target[0] = OPCODE_ADDIS(dr, sr, (addr >> 16) & 0xffff);
	return 0;
}

/* pb[] = { vector, size, vret_offs, ...hook_code...  }  */
static int action_reloc_hook(int action, unsigned long * hookcode, const int *pb)
{
	unsigned long addr, inst, vector = pb[0], size = pb[1], vret_offs = pb[2];
	cleanup_entry_t *clean;
	unsigned long *vector_virt, *target;
	action_pb_t *apb;
	char *lowmem;
	int i;

	/* Virtual address of exception vector */
	vector_virt = lowmem_phys_to_virt(vector);

	/* address of the vector hook code */
	addr = virt_to_phys((char *)hookcode);

	/* allocate lowmem and add cleanup handler */
	if (!(lowmem = lowmem_alloc(size, &clean)))
		return 1;

	/* printk("ACTION_RELOC_HOOK: %lx, %lx, %lx, %lx %p\n", vector, size, vret_action, vret_offs, lowmem); */

	memcpy(lowmem, &pb[3], size);

	/* perform the vret_action */
	for (i = 0; (apb = find_action(ACTION_VRET, i)); i++) {
		if (apb->params[0] != vector)
			continue;

		/* insert the absolut branch */
		target = (unsigned long *) (code_base + apb->offs);
		*target = ((18 << 26) + 2) | lowmem_tophys(lowmem + vret_offs);
		flush_icache_range((unsigned long) target, (unsigned long) target + 4);
		/* printk("'ba xxx' added (opcode %08lX at %p)\n", *target, target ); */
	}

	/* fix the hook address in the glue code */
	target = (unsigned long *) lowmem;
	target[1] = (target[1] & ~0xffff) | (addr >> 16);	/* target[0] = addis r3,0,0 */
	target[3] = (target[3] & ~0xffff) | (addr & 0xffff);	/* target[1] = ori r3,r3,0 */

	/* relocate instruction to be overwritten with a branch */
	inst = *vector_virt;
	clean->opcode = inst;
	if (relocate_inst(&inst, vector, lowmem_tophys(lowmem + vret_offs)))
		return 1;
	*(unsigned long *) (lowmem + vret_offs) = inst;
	flush_icache_range((unsigned long) lowmem, (unsigned long) lowmem + size);

	/* insert branch, 'ba lowmem_ph' */
	*(volatile unsigned long *)vector_virt = 0x48000002 + lowmem_tophys(lowmem);
	flush_icache_range((unsigned long) vector_virt, (unsigned long) vector_virt + 4);

	/* we are in business! */
	clean->inst_addr = vector_virt;
	return 0;
}

/* pb = { size, where_to_store_lowmem_addr, ...code... } */
static int action_reloc_low(int action, unsigned long * dummy, const int *pb)
{
	int size = pb[0];
	char **func_ptr = (char **)pb[1];
	char *lowmem;

	if (!(lowmem = lowmem_alloc(size, NULL)))
		return 1;
	memcpy(lowmem, (char *)&pb[2], size);

	flush_icache_range((unsigned long) lowmem, (unsigned long) lowmem + size);
	*func_ptr = lowmem;
	return 0;
}

/* pb = { symind, size, fret_offset, codeglue... } */
static int action_hook_function(int action, unsigned long * hookcode, const int *pb)
{
	unsigned long addr, fhook = pb[0], size = pb[1], fret_offs = pb[2];
	unsigned long *target, inst;
	char *lowmem, *func_addr = NULL;
	cleanup_entry_t *clean;

	switch (fhook) {
	case FHOOK_FLUSH_HASH_PAGE:
		func_addr = (char *)flush_hash_pages;
		break;
	default:
		printk("Bad fhook index %ld\n", fhook);
		return 1;
	}

	/* this does not have to be in lowmem, but it is simpler with a unified approach */
	if (!(lowmem = lowmem_alloc(size, &clean)))
		return 1;

	/* printk("ACTION_HOOK_FUNCTION: %lx, %lx, %lx %p\n", fhook, size, fret_offs, lowmem); */

	memcpy(lowmem, &pb[3], size);

	/* Fix the hook address in the glue code */
	target = (unsigned long *) lowmem;
	addr = (unsigned long) hookcode;
	target[1] = (target[1] & ~0xffff) | (addr >> 16);	/* target[1] = addis rX,0,0 */
	target[2] = (target[2] & ~0xffff) | (addr & 0xffff);	/* target[2] = ori rX,rX,0 */

	/* relocate overwritten instruction and add relative return branch */
	inst = *(unsigned long *) func_addr;
	clean->opcode = inst;
	if (relocate_inst(&inst, (unsigned long) func_addr, (unsigned long) lowmem + fret_offs))
		return 1;
	target = (unsigned long *) (lowmem + fret_offs);
	target[0] = inst;
	target[1] =
	    (18 << 26) |
	    (((unsigned long) func_addr - (unsigned long) & target[1] +
	      sizeof(long)) & 0x03fffffc);
	flush_icache_range((unsigned long) lowmem, (unsigned long) lowmem + size);
	_sync();

	/* insert relative branch, 'b lowmem' */
	*(volatile unsigned long *)func_addr =
	    (18 << 26) | ((lowmem - func_addr) & 0x03fffffc);
	flush_icache_range((unsigned long) func_addr, (unsigned long) func_addr + 4);

	_sync();

	/* we are in business! */
	clean->inst_addr = (unsigned long *) func_addr;
	return 0;
}

static int action_noaction(int action, unsigned long * hookcode, const int *pb)
{
	return 0;
}

static action_func_t actiontable[MAX_NUM_ACTIONS] = {
	[ACTION_LI_PHYS] = action_li_phys,
	[ACTION_LWZ_PHYSADDR_R] = action_lwz_physaddr_r,
	[ACTION_TOPHYS] = action_tophysvirt,
	[ACTION_TOVIRT] = action_tophysvirt,
	[ACTION_RELOC_HOOK] = action_reloc_hook,
	[ACTION_RELOCATE_LOW] = action_reloc_low,
	[ACTION_HOOK_FUNCTION] = action_hook_function,
	[ACTION_VRET] = action_noaction,
	[ACTION_FIX_SPRG2] = action_noaction,	/* Only used in Darwin */
	[ACTION_LIS_SPECVAR_H] = action_specvar,
	[ACTION_ORI_SPECVAR_L] = action_specvar,
};

/************************************************************************/
/*	write/remove hooks						*/
/************************************************************************/

static int relocate_code(void)
{
	extern char r__reloctable_start[], r__reloctable_end[];

	code_size = r__reloctable_end - r__reloctable_start;

	if (!(code_base = kmalloc(code_size, GFP_KERNEL)))
		return 1;

	memcpy(code_base, r__reloctable_start, code_size);
	reloc_virt_offs = (int)code_base - (int)r__reloctable_start;
	return 0;
}

int perform_actions(void)
{
	action_pb_t *pb;
	int action, i;

	if (relocate_code())
		return 1;
	lowmem_initialize();

	for (action = 0; action < MAX_NUM_ACTIONS; action++) {
		for (i = 0; (pb = find_action(action, i)); i++) {
			unsigned long *target = (unsigned long *) (code_base + pb->offs);

			if (pb->offs > code_size) {
				printk("OFFSET ERROR!\n");
				goto error;
			}

			if (!actiontable[action])
				goto error;
			if ((*actiontable[action]) (action, target, pb->params))
				goto error;
		}

		/* we need to flush the icache before the hook actions are performed */
		if (action == FLUSH_CACHE_ACTION)
			flush_icache_range((unsigned long) code_base,
					 (unsigned long) code_base + code_size);
	}
	/* to be on the safe side, flush the cache once more */
	flush_icache_range((unsigned long) code_base, (unsigned long) code_base + code_size);
	return 0;
      error:
	printk("MOL: action %d error\n", action);
	cleanup_actions();
	return 1;
}

void cleanup_actions(void)
{
	cleanup_entry_t *ce = &cleanup_table[0];
	int i;

	for (i = 0; i < num_cleanup_entries; i++, ce++) {
		if (ce->inst_addr) {
			*(volatile unsigned long *)ce->inst_addr =
			    cleanup_table[i].opcode;
			flush_icache_range((unsigned long) ce->inst_addr,
					 (unsigned long) ce->inst_addr + 4);
		}
	}
	lowmem_free_all();
	kfree(code_base);
}
