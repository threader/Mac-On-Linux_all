/*
 *   hostirq.c
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
#include "verbose.h"
#include "hostirq.h"
#include "pic.h"
#include "thread.h"
#include "molcpu.h"
#include "driver_mgr.h"

#include <signal.h>
#include <string.h>
#include <errno.h>
#include <sys/poll.h>

#define HOST_IRQ_COUNT			64

/* table that describes the mapping between host irqs and vm irqs, indexed by host irq */
static struct hostirq_mapping_entry {
	/* next used entry. -1 for last */
	int next;
	/* vm irq this host irq is mapped to */
	int vmirq;
} hostirq_mapping[HOST_IRQ_COUNT];

/* first and last used entry */
static int first_mapping, last_mapping;

int
hostirq_map(int hostirq, int vmirq)
{
	/* sanity check */
	if (hostirq < 0 || hostirq >= HOST_IRQ_COUNT)
		return -1;

	/* have the kernel module grab the interrupt */
	if (_grab_irq(hostirq))
		return -1;

	/* record the mapping */
	hostirq_mapping[hostirq].vmirq = vmirq;

	if (hostirq_mapping[hostirq].next == -1) {
		/* previously not used, enable */
		if (last_mapping != -1)
			hostirq_mapping[last_mapping].next = hostirq;

		last_mapping = hostirq;

		if (first_mapping == -1) {
			/* we are the first entry */
			first_mapping = hostirq;
		}
	}

	return 0;
}

void
hostirq_unmap(int hostirq)
{
	/* sanity check */
	if (hostirq < 0 || hostirq >= HOST_IRQ_COUNT)
		return;

	/* tell the kernel module to release the interrupt */
	_release_irq(hostirq);

	if (hostirq == first_mapping) {
		first_mapping = hostirq_mapping[hostirq].next;

		if (hostirq == last_mapping)
			last_mapping = -1;
	} else {
		int i;

		/* we have to walk the list */
		for (i = first_mapping; i != -1; i = hostirq_mapping[i].next) {
			if (hostirq_mapping[i].next == hostirq) {
				/* entry to be removed comes next, remove it */
				hostirq_mapping[i].next = hostirq_mapping[hostirq].next;

				if (hostirq == last_mapping)
					last_mapping = i;

				break;
			}
		}
	}

	hostirq_mapping[hostirq].next = -1;
}

void
hostirq_check_irqs(void)
{
	irq_bitfield_t mask;

	/* fast path */
	if (first_mapping == -1 || mregs->hostirq_active_cnt == 0)
		return;

	/* for now */
	memset(&mask, 0xff, sizeof(mask));

	/* Make the kernel turn on all the mapped host irqs. This will fire the
	 * handler fire the active irqs. We then raise the appropriate irqs in
	 * the virtual machine in mainloop_interrupt().
	 */
	mregs->hostirq_update = 1;
	interrupt_emulation();
	if (_get_irqs(&mask)) {
		printm("failed to retrieve interrupt information from kernel\n");
		return;
	}
}

static inline int
hostirq_check_bit(int nr, unsigned long *addr)
{
	ulong mask = 1 << (nr & 0x1f);
	ulong *p = ((ulong*)addr) + (nr >> 5);
	return (*p & mask) != 0;
}

void
hostirq_update_irqs(irq_bitfield_t *irq_state)
{
	int i;

	/* update the pic */
	for (i = first_mapping; i != -1; i = hostirq_mapping[i].next) {
		if (hostirq_check_bit(i, irq_state->irqs)) {
//			printm("update: raising %d\n", i);
			irq_line_hi(hostirq_mapping[i].vmirq);
		} else {
//			printm("update: lowering %d\n", i);
			irq_line_low(hostirq_mapping[i].vmirq);
		}
	}
}

static void
hostirq_sighup(int irq, siginfo_t *sinfo, void *foo)
{
//	printm("HUP: irq %d\n", sinfo->si_code);

	/* This is pretty much useless as currently the kernel always signals
	 * the main thread. Maybe useful in future...
	 */
	interrupt_emulation();
}

int
hostirq_init(void)
{
	struct sigaction sa;
	int i;

	/* zero the irq table */
	first_mapping = last_mapping = -1;
	for (i = 0; i < HOST_IRQ_COUNT; i++)
		hostirq_mapping[i].next = -1;

	/* install the signal handler */
	sa.sa_sigaction = hostirq_sighup;
	sa.sa_flags = SA_SIGINFO;
	sigemptyset(&sa.sa_mask);
	if (sigaction(SIGHUP, &sa, NULL))
		printm("error installing SIGHUP handler: %s\n", strerror(errno));

	return 1;
}

void
hostirq_cleanup(void)
{
	int i;

	/* release all host interrupts */
	for (i = first_mapping; i != -1; i = hostirq_mapping[i].next) {
		_release_irq(i);
		hostirq_mapping[i].next = -1;
	}

	first_mapping = -1;
	last_mapping = -1;
}

driver_interface_t hostirq_driver = {
	"host irq handling", hostirq_init, hostirq_cleanup
};


