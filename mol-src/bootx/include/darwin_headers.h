/*
 * 2001/01/11, Samuel Rydh <samuel@ibrium.se>
 * 
 * This file contains extracts from various Darwin headers
 * 
 */
/*
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 1.2 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.apple.com/publicsource and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 */

#ifndef _H_DARWIN_HEADERS
#define _H_DARWIN_HEADERS

typedef int		integer_t;
typedef integer_t	cpu_subtype_t;
typedef integer_t	cpu_type_t;

typedef int		vm_prot_t;

#define	CPU_TYPE_POWERPC	((cpu_type_t) 18)

typedef struct ppc_thread_state {
	unsigned int srr0;      /* Instruction address register (PC) */
	unsigned int srr1;      /* Machine state register (supervisor) */
	unsigned int r0;
	unsigned int r1;
	unsigned int r2;
	unsigned int r3;
	unsigned int r4;
	unsigned int r5;
	unsigned int r6;
	unsigned int r7;
	unsigned int r8;
	unsigned int r9;
	unsigned int r10;
	unsigned int r11;
	unsigned int r12;
	unsigned int r13;
	unsigned int r14;
	unsigned int r15;
	unsigned int r16;
	unsigned int r17;
	unsigned int r18;
	unsigned int r19;
	unsigned int r20;
	unsigned int r21;
	unsigned int r22;
	unsigned int r23;
	unsigned int r24;
	unsigned int r25;
	unsigned int r26;
	unsigned int r27;
	unsigned int r28;
	unsigned int r29;
	unsigned int r30;
	unsigned int r31;
	
	unsigned int cr;        /* Condition register */
	unsigned int xer;       /* User's integer exception register */
	unsigned int lr;        /* Link register */
        unsigned int ctr;       /* Count register */
        unsigned int mq;        /* MQ register (601 only) */

        unsigned int vrsave;    /* Vector Save Register */
} ppc_thread_state_t;


#endif   /* _H_DARWIN_HEADERS */
