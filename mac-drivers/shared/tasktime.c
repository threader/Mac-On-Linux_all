/* 
 *   Creation Date: <2002/12/28 14:37:29 samuel>
 *   Time-stamp: <2002/12/28 22:14:53 samuel>
 *   
 *	<tasktime.c>
 *	
 *	Method for drivers to get task time
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include <Notification.h>
#include "logger.h"
#include "tasktime.h"
#include "queue.h"

static NMUPP		handlerUPP;

static struct taskrec {
	fifo_el_t	fifo_el;

	DNeedTimeFunc	func;
	int		param1;
	int		param2;

	NMRec		nm;
	int		index;
} nmrecs[5];

static fifo_t		nmfifo = { NULL };


static void
NMHandler( NMRec *nm )
{
	struct taskrec *d = &nmrecs[ nm->nmRefCon ];

	(*d->func)( d->param1, d->param2 );
	NMRemove(nm);

	fifo_put( &nmfifo, &d->fifo_el );
}

void
DNeedTime( DNeedTimeFunc func, int param1, int param2 )
{
	struct taskrec *d = fifo_get( &nmfifo );
	NMRec *nm;
	
	if( !d ) {
		lprintf("Out of taskrecs (BAD)!\n");
		return;
	}
	nm = &d->nm;
	
	d->func = func;
	d->param1 = param1;
	d->param2 = param2;

	BlockZero( (char*)nm, sizeof(*nm) );
	
	nm->qType = nmType;
	nm->nmResp = handlerUPP;
	nm->nmRefCon = d->index;

	/* nm->nmIcon = NULL; */
	/* nm->nmSound = NULL; */
	/* nm->nmStr = NULL; */
	NMInstall( nm );
}

void
DNeedTime_Init( void )
{
	int i;

	handlerUPP = NewNMUPP( NMHandler );

	for( i=0; i<sizeof(nmrecs)/sizeof(nmrecs[0]); i++ ) {
		nmrecs[i].index = i;
		fifo_put( &nmfifo, &nmrecs[i].fifo_el );
	}
}

