/* 
 *   Creation Date: <2002/11/19 14:09:33 samuel>
 *   Time-stamp: <2002/11/23 16:13:08 samuel>
 *   
 *	<queue.c>
 *	
 *	semi-atomic queue operations
 *   
 *   Copyright (C) 2002 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "queue.h"
#include <DriverServices.h>
#include "logger.h"

/* refer to queue.h for locking assumptions */

/* Prevent compiler from optimization away this access */
#define GET_QUEUE_HEAD_VOLATILE(q)	(*(queue_el_t * volatile *) &(q)->head)

void *
dequeue( queue_t *queue )
{
	queue_el_t **qp, *q=queue->head;
	
	if( !q )
		return NULL;

	/* first element must be dequeued atomically */
	if( !q->next ) {
		if( CompareAndSwap( (long)q, 0, (unsigned long*)&queue->head ) )
			return q;
		/* try again */ 
		return dequeue( queue );
	}
	/* external dequeue lock assumed */
	for( qp=&q->next; (**qp).next; qp=&(**qp).next )
		;
	q = *qp;
	*qp = NULL;
	return q;
}

void *
dequeue_first( queue_t *queue )
{
	queue_el_t *el;
	do {
		el = GET_QUEUE_HEAD_VOLATILE( queue );
		if( !el )
			return NULL;
	} while( !CompareAndSwap( (long)el, (long)el->next, (unsigned long*)&queue->head ) );
	return el;
}

void
enqueue_tail( queue_t *queue, queue_el_t *new_el )
{
	queue_el_t *el, **pp;
	
	new_el->next = NULL;

	/* atomic access of first element in queue */
	while( !(el=GET_QUEUE_HEAD_VOLATILE(queue)) ) {
		if( CompareAndSwap( 0, (long)new_el, (unsigned long*)&queue->head ))
			return;
	} 
	for( pp=&el->next; *pp; pp=&(**pp).next )
		;
	*pp = new_el;
}

void
enqueue( queue_t *queue, queue_el_t *new_el )
{
	do {
		new_el->next = GET_QUEUE_HEAD_VOLATILE( queue );
	} while( !CompareAndSwap( (long)new_el->next, (long)new_el, (unsigned long*)&queue->head ) );
}

