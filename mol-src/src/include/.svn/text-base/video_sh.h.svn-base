/* 
 *   Creation Date: <2002/11/28 23:13:55 samuel>
 *   Time-stamp: <2003/03/01 14:28:56 samuel>
 *   
 *	<video_sh.h>
 *	
 *	Video interface
 *   
 *   Copyright (C) 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_VIDEO_SH
#define _H_VIDEO_SH

/* OSI_GET_VMODE_INFO, returned in r4-r9 */
typedef struct osi_get_vmode_info {
	short		num_vmodes;
	short		cur_vmode;		/* 1,2,... */
	short		num_depths;
	short		cur_depth_mode;		/* 0,1,2,... */
	short		w,h;
	int		refresh;		/* Hz/65536 */

	int		depth;
	short		row_bytes;
	short		offset;
} osi_get_vmode_info_t;

typedef struct osi_fb_info {
	unsigned long	mphys;
	int		rb, w, h, depth;
} osi_fb_info_t;

/* OSI_VideoCntrl selectors */
enum {
	kVideoStopVBL		= 0,
	kVideoStartVBL		= 1,
	kVideoRouteIRQ		= 2,
	kVideoUseHWCursor	= 3,
};

/* OSI_SetVideoPower */
enum {						/* from Mac OS's video.h */
	kAVPowerOff		= 0,
	kAVPowerStandby		= 1,		/* hsync off */
	kAVPowerSuspend		= 2,		/* vsync off */
	kAVPowerOn		= 3,
};


#define VIDEO_EVENT_CURS_CHANGE		1	/* hardware/software cursor change */

#endif   /* _H_VIDEO_SH */
