/*
 *   pciproxy.c
 *
 *   copyright (c) 2005 Mattias Nissler <mattias.nissler@gmx.de>
 *
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation; either version 2
 *   of the License, or (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
 *
 */

#include "mol_config.h"
#include "driver_mgr.h"
#include "pci.h"
#include "pcidefs.h"
#include "pic.h"
#include "promif.h"
#include "verbose.h"
#include "mmu_mappings.h"
#include "byteorder.h"
#include "wrapper.h"
#include "memory.h"
#include "res_manager.h"
#include "ioports.h"
#include "hostirq.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/mman.h>
#include <sys/poll.h>

SET_VERBOSE_NAME("PCIPROXY");

#define PPLOG(format, args...)		LOG(format "\n", ##args)

#if 0
#define DPRINT(args...)			PPLOG(args)
#else
#define DPRINT(args...)
#endif

#define MAX_BARS	7	/* 6 bars + rom */

/* pciproxy_device_t describes everything we need to know about a pci device in the host OS that is
 * made visible to the guest.
 */
typedef struct pciproxy_device pciproxy_device_t;

struct pciproxy_device {
	/* identification of the physical device (used for matching) */
	unsigned int did_and_vid, clc_and_rid;

	/* mol device node */
	mol_device_node_t *dev_node;

	/* path of the sysfs directory for our device */
	char sysfs_path[NAME_MAX];

	/* file descriptor of the config space in sysfs */
	int fd_config;

	/* the BARS our device uses in the host OS */
	struct {
		/* mmu_mapping structure used for mmu_map()ing */
		struct mmu_mapping mmum;
		/* set when this is BAR is available in the physical device (as determined by the
			 * kernel) */
		int used:1;
		/* wether the BAR is currently mmu_map()ed */
		int mapped:1;
		/* device flags (i/o or mem, mem flags) */
		unsigned char flags;
		/* linux virtual address */
		unsigned char *lvbase;
		/* i/o range id */
		int rangeid;
	} bars[MAX_BARS];

	/* pci address assigned by the bridge */
	pci_addr_t addr;

	/* our irq line inside MOL, the device may actually use a different irq in the host OS */
	int irq;

	/* the irq line the physical device is using in the host OS */
	int hostirq;

	/* linked list of all devices we manage */
	pciproxy_device_t *next;
};

/* head of the list */
static pciproxy_device_t *first_dev;

/* next pci memory address to be assigned. We start at 0x90000000 and proceed upwards. This must be
 * kept in sync with the "ranges" property of the pci bridge and may not collide with other address
 * space usage.
 */
#define PCIPROXY_ASSIGN_MEMORY_LOW	0x90000000
#define PCIPROXY_ASSIGN_MEMORY_HIGH	0xa0000000

static u64 pciproxy_next_mem_addr = PCIPROXY_ASSIGN_MEMORY_LOW;

/* assigned irqs. We use the range 26-31. I hope noone else uses that... */
#define PCIPROXY_ASSIGN_IRQ_LOW		26
#define PCIPROXY_ASSIGN_IRQ_HIGH	31

static char pciproxy_next_irq  = PCIPROXY_ASSIGN_IRQ_LOW;

/* assigned device numbers */
#define PCIPROXY_ASSIGN_DEVNUM_LOW	17
#define PCIPROXY_ASSIGN_DEVNUM_HIGH	31

static char pciproxy_next_devnum = PCIPROXY_ASSIGN_DEVNUM_LOW;

static unsigned int
pciproxy_do_read_config(int configfd, unsigned int offset, void *buf, unsigned int len)
{
	/* seek ... */
	if (lseek(configfd, offset, SEEK_SET) != offset) {
		PPLOG("seek to offset %d in config space file failed: %s", offset, strerror(errno));
		return 0;
	}

	/* ... and read the value. */
	if (read(configfd, buf, len) != len) {
		PPLOG("reading config space file failed: %s", strerror(errno));
		return 0;
	}

	return 1;
}

static unsigned int
pciproxy_do_write_config(int configfd, unsigned int offset, void *buf, unsigned int len)
{
	/* seek ... */
	if (lseek(configfd, offset, SEEK_SET) != offset) {
		PPLOG("seek to offset %d in config space file failed: %s", offset, strerror(errno));
		return 0;
	}

	/* ... and write the value. */
	if (write(configfd, buf, len) != len) {
		PPLOG("write config space file failed: %s", strerror(errno));
		return 0;
	}

	return 1;
}

/* config space read/write hooks */
static void
pciproxy_config_read(void *usr, int offset, char *val)
{
	pciproxy_device_t *pdev = (pciproxy_device_t *) usr;

	DPRINT("config_read: off %d val %d", offset, *val);

	/* ignore bogus reads */
	if (offset < 0 || pdev == NULL) {
		PPLOG("bogus config space read request");
		return;
	}

	/* see what we have... */
	if ((offset >= PCI_BASE_ADDRESS_0 && offset < PCI_CARDBUS_CIS)
			|| (offset >= PCI_ROM_ADDRESS && offset < PCI_CAPABILITY_LIST)) {
		/* this is handled by the generic pci code in pci.c */
		return;
	} else if (offset == PCI_INTERRUPT_LINE) {
		/* return the interrupt line */
		*val = (unsigned char) (pdev->irq & 0xff);
		return;
	} else {
		/* default: pass it through to the physical device */
		pciproxy_do_read_config(pdev->fd_config, offset, val, 1);
		return;
	}
}

static void
pciproxy_config_write(void *usr, int offset, char *val)
{
	pciproxy_device_t *pdev = (pciproxy_device_t *) usr;

	DPRINT("config_write: off %d val %d", offset, *val);

	/* ignore bogus writes */
	if (offset < 0 || pdev == NULL) {
		PPLOG("bogus config space write request");
		return;
	}

	/* find out what to do... */
	if ((offset >= PCI_BASE_ADDRESS_0 && offset < PCI_CARDBUS_CIS)
			|| (offset >= PCI_ROM_ADDRESS && offset < PCI_CAPABILITY_LIST)) {
		/* this is handled by the generic pci code in pci.c */
		return;
	} else if (offset == PCI_COMMAND) {
		/* command register, lower byte */
		/* we don't allow busmastering. If we did the device driver in the guest OS could
		 * program the device to write and read anywhere in the system memory address space
		 * which we don't control in MOL ;-)
		 */
		if (*val & PCI_COMMAND_MASTER)
			PPLOG("warning: PCI bus mastering not working with MOL\n");

		*val &= ~PCI_COMMAND_MASTER;
	} else if (offset == PCI_INTERRUPT_LINE) {
		/* setup new interrupt */
		/* XXX need to take some action here... (OS X doesn't need it) */
		PPLOG("please fix me: need some irq changing code...\n");
		//pdev->irq = *val;
		return;
	}

	/* if we are still there, pass the value to our device */
	pciproxy_do_write_config(pdev->fd_config, offset, val, 1);

}

/* hook table */
static pci_dev_hooks_t pciproxy_hooks =
{
	pciproxy_config_read,
	pciproxy_config_write,
};

#undef BAR_ACCESS_USERSPACE

#ifdef BAR_ACCESS_USERSPACE
static inline int
pciproxy_find_bar_for_mem_access(pciproxy_device_t *pdev, ulong addr, int len)
{
	int i;

	/* search all bars (excluding ROM) */
	for (i = 0; i < MAX_BARS - 1; i++) {
		if (pdev->bars[i].used && pdev->bars[i].mmum.mbase <= addr
				&& pdev->bars[i].mmum.mbase + pdev->bars[i].mmum.size > addr + len)
			return i;
	}
	
	return -1;
}

static ulong
pciproxy_read_mem(ulong addr, int len, void *usr)
{
	pciproxy_device_t *pdev = (pciproxy_device_t *) usr;
	ulong res = 0;
	char *lvaddr;
	int ind;

	/* find the bar */
	ind = pciproxy_find_bar_for_mem_access(pdev, addr, len);

	if (ind == -1) {
		PPLOG("could not satisfy memory read request");
		return 0;
	}

	lvaddr = (char *)pdev->bars[ind].lvbase + (addr - pdev->bars[ind].mmum.mbase);
	res = read_mem(lvaddr, len);

	DPRINT("read mem @ 0x%lx: 0x%lx", addr, res);

	return res;
}

static void
pciproxy_write_mem(ulong addr, ulong data, int len, void *usr)
{
	pciproxy_device_t *pdev = (pciproxy_device_t *) usr;
	char *lvaddr;
	int ind;

	DPRINT("write mem @ 0x%lx: 0x%lx", addr, data);

	/* find the bar */
	ind = pciproxy_find_bar_for_mem_access(pdev, addr, len);

	if (ind == -1) {
		PPLOG("could not satisfy memory write request");
		return;
	}

	lvaddr = (char *)pdev->bars[ind].lvbase + (addr - pdev->bars[ind].mmum.mbase);
	write_mem(lvaddr, data, len);
}

static io_ops_t pciproxy_ioops =
{
	pciproxy_read_mem,
	pciproxy_write_mem,
	NULL
};
#endif

static void
pciproxy_map_bar(int ind, ulong base, int map_or_unmap, void *usr)
{
	pciproxy_device_t *pdev = (pciproxy_device_t *) usr;

	DPRINT("map_bar: ind %d base 0x%08lx map_or_unmap %d", ind, base, map_or_unmap);

	/* see if ind is used by the device */
	if (ind < 0 || ind > MAX_BARS || !pdev->bars[ind].used) {
		PPLOG("warning: mapping request for invalid BAR %d\n", ind);
		return;
	}

#ifdef BAR_ACCESS_USERSPACE
	if (map_or_unmap) {
		/* delete old mapping if necessary */
		if (pdev->bars[ind].mapped) {
			PPLOG("warning: mapping BAR %d that is already mapped", ind);
			remove_io_range(pdev->bars[ind].rangeid);
		}

		pdev->bars[ind].mmum.mbase = base;
		pdev->bars[ind].rangeid = add_io_range(base, pdev->bars[ind].mmum.size, "pciproxy",
				0, &pciproxy_ioops, pdev);
		pdev->bars[ind].mapped = 1;
	} else {
		if (!pdev->bars[ind].mapped) {
			PPLOG("request to unmap unmapped BAR %d", ind);
			return;
		}

		remove_io_range(pdev->bars[ind].rangeid);
		pdev->bars[ind].mapped = 0;
	}
#else

	if (map_or_unmap) {
		/* delete old mapping if necessary */
		if (pdev->bars[ind].mapped) {
			PPLOG("warning: mapping BAR %d that is already mapped", ind);
			_remove_mmu_mapping(&(pdev->bars[ind].mmum));
			/* XXX check for errors? */
		}

		DPRINT("add_mmu_mapping: mbase %x lvbase %p size %x flags %x\n",
				pdev->bars[ind].mmum.mbase,
				pdev->bars[ind].mmum.lvbase,
				pdev->bars[ind].mmum.size,
				pdev->bars[ind].mmum.flags);

		/* now do the new mapping. have the kernel module map the linux physical memory to
		 * mac physical memory */
		pdev->bars[ind].mmum.mbase = base;
		_add_mmu_mapping(&(pdev->bars[ind].mmum));

		/* XXX check for errors? */
		pdev->bars[ind].mapped = 1;
	} else {
		if (!pdev->bars[ind].mapped) {
			PPLOG("request to unmap unmapped BAR %d", ind);
			return;
		}

		/* unmap */
		_remove_mmu_mapping(&(pdev->bars[ind].mmum));
		/* XXX check for errors? */
		pdev->bars[ind].mapped = 0;
	}
#endif

}

static void
pciproxy_grab_physical_irq(pciproxy_device_t *pdev)
{
	char name[NAME_MAX];
	FILE *irqfile;
	
	/* find the interrupt number of the device in the Linux host */
	snprintf(name, NAME_MAX, "%s/irq", pdev->sysfs_path);
	irqfile = fopen(name, "r");
	if (irqfile == NULL) {
		PPLOG("could not open %s: %s", name, strerror(errno));
		return;
	}
	
	if (fscanf(irqfile, "%u", &(pdev->hostirq)) != 1) {
		fclose(irqfile);
		PPLOG("%s: invalid irq number", name);
		return;
	}

	fclose(irqfile);

	/* map the interrupt */
	hostirq_map(pdev->hostirq, pdev->irq);
}

static void
pciproxy_init_irq(pciproxy_device_t *pdev)
{
	mol_device_node_t *pci_node;
	unsigned char ipin;
	u32 dummy = 0;
	u32 *irqmap, *newirqmap, *p, *irqmapmask;
	u32 default_mask[4] = { 0x0000f800, 0x00000000, 0x00000000, 0x00000000 };
	int len, mlen;
	int parent;

	/* check wether the device needs an interrupt */
	pciproxy_do_read_config(pdev->fd_config, PCI_INTERRUPT_PIN, &ipin, 1);
	if (ipin == 0)
		/* no interrupt needed */
		return;

	/* our device always uses pin 1 */
	dummy = 1;
	prom_add_property(pdev->dev_node, "interrupts", (char *) &dummy, sizeof(u32));

	/* read the property mol-interrupt. it is not a standard property but we allow the user to
	 * set it in order to to specify the interrupt line to use.
	 */
	if (prom_get_int_property(pdev->dev_node, "mol-interrupt", &(pdev->irq))) {
		/* not specified, assign an interrupt */
		if (pciproxy_next_irq > PCIPROXY_ASSIGN_IRQ_HIGH) {
			/* out of interrupts */
			PPLOG("out of assignable interrupts, some proxied devices won't work");
			PPLOG("you can workaround this problem by assigning interrupts by hand");
			return;
		}
		pdev->irq = pciproxy_next_irq++;
	}

	/* tell the pic about the irq */
	register_irq(&pdev->irq);

	/* now we need to install an interrupt mapping in the pci bridge device tree node */
	pci_node = prom_find_dev_by_path("/pci");
	if (pci_node == NULL) {
		/* should never happen */
		PPLOG("pci bridge disappeared in device tree, cannot map interrupt");
		return; 
	}

	irqmap = (u32 *) prom_get_property(pci_node, "interrupt-map", &len);
	if (irqmap == NULL) {
		PPLOG("warning: pci bridge is missing interrupt-map property!");
		len = 0;
	}

	irqmapmask = (u32 *) prom_get_property(pci_node, "interrupt-map-mask", &mlen);
	if (irqmapmask == NULL) {
		PPLOG("warning: pci bridge is missing interrupt-map-mask property!");
		irqmapmask = default_mask;
	} else if (mlen < 4 * sizeof(u32)) {
		PPLOG("warning: invalid interrupt-map-mask property at pci bridge!");
		irqmapmask = default_mask;
	}

	/* XXX: the following code is very PIC-implementation-dependent, because we do not care
	 * about lengths... */
	if (prom_get_int_property(pci_node, "interrupt-parent", &parent)) {
		PPLOG("warning: pci bridge is missing interrupt-parent property!");
		/* what do do? */
		parent = 0;
	}

	newirqmap = (u32 *) alloca(len + 6 * sizeof(u32));
	memcpy(newirqmap, irqmap, len);
	p = (u32 *) ((char *) newirqmap + len);
	/* add our entry */
	p[0] = ((pdev->addr & 0xffff) << 8) & irqmapmask[0];
	p[1] = p[2] = 0;
	/* MOLs interrupt mask doesn't even care about the interrupt pin */
	p[3] = 1 & irqmapmask[3];
	p[4] = parent;
	p[5] = pdev->irq;

	/* write the patched interrupt mapping */
	prom_add_property(pci_node, "interrupt-map", (char *) newirqmap,
			len + sizeof(u32) * 6);

	/* grab the irq of the physical device */
	pciproxy_grab_physical_irq(pdev);
}

static void
pciproxy_assign_memory(pciproxy_device_t *pdev)
{
	ulong mask;
	u32 *aaddrs, *p;
	u32 new_aaddrs[6 * 5], *cna = new_aaddrs;
	u32 reg[8 * 5], *creg = reg;
	u64 aaddr;
	u64 alen;
	int i, len, sixtyfourbits;

	/* see if the user already configured addresses by providing the assigned-adresses
	 * property.
	 */
	aaddrs = (u32 *) prom_get_property(pdev->dev_node, "assigned-addresses", &len);
	/* make sure len is sane */
	len = len - (len % (5 * sizeof(u32)));

	/* write the first reg entry describing the config space */
	creg[0] = (pdev->addr & 0xffff) << 8;
	creg[1] = creg[2] = creg[3] = creg[4] = 0;
	creg += 5;

	for (i = 0; i < MAX_BARS; i++) {

		/* ignore unused resources and those not being memory */
		if (!pdev->bars[i].used || (pdev->bars[i].flags & PCI_BASE_ADDRESS_SPACE_IO))
			continue;

		/* try user specified address */
		alen = aaddr = 0;
		if (aaddrs != NULL) {
			/* find it. every entry occupies 5 words */
			for (p = aaddrs; p - aaddrs < len; p += 5) {
				/* p[0] & 0xff contains the bar offset in config space this address
				 * is meant for.
				 */
				if ((p[0] & 0xff) == (0x10 + i * 4)) {
					/* extract it. we have 3 address cells, containing a 64 bit
					 * address in cell 2 & 3, followed by a 2 size cells,
					 * containing 64 bits length.
					 */
					p = aaddrs + i * (2 + 3);
					aaddr = p[2] | ((u64) p[1] << 32);
					alen = p[4] | ((u64) p[3] << 32);
					break;
				}
			}
		}

		/* check the user specified value. It also has to be in the address ranges specified
		 * in the "ranges" property of the pci device tree node, but I don't check that
		 * here.
		 */
		if (aaddr == 0 || alen == 0) {
			/* assign an address. we use the length the kernel gave us */
			alen = pdev->bars[i].mmum.size;
			if (pciproxy_next_mem_addr + alen > PCIPROXY_ASSIGN_MEMORY_HIGH) {
				/* out of addresses. */
				PPLOG("out of assignable pci memory space, so some proxied devices won't work");
				PPLOG("you can assign addresses by hand in the devnode files to workaround this problem");
				break;
			}

			aaddr = pciproxy_next_mem_addr;
			pciproxy_next_mem_addr += alen;
		}
		
		/* write data to our new assigned-addresses property */
		sixtyfourbits = (((aaddr + alen) & 0xffffffff00000000LL) != 0);
		cna[0] = (0 << 31) /* relocatable */
			| ((pdev->bars[i].flags & PCI_BASE_ADDRESS_MEM_PREFETCH) << 30)
			| ((0x2 | sixtyfourbits) << 24) /* default: 32bit address */
			| ((pdev->addr & 0xffff) << 8) /* device address */
			| (0x10 + i * 4); /* reg#: bar offset in config space */
		cna[1] = aaddr >> 32;
		cna[2] = aaddr & 0xffffffff;
		cna[3] = alen >> 32;
		cna[4] = alen & 0xffffffff;

		/* write data for the reg property */
		creg[0] = cna[0];
		creg[1] = creg[2] = 0;
		creg[3] = cna[3];
		creg[4] = cna[4];

		cna += 5;
		creg += 5;

		/* MOLs pci code will write the addresses to the BARS later */
		
		/* inform the pci code about the mapping function and decode mask */
		if ((pdev->bars[i].flags & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO)
			mask = pdev->bars[i].flags | PCI_BASE_ADDRESS_IO_MASK;
		else
			mask = pdev->bars[i].flags | PCI_BASE_ADDRESS_MEM_MASK;

		if (i < 6)
			set_pci_base(pdev->addr, i, mask, pciproxy_map_bar);
	}

	/* write the new assigned-addresses property */
	prom_add_property(pdev->dev_node, "assigned-addresses", (char *) new_aaddrs,
			(cna - new_aaddrs) * sizeof(u32));

	/* and the reg property */
	prom_add_property(pdev->dev_node, "reg", (char *) reg, (creg - reg) * sizeof(u32));
}

static void
pciproxy_setup_bars(pciproxy_device_t *pdev)
{
	char resfile_name[NAME_MAX], rom_file[NAME_MAX];
	int i, romfd;

	/* init the info in pdev */
	memset(pdev->bars, 0, sizeof(pdev->bars));

	/* read the resource file in the sysfs dir */
	snprintf(resfile_name, NAME_MAX, "%s/resource", pdev->sysfs_path);
	FILE *resfile = fopen(resfile_name, "r");

	if (resfile == NULL) {
		PPLOG("could not open %s: %s", resfile_name, strerror(errno));
		return;
	}

	/* read it line by line */
	for (i = 0; i < MAX_BARS - 1; i++) {
		unsigned long long start, end, flags;

		if (fscanf(resfile, "0x%016Lx 0x%016Lx 0x%016Lx\n", &start, &end, &flags) != 3)
			continue;

		/* see if it is for real */
		if (start == end)
			continue;

		/* ok, record the data */
		pdev->bars[i].mmum.size = (size_t) (end - start + 1);
		pdev->bars[i].mmum.mbase = 0;
		pdev->bars[i].mmum.flags = 0;
		pdev->bars[i].flags = flags;

#ifdef BAR_ACCESS_USERSPACE
		/* grab linux physical memory (map it into linux virtual address space) */
		pdev->bars[i].lvbase = (unsigned char *)(map_phys_mem(NULL, (ulong) start,
				pdev->bars[i].mmum.size, PROT_READ | PROT_WRITE));
		if (pdev->bars[i].lvbase == NULL) {
			PPLOG("could not map device memory at 0x%08Lx for BAR %d", start, i);
			continue;
		}

		DPRINT("mapped physical memory @ %llx for bar %d to %p\n", start, i,
				pdev->bars[i].lvbase);
#else
		pdev->bars[i].mmum.flags = MAPPING_PHYSICAL | MAPPING_MACOS_CONTROLS_CACHE
			| MAPPING_FORCE_WRITABLE;

		/* lvbase is actually the physical address for MAPPING_PHYSICAL */
		pdev->bars[i].mmum.lvbase = (char *) ((unsigned long) start);
#endif
		
		pdev->bars[i].used = 1;
		pdev->bars[i].mapped = 0;

		/* save flags of the physical device */
		if ((flags & PCI_BASE_ADDRESS_SPACE) == PCI_BASE_ADDRESS_SPACE_IO)
			pdev->bars[i].flags = (~PCI_BASE_ADDRESS_IO_MASK) & flags;
		else
			pdev->bars[i].flags = (~PCI_BASE_ADDRESS_MEM_MASK) & flags;
	}

	fclose(resfile);

	/* see if we can find a rom */
	snprintf(rom_file, NAME_MAX, "%s/rom", pdev->sysfs_path);
	romfd = open(rom_file, O_RDONLY);
	if (romfd != -1) {
		/* the kernel says there is a rom */
		use_pci_rom(pdev->addr, rom_file);

		pdev->bars[6].mmum.size = lseek(romfd, 0, SEEK_END);
		pdev->bars[6].mmum.mbase = 0;
		pdev->bars[6].mmum.flags = PCI_BASE_ADDRESS_SPACE_MEMORY;
		pdev->bars[6].used = 1;

		close(romfd);
	} else {
		if (errno != ENOENT)
			PPLOG("cannot access %s: %s", rom_file, strerror(errno));
	}
}

static int
pciproxy_install_device(pciproxy_device_t *pdev)
{
	int vid, did, rid, clc;
	pci_dev_info_t pci_info;

	/* get the values we need */
	prom_get_int_property(pdev->dev_node, "vendor-id", &vid);
	prom_get_int_property(pdev->dev_node, "device-id", &did);
	prom_get_int_property(pdev->dev_node, "revision-id", &rid);
	prom_get_int_property(pdev->dev_node, "classcode", &clc);

	pci_info.vendor_id = vid;
	pci_info.device_id = did;
	pci_info.revision = rid;
	pci_info.class_prog = clc & 0xff;
	pci_info.device_class = clc >> 8;

	/* assign an address */
	if (pciproxy_next_devnum > PCIPROXY_ASSIGN_DEVNUM_HIGH) {
		PPLOG("out of device numbers for the proxied devices");
		return 0;
	}

	/* XXX: check wether bus number is always 0 */
	pdev->addr = PCIADDR_FROM_BUS_DEVFN(get_pci_domain(pdev->dev_node), 0,
			(pciproxy_next_devnum++) << 3);

	/* add it to the bus */
	if (add_pci_device(pdev->addr, &pci_info, pdev) == -1)
		return 0;

	return 1;
}

static void
pciproxy_add_name_and_compatible(pciproxy_device_t *pdev)
{
	int vid, did, rid, clc, svid, sid, len;
	int max = 512;
	char value[max];

	/* get the values we need */
	prom_get_int_property(pdev->dev_node, "vendor-id", &vid);
	prom_get_int_property(pdev->dev_node, "device-id", &did);
	prom_get_int_property(pdev->dev_node, "revision-id", &rid);
	prom_get_int_property(pdev->dev_node, "classcode", &clc);
	prom_get_int_property(pdev->dev_node, "subsystem-vendor-id", &svid);
	prom_get_int_property(pdev->dev_node, "subsystem-id", &sid);

	/* fix the name, if necessary */
	if (*prom_get_property(pdev->dev_node, "name", &len) == 0) {
		len = snprintf(value, 512, "pci%04x,%04x", vid, did);
		prom_add_property(pdev->dev_node, "name", value, len + 1);
	}

	/* create the compatible value */
	if (svid != 0) {
		len = snprintf(value, max, "pci%04x,%04x.%04x.%04x.%02x",
				vid, did, svid, sid, rid) + 1;
		max -= len;
		len += snprintf(value + len, max, "pci%04x,%04x.%04x.%04x",
				vid, did, svid, sid) + 1;
		max -= len;
		len += snprintf(value + len, max, "pci%04x,%04x", svid, sid) + 1;
		max -= len;
	}

	len += snprintf(value + len, max, "pci%04x,%04x.%02x", vid, did, rid) + 1;
	max -= len;
	len += snprintf(value + len, max, "pci%04x,%04x", vid, did) + 1;
	max -= len;
	len += snprintf(value + len, max, "pciclass%06x", clc) + 1;
	max -= len;
	len += snprintf(value + len, max, "pciclass%04x", clc >> 8) + 1;
	
	prom_add_property(pdev->dev_node, "compatible", value, len);
}

static struct pciproxy_pci_property {
	char *name;
	unsigned char offset;
	/* len must be 1, 2, 3 or 4, see below */
	unsigned char len;
} pciproxy_pci_props[] = {
	{ "vendor-id", PCI_VENDOR_ID, 2 },
	{ "device-id", PCI_DEVICE_ID, 2 },
	{ "revision-id", PCI_REVISION_ID, 1 },
	{ "classcode", PCI_CLASS_PROG, 3 },
	{ "subsystem-vendor-id", PCI_SUBSYSTEM_VENDOR_ID, 2 },
	{ "subsystem-id", PCI_SUBSYSTEM_ID, 2 },
	{ "min-grant", PCI_MIN_GNT, 1 },
	{ "max-latency", PCI_MAX_LAT, 1 },
};

static void
pciproxy_add_pci_property(pciproxy_device_t *pdev, struct pciproxy_pci_property *prop)
{
	u8 data[4];
	u32 value;
	int i;

	if (prop->len > 4 || prop->len < 1) {
		/* won't happen */
		PPLOG("unsupported property length");
		return;
	}

	/* if the property already exists (from the imported file), keep it. this allows the user to
	 * override the autogenerated properties
	 */
	if (prom_get_property(pdev->dev_node, prop->name, NULL) != NULL)
		return;

	/* read data from config space, adjust byte order */
	pciproxy_do_read_config(pdev->fd_config, prop->offset, data, prop->len);

	value = 0;
	for (i = 0; i < prop->len; i++)
		/* PCI is always little endian */
		value |= data[i] << (i * 8);

	/* add the property to the device node */
	prom_add_property(pdev->dev_node, prop->name, (char *) &value, sizeof(u32));
}

static int
pciproxy_init_device(mol_device_node_t *dev_node, char *sysfs_path, int configfd)
{
	pciproxy_device_t *pdev;
	int i;
	int dummy;

	/* allocate a device structure */
	pdev = malloc(sizeof(pciproxy_device_t));
	if (pdev == NULL) {
		PPLOG("no memory");
		return 0;
	}
	memset(pdev, 0, sizeof(pciproxy_device_t));
	pdev->hostirq = -1;

	/* save already known members */
	pdev->dev_node = dev_node;
	strncpy(pdev->sysfs_path, sysfs_path, NAME_MAX);
	pdev->fd_config = configfd;

	/* then populate the device node */
	dummy = 3;
	prom_add_property(pdev->dev_node, "#address-cells", (char *) &dummy, sizeof(int));
	dummy = 2;
	prom_add_property(pdev->dev_node, "#size-cells", (char *) &dummy, sizeof(int));
	dummy = 1;
	prom_add_property(pdev->dev_node, "#interrupt-cells", (char *) &dummy, sizeof(int));
	for (i = 0; i < sizeof(pciproxy_pci_props) / sizeof(struct pciproxy_pci_property); i++)
		pciproxy_add_pci_property(pdev, &(pciproxy_pci_props[i]));
	pciproxy_add_name_and_compatible(pdev);

	/* give the device an address and notify the bridge of its existence */
	if (!pciproxy_install_device(pdev))
		goto failure;

	/* read BAR info from the kernel */
	pciproxy_setup_bars(pdev);

	/* assign memory regions (this writes the assigned-addresses and reg properties to the
	 * device tree node) */
	pciproxy_assign_memory(pdev);

	/* set our hooks */
	set_pci_dev_hooks(pdev->addr, &pciproxy_hooks);

	/* init the interrupt */
	pciproxy_init_irq(pdev);

	/* hook the device into the list */
	pdev->next = first_dev;
	first_dev = pdev;

	return 1;

failure:
	/* free memory */
	free(pdev);
	return 0;
}

static void
pciproxy_check_device(char *devspec, char *node_file, mol_device_node_t *pci_node)
{
	mol_device_node_t *dev_node = NULL;
	int domain, busnum, devnum, func;
	int configfd;
	char sysfs_path[NAME_MAX], config[NAME_MAX];

	/* read the devspec */
	if (sscanf(devspec, "%4x:%2x:%2x.%d", &domain, &busnum, &devnum, &func) != 4) {
		PPLOG("invalid device specification %s. should be <domain>:<bus>:<dev>.<func>",
				devspec);
		return;
	}

	/* check wether the device exists in the host OS */
	snprintf(sysfs_path, NAME_MAX, "/sys/devices/pci%04x:%02x/%04x:%02x:%02x.%d",
			domain, busnum, domain, busnum, devnum, func);
	snprintf(config, NAME_MAX, "%s/config", sysfs_path);
	configfd = open(config, O_RDWR);
	if (configfd == -1) {
		PPLOG("could not open %s: %s. check device specification", config, strerror(errno));
		return;
	}

	/* if provided, read in the device node file */
	if (node_file != NULL) {
		dev_node = prom_import_node(pci_node, node_file);
		if (dev_node == NULL)
			PPLOG("could not import device node from %s", node_file);

		DPRINT("read nodefile: %p", dev_node);
	}

	/* make sure the dev_node is not null */
	if (dev_node == NULL) {
		/* we use a dummy name, that is corrected later */
		dev_node = prom_add_node(pci_node, "");
		if (dev_node == NULL) {
			PPLOG("could not create device node for %s", devspec);
			close(configfd);
			return;
		}
	}

	/* that done, initialize our data structures */
	if (!pciproxy_init_device(dev_node, sysfs_path, configfd)) {
		close(configfd);
		prom_delete_node(dev_node);
	}

}

static int
pciproxy_init(void)
{
	int i;
	char *devspec, *node_file;
	mol_device_node_t *pci_node;

	/* intialize list */
	first_dev = NULL;

	/* only get going when the user switched us on by configuration */
	if (get_bool_res("enable_pciproxy") != 1)
		return 1;
	
	/* find the pci device tree node */
	pci_node = prom_find_dev_by_path("/pci");
	if (pci_node == NULL) {
		PPLOG("pci node not found in the device tree");
		return 1;
	}

	/* Read the devices that should be made available from the configuration file. The optional
	 * second argument gives the name of a file containing a template device tree. This can be
	 * used to override properties in the autogenerated device node.
	 */
	for (i = 0; (devspec = get_str_res_ind("pci_proxy_device", i, 0)); i++) {
		node_file = get_filename_res_ind("pci_proxy_device", i, 1);
		pciproxy_check_device(devspec, node_file, pci_node);
	}

	return 1;
}

static void
pciproxy_cleanup(void)
{
	pciproxy_device_t *temp;
	int i;
	unsigned char b;

	/* walk the list and clean everything up */
	while (first_dev != NULL) {
		/* disable the device */
		pciproxy_do_read_config(first_dev->fd_config, PCI_COMMAND, &b, 1);
		b &= ~(PCI_COMMAND_IO | PCI_COMMAND_MEMORY);
		pciproxy_do_write_config(first_dev->fd_config, PCI_COMMAND, &b, 1);

		/* remove the irq mapping */
		if (first_dev->hostirq != -1)
			hostirq_unmap(first_dev->hostirq);

		/* at the moment one can't remove pci devices from the bridge once they are added.
		 * While this makes sense, we have to be careful what is cleaned up here and what
		 * the generic pci cleanup code does.
		 */

		/* unmap all mappings */
		for (i = 0; i < MAX_BARS; i++) {
			if (first_dev->bars[i].used) {
#ifdef BAR_ACCESS_USERSPACE
				if (first_dev->bars[i].mapped)
					remove_io_range(first_dev->bars[i].rangeid);

				unmap_mem((char *) first_dev->bars[i].lvbase,
						first_dev->bars[i].mmum.size);
#else
				if (first_dev->bars[i].mapped)
					_remove_mmu_mapping(&(first_dev->bars[i].mmum));
#endif
			}
		}

		/* close the config fd */
		close(first_dev->fd_config);

		/* goto to next list entry, free memory */
		temp = first_dev;
		first_dev = first_dev->next;
		free(temp);
	}

}

driver_interface_t pciproxy_driver =
{
	"pciproxy", pciproxy_init, pciproxy_cleanup
};

