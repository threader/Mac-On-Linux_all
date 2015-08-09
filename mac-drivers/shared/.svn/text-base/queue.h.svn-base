/* 
 *   Creation Date: <2002/11/19 14:57:35 samuel>
 *   Time-stamp: <2002/11/19 15:59:22 samuel>
 *   
 *	<queue.h>
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

#ifndef _H_QUEUE
#define _H_QUEUE

/* Queue locking assumptions:
 *
 * no concurrent access (external locking required):
 *	dequeue()
 *	dequeue_first()
 *	enqueue_tail()
 *
 * always safe:
 *	enqueue()
 *
 * fifo_get(), fifo_put() are always safe.
 */

typedef struct queue_el {
	struct queue_el *next;	
} queue_el_t;

typedef struct {
	queue_el_t *head;
} queue_t;

typedef queue_t		fifo_t;
typedef queue_el_t	fifo_el_t;

#define fifo_get(f)	dequeue_first((f))
#define fifo_put(f,el)	enqueue((f),(el))

extern void *dequeue( queue_t *q );
extern void *dequeue_first( queue_t *q );
extern void enqueue( queue_t *head, queue_el_t *el );
extern void enqueue_tail( queue_t *q, queue_el_t *el );

#endif   /* _H_QUEUE */
