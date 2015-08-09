/*
 *   hostirq.h
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

#ifndef _H_HOSTIRQ
#define _H_HOSTIRQ

#include "mac_registers.h"

/* hostirq.c */

/* maps host interrupt hostirq to virtual machine interrupt vmirq */
extern int hostirq_map(int hostirq, int vmirq);
/* unmaps host interrupt hostirq */
extern void hostirq_unmap(int hostirq);

/* updates the irq line info */
extern void hostirq_update_irqs(irq_bitfield_t *irq_state);
/* calls the kernel in order to get state of the host irq lines */
extern void hostirq_check_irqs(void);

/* init and cleanup */
extern int hostirq_init(void);
extern void hostirq_cleanup(void);

#endif /* _H_HOSTIRQ */

