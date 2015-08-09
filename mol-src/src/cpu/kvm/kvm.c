/*
 *    <kvm.c>
 *
 *   Helper code to interface with the Kernel based Virtual Machine.
 *
 *   Copyright (C) 2010 Alexander Graf <alex@csgraf.de>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */

#include "mol_config.h"
#include <sys/mman.h>
#include <sys/time.h>

#include <unistd.h>
#include <dirent.h>
#include <sys/types.h>

#include "wrapper.h"
#include "timer.h"
#include "memory.h"
#include "molcpu.h"
#include "mac_registers.h"

#include <linux/kvm.h>
#include <stdarg.h>
#include "kvm.h"
#include <byteswap.h>

static int kvm_fd;
static int vm_fd;
static int vcpu_fd;
struct kvm_run *kvm_run;

int kvm_init(void)
{
    long mmap_size;
    struct kvm_enable_cap cap;
    int r;

    kvm_fd = open("/dev/kvm", O_RDWR);
    if (kvm_fd < 0) {
        fprintf(stderr, "KVM: Couldn't open /dev/kvm\n");
        return -1;
    }

    vm_fd = kvm_ioctl(KVM_CREATE_VM, 0);
    if (vm_fd < 0) {
        fprintf(stderr, "KVM: Couldn't create VM\n");
        return -1;
    }

    vcpu_fd = kvm_vm_ioctl(KVM_CREATE_VCPU, 0);
    if (vcpu_fd < 0) {
        fprintf(stderr, "kvm_create_vcpu failed\n");
        return -1;
    }

    memset(&cap, 0, sizeof(cap));
    cap.cap = KVM_CAP_PPC_OSI;
    r = kvm_vcpu_ioctl(KVM_ENABLE_CAP, &cap);
    if (r < 0) {
        fprintf(stderr, "kvm_enable_cap failed\n");
        return -1;
    }

    mmap_size = kvm_ioctl(KVM_GET_VCPU_MMAP_SIZE, 0);
    if (mmap_size < 0) {
        fprintf(stderr, "KVM_GET_VCPU_MMAP_SIZE failed\n");
        return -1;
    }

    kvm_run = (struct kvm_run *)mmap(NULL, mmap_size, PROT_READ | PROT_WRITE,
                                     MAP_SHARED, vcpu_fd, 0);
    if (kvm_run == MAP_FAILED) {
        fprintf(stderr, "mmap'ing vcpu state failed\n");
        return -1;
    }

    return 0;
}

int kvm_ioctl(int type, ...)
{
    int ret;
    void *arg;
    va_list ap;

    va_start(ap, type);
    arg = va_arg(ap, void *);
    va_end(ap);

    ret = ioctl(kvm_fd, type, arg);
    if (ret == -1)
        ret = -errno;

    return ret;
}

int kvm_vm_ioctl(int type, ...)
{
    int ret;
    void *arg;
    va_list ap;

    va_start(ap, type);
    arg = va_arg(ap, void *);
    va_end(ap);

    ret = ioctl(vm_fd, type, arg);
    if (ret == -1)
        ret = -errno;

    return ret;
}

int kvm_vcpu_ioctl(int type, ...)
{
    int ret;
    void *arg;
    va_list ap;

    va_start(ap, type);
    arg = va_arg(ap, void *);
    va_end(ap);

    ret = ioctl(vcpu_fd, type, arg);
    if (ret == -1)
        ret = -errno;

    return ret;
}

static int kvm_mem_slot = 0;

int kvm_del_user_memory_region(struct mmu_mapping *m)
{
    printf("XXX KVM unmap %#08lx - %#08lx to %p flags %x\n", m->mbase,
           m->mbase + m->size, m->lvbase, m->flags);

    return 0;
}

int fb_slot = -1;
unsigned long fb_size = 0;
static int fb_len;
static unsigned int *fb_bitmap;

#define TARGET_PAGE_SIZE	4096

int kvm_set_user_memory_region(struct mmu_mapping *m)
{
    struct kvm_userspace_memory_region mem;

    mem.slot = kvm_mem_slot++;
    mem.guest_phys_addr = m->mbase;
    mem.memory_size = m->size;
    mem.userspace_addr = (unsigned long)m->lvbase;
    mem.flags = 0;

    if (m->flags & MAPPING_FB) {
        mem.flags |= KVM_MEM_LOG_DIRTY_PAGES;
        fb_slot = mem.slot;
        fb_size = mem.memory_size;

        fb_len = ((fb_size / TARGET_PAGE_SIZE) + 32 - 1) / 32;
        fb_bitmap = malloc(fb_len);
    }

    printf("KVM mapped %#08lx - %#08lx to %p flags %x\n", m->mbase,
           m->mbase + m->size, m->lvbase, m->flags);

    return kvm_vm_ioctl(KVM_SET_USER_MEMORY_REGION, &mem);
}

int kvm_untrigger_irq(void)
{
    unsigned irq = KVM_INTERRUPT_UNSET;

    kvm_vcpu_ioctl(KVM_INTERRUPT, &irq);

    return 0;
}

int kvm_trigger_irq(void)
{
    unsigned irq = KVM_INTERRUPT_SET;

    kvm_vcpu_ioctl(KVM_INTERRUPT, &irq);

    return 0;
}

static int fb_bytes_per_row;
static int fb_height;

int kvm_set_fb_size(int bytes_per_row, int height)
{
    fb_bytes_per_row = bytes_per_row;
    fb_height = height;

    return 0;
}

int kvm_get_dirty_fb_lines(short *rettable, int table_size_in_bytes)
{
    struct kvm_dirty_log d;
    unsigned int i, j;
    unsigned long page_number, addr, c;
    int known_start = 0;

    /* no fb mapped */
    if (fb_slot == -1)
        return 0;

    rettable[0] = 0; // starting y
    rettable[1] = fb_height - 1; // ending y

    memset(fb_bitmap, 0, fb_len);

    d.dirty_bitmap = fb_bitmap;
    d.slot = fb_slot;

    if (kvm_vm_ioctl(KVM_GET_DIRTY_LOG, &d) == -1) {
        /* failed -> expose all screen as updated */
        return 1;
    }

    rettable[1] = 0;
    for (i = 0; i < fb_len; i++) {
        if (fb_bitmap[i] != 0) {
            c = bswap_32(fb_bitmap[i]);
            do {
                j = ffsl(c) - 1;
                c &= ~(1ul << j);
                page_number = i * 32 + j;
                addr = page_number * TARGET_PAGE_SIZE;

                if (!known_start) {
                    rettable[0] = addr / fb_bytes_per_row;
                    known_start = 1;
                }

                rettable[1] = ((addr + TARGET_PAGE_SIZE) / fb_bytes_per_row);
            } while (c != 0);
        }
    }

    /* not dirty */
    if (rettable[0] == rettable[1])
        return 0;

    /* cap on fb_height */
    if (rettable[1] > (fb_height - 1))
        rettable[1] = (fb_height - 1);

    return 1;
}

void kvm_regs_kvm2mol(void)
{
    struct kvm_regs regs;
    int i, ret;

    ret = kvm_vcpu_ioctl(KVM_GET_REGS, &regs);
    if (ret < 0)
        return;

    mregs->nip = regs.pc;
    mregs->msr = regs.msr;
    mregs->link = regs.lr;
    mregs->cr = regs.cr;
    mregs->ctr = regs.ctr;
    mregs->xer = regs.xer;

    for (i = 0; i < 32; i++)
        mregs->gpr[i] = regs.gpr[i];
}

void kvm_regs_mol2kvm(void)
{
    struct kvm_regs regs;
    int i, ret;

    ret = kvm_vcpu_ioctl(KVM_GET_REGS, &regs);
    if (ret < 0)
        return;

    regs.pc = mregs->nip;
    regs.msr = mregs->msr;
    regs.lr = mregs->link;
    regs.cr = mregs->cr;
    regs.ctr = mregs->ctr;
    regs.xer = mregs->xer;

    for (i = 0; i < 32; i++)
        regs.gpr[i] = mregs->gpr[i];

    ret = kvm_vcpu_ioctl(KVM_SET_REGS, &regs);

    return;
}
