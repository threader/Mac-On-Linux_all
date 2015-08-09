
#ifndef IRQ_UTILS_H_
#define IRQ_UTILS_H_

#include <DriverServices.h>
#include <NameRegistry.h>
#include "LinuxOSI.h"

typedef struct {
	ISTProperty		istp;
	InterruptSetMember	interruptSetMember;		/* Interrupt set identifier */
	long			aapl_int;			/* 'AAPL,interrupt' property (if nonzero) */

	void			*oldInterruptSetRefcon;		/* Existing refCon */
	InterruptHandler	oldInterruptServiceFunction;	/* -> old isr function */
	InterruptEnabler	oldInterruptEnableFunction;	/* -> interrupt enabler */
	InterruptDisabler	oldInterruptDisableFunction;	/* -> interrupt disabler */

	UInt32			pci_addr;			/* first word of 'reg' property */
} IRQInfo;

extern OSStatus 		InstallIRQ_NT( char *name, char *type, InterruptHandler handler, IRQInfo *info );
extern OSStatus 		InstallIRQ( RegEntryID *reg_entry, InterruptHandler handler, IRQInfo *info );
extern OSStatus 		UninstallIRQ( IRQInfo *info );
extern OSStatus			SetIRQRefCon( IRQInfo *info, void *refCon );

static inline UInt32 GetOSIID( IRQInfo *info ) {
	return info->pci_addr;
}

extern int			NRFindNode( char *name, char *type, RegEntryID *node_id );


#endif
