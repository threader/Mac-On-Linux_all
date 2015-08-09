/* 
 *   Creation Date: <1998-11-10 08:57:52 samuel>
 *   Time-stamp: <2002/12/07 17:56:16 samuel>
 *   
 *	<pic.h>
 *	
 *	Interrupt controller
 *   
 *   Copyright (C) 1997, 1999, 2000, 2002 Samuel Rydh
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_PIC
#define _H_PIC

typedef struct {
	void	(*irq_hi)( int irq );
	int	(*irq_low)( int irq );
	int	(*register_irq)( int *irq );
} irq_controller_t;

extern irq_controller_t 	g_irq_controller;

#define irq_line_hi( irq )	g_irq_controller.irq_hi( (irq) )
#define irq_line_low( irq )	g_irq_controller.irq_low( (irq) )

static inline int register_irq( int *irq ) {
	if( g_irq_controller.register_irq )
		return g_irq_controller.register_irq( irq );
	return 0;
}


#endif   /* _H_PIC */

