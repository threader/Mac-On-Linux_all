/* 
 *   Creation Date: <1999/12/22 23:26:44 samuel>
 *   Time-stamp: <2003/01/03 02:42:51 samuel>
 *   
 *	<mouse_sh.h>
 *	
 *	Mouse interface (also included from MacOS)
 *   
 *   Copyright (C) 1999, 2000, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_MOUSE_SH
#define _H_MOUSE_SH

enum { kMouseGetDPI=0, kMouseRouteIRQ };


/* OSI_GET_MOUSE, returned in r4-r8 */
typedef struct osi_mouse {
	int	x,y;
	int	width, height;		/* for absolute events */
	int	event;
} osi_mouse_t;
#define SIZEOF_OSI_MOUSE_T		20

/* event field (also used in the implementation) */
enum {
	/* XXX: mouse up/down values hardcoded in osi_mouse.c */
	kMouseEvent_Down1		= 1,	/* 1 as long the button is down */	
	kMouseEvent_Down2		= 2,
	kMouseEvent_Down3		= 4,
	kMouseEvent_ButMask		= 7,

	kMouseEvent_Up1			= 8,	/* not returned by OSI_GET_MOUSE */
	kMouseEvent_Up2			= 16,
	kMouseEvent_Up3			= 32,

	kMouseEvent_MoveDelta		= 64,
	kMouseEvent_MoveTo		= 128,	/* dx/dy contains total width/hight */
	kMouseEvent_HwSwCursSwitch	= 256	/* hardware/software cursor switch */
};

#endif   /* _H_MOUSE_SH */
