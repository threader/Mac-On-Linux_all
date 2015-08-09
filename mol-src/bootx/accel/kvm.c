/*
 *	<kvm.c>
 *
 *	KVM magic page handler
 *
 *   Copyright (C) 2010 Alexander Graf <agraf@suse.de>
 *
 */

#include "sl.h"
#include "osi_calls.h"
#include "mac_registers.h"
#include "mol.h"
#include "emuaccel_sh.h"
#include "boothelper_sh.h"

#define KVM_MAGIC_PAGE		0xffff9000
#define magic_var(x) KVM_MAGIC_PAGE + offsetof(struct kvm_vcpu_arch_shared, x)

#define KVM_INST_LWZ		0x80000000
#define KVM_INST_STW		0x90000000
#define KVM_INST_LD		0xe8000000
#define KVM_INST_STD		0xf8000000
#define KVM_INST_NOP		0x60000000
#define KVM_INST_B		0x48000000
#define KVM_INST_B_MASK		0x03ffffff
#define KVM_INST_B_MAX		0x01ffffff

#define KVM_MASK_RT		0x03e00000
#define KVM_RT_30		0x03c00000
#define KVM_MASK_RB		0x0000f800
#define KVM_INST_MFMSR		0x7c0000a6
#define KVM_INST_MFSPR_SPRG0	0x7c1042a6
#define KVM_INST_MFSPR_SPRG1	0x7c1142a6
#define KVM_INST_MFSPR_SPRG2	0x7c1242a6
#define KVM_INST_MFSPR_SPRG3	0x7c1342a6
#define KVM_INST_MFSPR_SRR0	0x7c1a02a6
#define KVM_INST_MFSPR_SRR1	0x7c1b02a6
#define KVM_INST_MFSPR_DAR	0x7c1302a6
#define KVM_INST_MFSPR_DSISR	0x7c1202a6

#define KVM_INST_MTSPR_SPRG0	0x7c1043a6
#define KVM_INST_MTSPR_SPRG1	0x7c1143a6
#define KVM_INST_MTSPR_SPRG2	0x7c1243a6
#define KVM_INST_MTSPR_SPRG3	0x7c1343a6
#define KVM_INST_MTSPR_SRR0	0x7c1a03a6
#define KVM_INST_MTSPR_SRR1	0x7c1b03a6
#define KVM_INST_MTSPR_DAR	0x7c1303a6
#define KVM_INST_MTSPR_DSISR	0x7c1203a6

#define KVM_INST_TLBSYNC	0x7c00046c
#define KVM_INST_DCBA		0x7c0005ec
#define KVM_INST_MTMSRD_L0	0x7c000164
#define KVM_INST_MTMSRD_L1	0x7c010164
#define KVM_INST_MTMSR		0x7c000124
#define KVM_INST_MTSRIN		0x7c0001e4

#define u8	unsigned char
#define __u8	unsigned char
#define u16	unsigned short
#define __u16	unsigned short
#define u32	unsigned int
#define __u32	unsigned int
#define u64	unsigned long long
#define __u64	unsigned long long

struct kvm_vcpu_arch_shared {
	__u64 scratch1;
	__u64 scratch2;
	__u64 scratch3;
	__u64 critical;		/* Guest may not get interrupts if == r1 */
	__u64 sprg0;
	__u64 sprg1;
	__u64 sprg2;
	__u64 sprg3;
	__u64 srr0;
	__u64 srr1;
	__u64 dar;
	__u64 msr;
	__u32 dsisr;
	__u32 int_pending;	/* Tells the guest if we have an interrupt */
	__u32 sr[16];
};

#define KVM_SC_MAGIC_R0		0x4b564d21 /* "KVM!" */
#define HC_VENDOR_KVM		(42 << 16)
#define HC_EV_SUCCESS		0
#define HC_EV_UNIMPLEMENTED	12
#define KVM_FEATURE_MAGIC_PAGE	1
#define KVM_HC_FEATURES			3
#define KVM_HC_PPC_MAP_MAGIC_PAGE	4
#define INST_RFI		0x4c000064

static char *kvm_tmp;
static int kvm_tmp_index;
static int kvm_patching_worked = 1;


static inline unsigned int get_rt(u32 inst)
{
	return (inst >> 21) & 0x1f;
}

static inline unsigned int get_rb(u32 inst)
{
	return (inst >> 11) & 0x1f;
}


static void flush_inst(ulong addr)
{
	__asm__ volatile("dcbst 0,%0 ; sync ; icbi 0,%0 ; sync ; isync" :: "r" (addr));
}

static inline void kvm_patch_ins(u32 *inst, u32 new_inst)
{
	*inst = new_inst;
	flush_inst((ulong)inst);
}

static void kvm_patch_ins_ld(u32 *inst, long addr, u32 rt)
{
	kvm_patch_ins(inst, KVM_INST_LWZ | rt | ((addr + 4) & 0x0000fffc));
}

static void kvm_patch_ins_lwz(u32 *inst, long addr, u32 rt)
{
	kvm_patch_ins(inst, KVM_INST_LWZ | rt | (addr & 0x0000ffff));
}

static void kvm_patch_ins_std(u32 *inst, long addr, u32 rt)
{
	kvm_patch_ins(inst, KVM_INST_STW | rt | ((addr + 4) & 0x0000fffc));
}

static void kvm_patch_ins_stw(u32 *inst, long addr, u32 rt)
{
	kvm_patch_ins(inst, KVM_INST_STW | rt | (addr & 0x0000fffc));
}

static void kvm_patch_ins_nop(u32 *inst)
{
	kvm_patch_ins(inst, KVM_INST_NOP);
}

static void kvm_patch_ins_b(u32 *inst, int addr)
{
	kvm_patch_ins(inst, KVM_INST_B | (addr & KVM_INST_B_MASK));
}

static u32 *kvm_alloc(int len)
{
	u32 *p;

	if ((kvm_tmp_index + len) > 1024 * 1024) {
		printm("KVM: No more space (%d + %d)\n", kvm_tmp_index, len);
		return NULL;
	}

	p = (void*)&kvm_tmp[kvm_tmp_index];
	kvm_tmp_index += len;

	return p;
}

extern u32 kvm_emulate_mtsrin_branch_offs;
extern u32 kvm_emulate_mtsrin_reg1_offs;
extern u32 kvm_emulate_mtsrin_reg2_offs;
extern u32 kvm_emulate_mtsrin_orig_ins_offs;
extern u32 kvm_emulate_mtsrin_len;
extern u32 kvm_emulate_mtsrin[];

static void kvm_patch_ins_mtsrin(u32 *inst, u32 rt, u32 rb)
{
	u32 *p;
	int distance_start;
	int distance_end;
	ulong next_inst;

	p = kvm_alloc(kvm_emulate_mtsrin_len * 4);
	if (!p)
		return;

	/* Find out where we are and put everything there */
	distance_start = (ulong)p - (ulong)inst;
	next_inst = ((ulong)inst + 4);
	distance_end = next_inst - (ulong)&p[kvm_emulate_mtsrin_branch_offs];

	/* Make sure we only write valid b instructions */
	if (distance_start > KVM_INST_B_MAX) {
		kvm_patching_worked = 0;
		return;
	}

	/* Modify the chunk to fit the invocation */
	memcpy(p, kvm_emulate_mtsrin, kvm_emulate_mtsrin_len * 4);
	p[kvm_emulate_mtsrin_branch_offs] |= distance_end & KVM_INST_B_MASK;
	p[kvm_emulate_mtsrin_reg1_offs] |= (rb << 10);
	p[kvm_emulate_mtsrin_reg2_offs] |= rt;
	p[kvm_emulate_mtsrin_orig_ins_offs] = *inst;

	/* Patch the invocation */
	kvm_patch_ins_b(inst, distance_start);
}

extern u32 kvm_emulate_mtmsr_branch_offs;
extern u32 kvm_emulate_mtmsr_reg1_offs;
extern u32 kvm_emulate_mtmsr_reg2_offs;
extern u32 kvm_emulate_mtmsr_orig_ins_offs;
extern u32 kvm_emulate_mtmsr_len;
extern u32 kvm_emulate_mtmsr[];

static void kvm_patch_ins_mtmsr(u32 *inst, u32 rt)
{
	u32 *p;
	int distance_start;
	int distance_end;
	ulong next_inst;

	p = kvm_alloc(kvm_emulate_mtmsr_len * 4);
	if (!p)
		return;

	/* Find out where we are and put everything there */
	distance_start = (ulong)p - (ulong)inst;
	next_inst = ((ulong)inst + 4);
	distance_end = next_inst - (ulong)&p[kvm_emulate_mtmsr_branch_offs];

	/* Make sure we only write valid b instructions */
	if (distance_start > KVM_INST_B_MAX) {
		kvm_patching_worked = 0;
		return;
	}

	/* Modify the chunk to fit the invocation */
	memcpy(p, kvm_emulate_mtmsr, kvm_emulate_mtmsr_len * 4);
	p[kvm_emulate_mtmsr_branch_offs] |= distance_end & KVM_INST_B_MASK;

	/* Make clobbered registers work too */
	switch (get_rt(rt)) {
	case 30:
		kvm_patch_ins_lwz(&p[kvm_emulate_mtmsr_reg1_offs],
				 magic_var(scratch2), KVM_RT_30);
		kvm_patch_ins_lwz(&p[kvm_emulate_mtmsr_reg2_offs],
				 magic_var(scratch2), KVM_RT_30);
		break;
	case 31:
		kvm_patch_ins_lwz(&p[kvm_emulate_mtmsr_reg1_offs],
				 magic_var(scratch1), KVM_RT_30);
		kvm_patch_ins_lwz(&p[kvm_emulate_mtmsr_reg2_offs],
				 magic_var(scratch1), KVM_RT_30);
		break;
	default:
		p[kvm_emulate_mtmsr_reg1_offs] |= rt;
		p[kvm_emulate_mtmsr_reg2_offs] |= rt;
		break;
	}

	p[kvm_emulate_mtmsr_orig_ins_offs] = *inst;

	/* Patch the invocation */
	kvm_patch_ins_b(inst, distance_start);
}

static unsigned long kvm_hypercall(unsigned long *in,
				   unsigned long *out,
				   unsigned long nr)
{
	unsigned long register r0 asm("r0");
	unsigned long register r3 asm("r3") = in[0];
	unsigned long register r4 asm("r4") = in[1];
	unsigned long register r5 asm("r5") = in[2];
	unsigned long register r6 asm("r6") = in[3];
	unsigned long register r7 asm("r7") = in[4];
	unsigned long register r8 asm("r8") = in[5];
	unsigned long register r9 asm("r9") = in[6];
	unsigned long register r10 asm("r10") = in[7];
	unsigned long register r11 asm("r11") = nr;
	unsigned long register r12 asm("r12");

	asm volatile("lis	0, 0x4b56\n"
		     "ori	0, 0, 0x4d21\n"
		     "sc"
		     : "=r"(r0), "=r"(r3), "=r"(r4), "=r"(r5), "=r"(r6),
		       "=r"(r7), "=r"(r8), "=r"(r9), "=r"(r10), "=r"(r11),
		       "=r"(r12)
		     : "r"(r3), "r"(r4), "r"(r5), "r"(r6), "r"(r7), "r"(r8),
		       "r"(r9), "r"(r10), "r"(r11)
		     : "memory", "cc", "xer", "ctr", "lr");

	out[0] = r4;
	out[1] = r5;
	out[2] = r6;
	out[3] = r7;
	out[4] = r8;
	out[5] = r9;
	out[6] = r10;
	out[7] = r11;

	return r3;
}

static void kvm_check_ins(u32 *inst)
{
	u32 _inst = *inst;
	u32 inst_no_rt = _inst & ~KVM_MASK_RT;
	u32 inst_rt = _inst & KVM_MASK_RT;

	switch (inst_no_rt) {
	/* Loads */
	case KVM_INST_MFMSR:
		kvm_patch_ins_ld(inst, magic_var(msr), inst_rt);
		break;
	case KVM_INST_MFSPR_SPRG0:
		kvm_patch_ins_ld(inst, magic_var(sprg0), inst_rt);
		break;
	case KVM_INST_MFSPR_SPRG1:
		kvm_patch_ins_ld(inst, magic_var(sprg1), inst_rt);
		break;
	case KVM_INST_MFSPR_SPRG2:
		kvm_patch_ins_ld(inst, magic_var(sprg2), inst_rt);
		break;
	case KVM_INST_MFSPR_SPRG3:
		kvm_patch_ins_ld(inst, magic_var(sprg3), inst_rt);
		break;
	case KVM_INST_MFSPR_SRR0:
		kvm_patch_ins_ld(inst, magic_var(srr0), inst_rt);
		break;
	case KVM_INST_MFSPR_SRR1:
		kvm_patch_ins_ld(inst, magic_var(srr1), inst_rt);
		break;
	case KVM_INST_MFSPR_DAR:
		kvm_patch_ins_ld(inst, magic_var(dar), inst_rt);
		break;
	case KVM_INST_MFSPR_DSISR:
		kvm_patch_ins_lwz(inst, magic_var(dsisr), inst_rt);
		break;

	/* Stores */
	case KVM_INST_MTSPR_SPRG0:
		kvm_patch_ins_std(inst, magic_var(sprg0), inst_rt);
		break;
	case KVM_INST_MTSPR_SPRG1:
		kvm_patch_ins_std(inst, magic_var(sprg1), inst_rt);
		break;
	case KVM_INST_MTSPR_SPRG2:
		kvm_patch_ins_std(inst, magic_var(sprg2), inst_rt);
		break;
	case KVM_INST_MTSPR_SPRG3:
		kvm_patch_ins_std(inst, magic_var(sprg3), inst_rt);
		break;
	case KVM_INST_MTSPR_SRR0:
		kvm_patch_ins_std(inst, magic_var(srr0), inst_rt);
		break;
	case KVM_INST_MTSPR_SRR1:
		kvm_patch_ins_std(inst, magic_var(srr1), inst_rt);
		break;
	case KVM_INST_MTSPR_DAR:
		kvm_patch_ins_std(inst, magic_var(dar), inst_rt);
		break;
	case KVM_INST_MTSPR_DSISR:
		kvm_patch_ins_stw(inst, magic_var(dsisr), inst_rt);
		break;

	/* Nops */
	case KVM_INST_TLBSYNC:
	case KVM_INST_DCBA:
		kvm_patch_ins_nop(inst);
		break;

	/* Rewrites */
	case KVM_INST_MTMSR:
	case KVM_INST_MTMSRD_L0:
	case KVM_INST_MTMSRD_L1:
		kvm_patch_ins_mtmsr(inst, inst_rt);
		break;
	}

	switch (inst_no_rt & ~KVM_MASK_RB) {
	case KVM_INST_MTSRIN:
		/* We use r30 and r31 during the hook */
		if ((get_rt(inst_rt) < 30) && (get_rb(inst_rt) < 30))
			kvm_patch_ins_mtsrin(inst, inst_rt, _inst & KVM_MASK_RB);
		break;
	}
}

static void inst_replacements(void)
{
	u32 *p;
	unsigned long hc_in[8], hc_out[8];

	hc_in[0] = KVM_MAGIC_PAGE;
	hc_in[1] = KVM_MAGIC_PAGE;

	if (kvm_hypercall(hc_in, hc_out,
		HC_VENDOR_KVM | KVM_HC_PPC_MAP_MAGIC_PAGE)) {
		printm("Mapping magic page failed\n");
		return;
	}

	kvm_tmp = (void*)AllocateKernelMemory(1024 * 1024);

	asm volatile ("mtsprg 3, %0" : : "r"(kvm_tmp));

	for( p = NULL; (ulong)p < (ulong)kvm_tmp; p++ ) {
		/* Don't overwrite mtmsr patcher */
		if (((ulong)p >= (ulong)kvm_emulate_mtmsr) &&
		    ((ulong)p <= (ulong)&kvm_emulate_mtmsr[kvm_emulate_mtmsr_len]))
			continue;

		kvm_check_ins(p);
	}

	/* self-test */
	{
		char *x;
		struct kvm_vcpu_arch_shared *m = (void*)KVM_MAGIC_PAGE;

		asm volatile("mfsprg %0, 3" : "=r"(x));

		if ((ulong)x != (ulong)kvm_tmp)
			printm("KVM patching broken!\n");

		if (m->sprg3 != (ulong)kvm_tmp)
			printm("KVM magic page broken!\n");
	}

	/* Check for known bad code and patch it if possible */

	/* make _bcopy_physvir_32 use bcopy */
	for( p = NULL; (ulong)p < (ulong)kvm_tmp; p++ ) {
		if ((*p == 0x40820098) && (p[1] == 0x512b03fe)) {
			printm("KVM patched: _bcopy_physvir_32\n");
			*p = 0x48000098;
		}
	}

	printm("KVM advanced inst patch: %s\n",
		kvm_patching_worked ? "success" : "failure");
}

typedef struct {
	char	*key;
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

static int kvm_available(void)
{
	unsigned long r, in[8], out[8];
	u32 old_c00_inst;
	u32 *c00 = (u32 *)(void*)0xc00;

	/* Make sure that sc immediately returns */
	old_c00_inst = *c00;
	*c00 = INST_RFI;
	flush_inst(0xc00);

	/* Call into KVM and check if we're safe to use PV */
	in[0] = -1;
	in[1] = 0;
	r = kvm_hypercall(in, out, HC_VENDOR_KVM | KVM_HC_FEATURES);
	if (r != HC_EV_SUCCESS)
		return 0;

	if (!(out[0] & (1 << KVM_FEATURE_MAGIC_PAGE)))
		return 0;

	/* Get back the original instruction */
	*c00 = old_c00_inst;
	flush_inst(0xc00);

	return 1;
}

int setup_kvm_acceleration(void)
{
	if (!kvm_available())
		return -1;

	splitmode_fixes();

	if( BootHResPresent("no_inst_replacements") ) {
		printm("Warning instruction replacement DISABLED!\n");
	} else {
		inst_replacements();
	}

	return 0;
}

