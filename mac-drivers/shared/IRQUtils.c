/* 
 *   Creation Date: <2002/11/27 21:31:04 samuel>
 *   Time-stamp: <2002/12/07 21:37:52 samuel>
 *   
 *	<IRQUtils.c>
 *	
 *	Generic interrupt stuff
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "IRQUtils.h"
#include "logger.h"
#include "LinuxOSI.h"

#ifndef CLEAR
#define CLEAR(what)	BlockZero((char*)&what, sizeof what)
#endif


/************************************************************************/
/*	Interrupt tree extension					*/
/************************************************************************/

/*
 * Extend the IST tree if necessary. The tree needs to be
 * extended if one of the multi-function device nodes lacks a
 * driver-ist property. At least one of the nodes should already have a 
 * driver-ist node (which will be renamed to 'driver-ist-org'). This 
 * node becomes the interrupt parent of the added nodes.
 */

typedef struct {
	IRQInfo info;
	int 	num_members;

	int	last_count;
	int	last_member;
	int	next_start;
	int	count;
} ISTParentInfo;

static InterruptMemberNumber
TransversalISR( InterruptSetMember ISTmember, void *refCon, UInt32 int_count )
{
	ISTParentInfo *p = (ISTParentInfo*)refCon;

	// lprintf("TransversalISR called %d\n", int_count);

	if( int_count != p->last_count ) {
		/* New interrupt */
		p->last_count = int_count;
		p->last_member = p->next_start;
		p->next_start = (p->next_start + 1) % p->num_members;
		p->count = 1;
		return p->last_member + 1;
	}
	
	/* Probe next member */
	if( p->count++ < p->num_members ) {
		p->last_member = (p->last_member + 1) % p->num_members;
		return p->last_member + 1;
	}
	
	lprintf("...No-one handled this interrupt!...\n");
	return kIsrIsNotComplete;
}

static void
ChildInterruptEnabler( InterruptSetMember ISTmember, void *refCon )
{
	// lprintf("ChildInterruptEnabler called, member=%d, refCon %d\n", ISTmember, (long)refCon );
}

static InterruptSourceState
ChildInterruptDisabler( InterruptSetMember ISTmember, void *refCon )
{
	// lprintf("ChildInterruptDisabler called, member=%d refCon %d\n", ISTmember, (long)refCon );
	/* XXX: The return value is wrong... */
	return 0;
}

static int 
DoExtendISTTree( RegEntryID *root_id, RegEntryID *funcs, int num_funcs, ISTProperty *istp )
{
	ISTParentInfo *p;
	InterruptSetID setID;
	OSStatus status=noErr;
	int i;
	
	/* tree needs to be extended */
	lprintf("Extending interrupt tree...\n");
	p = PoolAllocateResident( sizeof(ISTParentInfo), true /*clear*/ );
	if( !p ) {
		lprintf("Could not allocate resident memory!\n");
		return -1;
	}
	p->num_members = num_funcs;
	
	status = GetInterruptFunctions( (*istp)[kISTChipInterruptSource].setID,
					(*istp)[kISTChipInterruptSource].member,
					&p->info.oldInterruptSetRefcon,
					&p->info.oldInterruptServiceFunction,
					&p->info.oldInterruptEnableFunction,
					&p->info.oldInterruptDisableFunction);
	if( status )
		goto err;
			
	/* disable interrupts */
	if( p->info.oldInterruptDisableFunction )
		p->info.oldInterruptDisableFunction( p->info.interruptSetMember, 
						     p->info.oldInterruptSetRefcon );

	status = CreateInterruptSet( (*istp)[kISTChipInterruptSource].setID,
				     (*istp)[kISTChipInterruptSource].member,
				     num_funcs, &setID, kReturnToParentWhenNotComplete );	
	if( status )
		goto err;

	status = InstallInterruptFunctions( (*istp)[kISTChipInterruptSource].setID, 
					    (*istp)[kISTChipInterruptSource].member, 
					    p, TransversalISR, NULL, NULL );
	if( status )
		goto err;
	
	/* Add driver-ist properties */
	RegistryPropertyDelete( root_id, kISTPropertyName );
	RegistryPropertyCreate( root_id, "driver-ist-parent", istp, sizeof(ISTProperty) );

	for( i=0; i<num_funcs; i++ ) {
		ISTProperty child;
		
		CLEAR( child );
		child[kISTChipInterruptSource].setID = setID;
		child[kISTChipInterruptSource].member = i+1;

		// lprintf("Adding driver-ist property\n");
		RegistryPropertyCreate( &funcs[i], kISTPropertyName, &child, sizeof(child) );
		InstallInterruptFunctions( setID, i+1, NULL, NULL, 
					   ChildInterruptEnabler, ChildInterruptDisabler );
	}

	/* enable interrupts */
	if( p->info.oldInterruptEnableFunction )
		p->info.oldInterruptEnableFunction( p->info.interruptSetMember, 
						    p->info.oldInterruptSetRefcon);
err:
	if( status != noErr ) {
		lprintf("Unexpected error %d\n", status );
		PoolDeallocate( p );
	}
	return status;
}

static OSStatus
ExtendISTTree( RegEntryID *reg_entry ) 
{
	enum { MAX_NUM_FUNCS = 8 };
	RegPropertyValueSize propertySize;
	RegEntryID funcs[MAX_NUM_FUNCS], root_id, re;
	int i, found=0, num_funcs=0, num_roots=0;
	unsigned long reg1, reg2;
	RegEntryIter cookie;
	ISTProperty istp;
	OSStatus status;
	Boolean done;
	
	/* lprintf("ExtendISTTree\n"); */
	
	/* Find out which devfn we are examining */
	propertySize = 4;
	if( (status=RegistryPropertyGet( reg_entry, "reg", &reg1, &propertySize)) ) {
		lprintf("Node is lacking a 'reg' property");
		return -1;
	}

	/* Allocate IDs */
	for( i=0; i<MAX_NUM_FUNCS; i++ )
		RegistryEntryIDInit( &funcs[i] );
	RegistryEntryIDInit( &root_id );		
	
	/* Find all nodes whith this bus and device number */
	RegistryEntryIterateCreate( &cookie );

	for( ;; RegistryEntryIDDispose(&re) ) {
		status = RegistryEntryIterate( &cookie, kRegIterContinue, &re, &done );
		if( done || status != noErr )
			break;
			
		propertySize = 4;
		if( RegistryPropertyGet( &re, "reg", &reg2, &propertySize ) )
			continue;
			
		/* Only the function number may different */
		if( (reg2 & ~0x700) != (reg1 & ~0x700) )
			continue;

		if( num_funcs >= MAX_NUM_FUNCS ) {
			lprintf("Too many cards found!\n");
			continue;
		}		
		RegistryEntryIDCopy( &re, &funcs[num_funcs++] );

		propertySize = sizeof( ISTProperty );
		if( !RegistryPropertyGet( &re, kISTPropertyName, &istp, &propertySize) ) {
			if( !num_roots++ )
				RegistryEntryIDCopy( &re, &root_id );
		}
	}
	RegistryEntryIterateDispose( &cookie );

	status = noErr;
	
	if( !num_roots || !num_funcs ) {
		lprintf("Could not find the interrupt node (num_roots %d, num_funcs %d)!\n", 
			num_roots, num_funcs);
	} else if( num_roots == 1 && num_funcs > 1 ) {
		status = DoExtendISTTree( &root_id, funcs, num_funcs, &istp );
	}

	/* Free IDs */
	for( i=0; i<MAX_NUM_FUNCS; i++ )
		RegistryEntryIDDispose( &funcs[i] );
	RegistryEntryIDDispose( &root_id );

	if( status )
		lprintf("Error %d in ExtendISTTree\n", status );
	return status;
}


/************************************************************************/
/*	IRQ installation						*/
/************************************************************************/

OSStatus
InstallIRQ( RegEntryID *reg_entry, InterruptHandler handler, IRQInfo *info )
{
	RegPropertyValueSize propertySize;
	OSStatus status;

	if( (status=ExtendISTTree(reg_entry)) )
		return status;

	propertySize = sizeof(ISTProperty);
	if( (status=RegistryPropertyGet( reg_entry, kISTPropertyName, &info->istp, &propertySize)) ) {
		lprintf("Could not find the 'driver-ist' node (status %d)!\n", status);
		return status;
	}
		
	propertySize = sizeof(info->aapl_int);
	if( RegistryPropertyGet( reg_entry, "AAPL,interrupts", &info->aapl_int, &propertySize ) )
		info->aapl_int = 0;
	
	info->interruptSetMember.setID = info->istp[kISTChipInterruptSource].setID;
	info->interruptSetMember.member = info->istp[kISTChipInterruptSource].member;

	status = GetInterruptFunctions( info->istp[kISTChipInterruptSource].setID,
					info->istp[kISTChipInterruptSource].member,
					&info->oldInterruptSetRefcon,
					&info->oldInterruptServiceFunction,
					&info->oldInterruptEnableFunction,
					&info->oldInterruptDisableFunction);
	if( status != noErr ) {
		lprintf("InstallIRQ, GetInterruptFunctions failed (%d)\n", status );
		return status;
	}

	if( info->oldInterruptDisableFunction )
		info->oldInterruptDisableFunction( info->istp[kISTChipInterruptSource],
						   info->oldInterruptSetRefcon);

	status = InstallInterruptFunctions( info->istp[kISTChipInterruptSource].setID, 
					    info->istp[kISTChipInterruptSource].member, 
					    NULL, handler, NULL, NULL );
	if( status != noErr ) {
		lprintf("InstallIRQ, InstallInterruptFunctions failed (%d)\n", status );
		return status;
	}

	propertySize = 4;
	if( (status=RegistryPropertyGet( reg_entry, "reg", &info->pci_addr, &propertySize )) ) {
		lprintf("Could not get the 'reg' property for the interrupt node\n");
		return status;
	}

	/* enable interrupts */
	if( info->oldInterruptEnableFunction )
		info->oldInterruptEnableFunction( info->interruptSetMember,
						  info->oldInterruptSetRefcon );
	return status;
}

OSStatus
InstallIRQ_NT( char *name, char *type, InterruptHandler handler, IRQInfo *info )
{
	RegEntryID re;
	OSStatus status;

	RegistryEntryIDInit( &re );
	if( !NRFindNode( name, type, &re ) ) {
		RegistryEntryIDDispose( &re );
		lprintf("Failed to find node %s!\n", name);
		return 1;
	}
	status = InstallIRQ( &re, handler, info );
	RegistryEntryIDDispose( &re );

	if( status ) {
		lprintf("InstallIRQ_NT returned %d\n", status );
	}
	return status;
}

OSStatus
SetIRQRefCon( IRQInfo *info, void *refCon )
{
	OSStatus status;
	status = InstallInterruptFunctions( info->istp[kISTChipInterruptSource].setID,
					    info->istp[kISTChipInterruptSource].member,
					    refCon, NULL, NULL, NULL );
	return status;
}
 
OSStatus
UninstallIRQ( IRQInfo *info )
{
	OSStatus status;

	if( info->oldInterruptDisableFunction )
		info->oldInterruptDisableFunction( info->interruptSetMember,
						   info->oldInterruptSetRefcon);

	status = InstallInterruptFunctions( info->istp[kISTChipInterruptSource].setID,
					    info->istp[kISTChipInterruptSource].member,
					    info->oldInterruptSetRefcon,
					    info->oldInterruptServiceFunction,
					    info->oldInterruptEnableFunction,
					    info->oldInterruptDisableFunction );
	return status;
}


/************************************************************************/
/*	Misc								*/
/************************************************************************/

/* Lookup a node in the name registry tree */  
int
NRFindNode( char *name, char *type, RegEntryID *node_id )
{
	Boolean done;
	RegEntryIter cookie;
	RegEntryID re;
	OSStatus status;
	unsigned long prop_size;
	char buf[32];
	int found = 0;
	
	RegistryEntryIterateCreate( &cookie );

	for( ; !found ; RegistryEntryIDDispose(&re) ) {
		status = RegistryEntryIterate( &cookie, kRegIterContinue, &re, &done );
		if( done || status )
			break;
			
		prop_size = sizeof(buf);	
		if( name ) {
			status = RegistryPropertyGet( &re, "name", buf, &prop_size );
			if( status || CStrNCmp( buf, name, sizeof(buf) ) )
				continue;
		}
		if( type ) {
			status = RegistryPropertyGet( &re, "type", buf, &prop_size );
			if( status || CStrNCmp( buf, name, sizeof(buf) ) )
				continue;
		}
		RegistryEntryIDCopy( &re, node_id );
		found = 1;
	}
	RegistryEntryIterateDispose( &cookie );
	return found;
}
