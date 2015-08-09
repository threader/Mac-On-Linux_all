/*
 *    <misc.c>
 *
 *   MOL glueing code to access the Kernel based Virtual Machine
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
#include <signal.h>

#include "wrapper.h"
#include "timer.h"
#include "memory.h"
#include "molcpu.h"
#include "mac_registers.h"
#include "rvec.h"
#include "drivers.h"
#include "res_manager.h"

#include <linux/kvm.h>
#include <pthread.h>

#include "kvm.h"

extern priv_rvec_entry_t gRVECtable[NUM_RVECS];

int mol_ioctl( int cmd, int p1, int p2, int p3 )
{
    switch(cmd) {
    case MOL_IOCTL_SETUP_FBACCEL:
        return kvm_set_fb_size( p2, p3 ); /* ppr, height */
        break;
    case MOL_IOCTL_GET_DIRTY_FBLINES:
        return kvm_get_dirty_fb_lines( (short*)p1, p2);
        break;
    case MOL_IOCTL_MMU_MAP:
        if (p2)
            return kvm_set_user_memory_region((struct mmu_mapping *)p1);
        else
            return kvm_del_user_memory_region((struct mmu_mapping *)p1);
        break;
    }
    return 0;
}

int mol_ioctl_simple( int cmd )
{
    return 0;
}


void flush_icache_range( char *start, char *stop )
{
}

void molcpu_arch_init( void )
{
}

void molcpu_arch_cleanup( void )
{
}

void wrapper_init( void )
{
}

void wrapper_cleanup( void )
{
}

int open_session( void )
{
    int r;

    mregs = (mac_regs_t*)malloc(sizeof(*mregs));
    memset( mregs, 0, sizeof(mac_regs_t) );

    r = kvm_init();
    if (r) {
        fprintf(stderr, "KVM init failed\n");
        return r;
    }

    return 0;
}

static int __kvm_init(void)
{
    struct kvm_sregs sregs;

    memset(&sregs, 0, sizeof(sregs));

    sregs.pvr = mregs->spr[S_PVR];
    if (kvm_vcpu_ioctl(KVM_SET_SREGS, &sregs)) {
        fprintf(stderr, "KVM sregs failed\n");
        return -1;
    }

    return 0;
}

void close_session( void )
{
}

static void alarm_handler(int signal, siginfo_t *info, void *raw_context)
{
}

static void *thread_alarm(void *opaque)
{
    pthread_t thread = (pthread_t)opaque;

    while(1) {
        usleep(1000);
        pthread_kill(thread, SIGALRM);
    }

    return NULL;
}

void molcpu_mainloop_prep( void )
{
    if (__kvm_init()) {
        fprintf(stderr, "KVM init failed\n");
        exit(1);
    }
}

mol_kmod_info_t *get_mol_kmod_info( void )
{
        static mol_kmod_info_t info;
        static int once=0;

        if( !once ) {
                memset( &info, 0, sizeof(info) );
                //_get_info( &info );
        }
        return &info;
}

static void kvm_do_mmio(void)
{
    unsigned long tmp;

    if (kvm_run->mmio.is_write) {
        switch(kvm_run->mmio.len) {
            case 1:
                tmp = *(u8*)kvm_run->mmio.data;
                break;
            case 2:
                tmp = *(u16*)kvm_run->mmio.data;
                break;
            case 4:
                tmp = *(u32*)kvm_run->mmio.data;
                break;
        }
        do_io_write(NULL, kvm_run->mmio.phys_addr, tmp, kvm_run->mmio.len);
    } else {
        do_io_read(NULL, kvm_run->mmio.phys_addr, kvm_run->mmio.len, &tmp);
        switch(kvm_run->mmio.len) {
            case 1:
                *(u8*)kvm_run->mmio.data = tmp;
                break;
            case 2:
                *(u16*)kvm_run->mmio.data = tmp;
                break;
            case 4:
                *(u32*)kvm_run->mmio.data = tmp;
                break;
        }
    }
}

static void start_alarm(void)
{
    pthread_t alarm_thread;
    struct sigaction sa;

    sa.sa_handler = 0;
    sa.sa_sigaction = &alarm_handler;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGALRM, &sa, NULL);

    pthread_create(&alarm_thread, NULL, thread_alarm, (void*)pthread_self());
}

extern int mainloop_interrupt( int dummy_rvec );

void molcpu_mainloop( void )
{
    static int inited = 0;

    if (!inited) {
        inited = 1;
        kvm_regs_mol2kvm();
        start_alarm();
    }

    while (1) {
        int r;
        int (*osicall)(int);

        if (mregs->interrupt) {
            if(mainloop_interrupt(0)) {
                printf("mainloop_interrupt wants us to exit the main loop\n");
                return;
            }
        }

        mregs->in_virtual_mode = 1;
        r = kvm_vcpu_ioctl(KVM_RUN, 0);
        mregs->in_virtual_mode = 0;

        if (r == -EINTR || r == -EAGAIN) {
            osicall = gRVECtable[RVEC_TIMER].rvec;
            osicall(0);
            continue;
        }

        switch (kvm_run->exit_reason) {
            case KVM_EXIT_MMIO:
                kvm_do_mmio();
                break;
            case KVM_EXIT_OSI:
            {
                __u64 *gprs = kvm_run->osi.gprs;
                int i;

                for (i = 0; i < 32; i++)
                    mregs->gpr[i] = gprs[i];

                osicall = gRVECtable[RVEC_OSI_SYSCALL].rvec;
                osicall(0);

                for (i = 0; i < 32; i++)
                    gprs[i] = mregs->gpr[i];
                break;
            }
            default:
                fprintf(stderr, "KVM: Unknown exit code: %d\n",
                        kvm_run->exit_reason);
                exit(1);
                break;
        }
    }
}

void save_fpu_completely( struct mac_regs *mregs )
{
}

void reload_tophalf_fpu( struct mac_regs *mregs )
{
}

void shield_fpu( struct mac_regs *mregs )
{
}

ulong get_cpu_frequency( void )
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
ulong get_bus_frequency( void )
{
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
