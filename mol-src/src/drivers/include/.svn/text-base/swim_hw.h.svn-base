/* 
 *   Creation Date: <1999/02/22 20:01:03 samuel>
 *   Time-stamp: <1999/03/01 03:16:37 samuel>
 *   
 *	<swim_hw.h>
 *
 *	Copyright (C) 1999 Samuel Rydh
 *
 *	Copyright (C) 1996 Paul Mackerras
 * 
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_SWIM_HW
#define _H_SWIM_HW

#define NUM_SWIM_REGS	16
#define SWIM_REG_SIZE	16		/* register + 15 byte pad */

enum {
	r_data=0,
	r_usecs,		/* counts down at 1MHz */
	r_error,
	r_mode,
	r_select,		/* controls CA0, CA1, CA2 and LSTRB signals */
	r_reg5,
	r_control,		/* writing bits clears them */
	r_status,		/* writing bits sets them in control */
	r_intr,
	r_nseek,	   	/* # tracks to seek */
	r_ctrack,		/* current track number (0xff == not determined?)*/
	r_csect,		/* current sector number */
	r_ssize,		/* sector size code?? */
	r_sector,		/* sector # to read or write */
	r_nsect,		/* # sectors to read or write */
	r_intr_enable
};

/*
#define control_bic     r_control
#define control_bis     r_status
*/

/* Bits in select register */
#define CA_MASK         7
#define LSTRB           8

/* Bits in control register */
#define DO_SEEK         0x80
#define SELECT          0x20
#define WRITE_SECTORS   0x10
#define SCAN_TRACK      0x08
#define SEC_DRIVE_ENABLE  0x04		/* second drive */
#define DRIVE_ENABLE    0x02		/* first drive */
#define INTR_ENABLE     0x01

/* when SCAN_TRACK is set, SEEN_SECTOR bit in intr is set,
   ctrack/csect returns track/sektor */

/* Bits in status register */
#define DATA            0x08
/* 0x20, 0x4 and 0x2 appears to be status flags also */

/* Bits in intr and intr_enable registers */
#define ERROR           0x20
#define DATA_CHANGED    0x10
#define TRANSFER_DONE   0x08
#define SEEN_SECTOR     0x04
#define SEEK_DONE       0x02         
/* bit 1 appears to be set */

/* Select values for swim3_action */
#define SEEK_POSITIVE   0
#define SEEK_NEGATIVE   4
#define STEP            1
#define MOTOR_ON        2
#define MOTOR_OFF       6
#define EJECT           7

/* read values of status */
#define STEP_DIR        0	/* head step direction (0==pos, 1==neg) */
#define STEPPING        1	/* 0 when stepping, 1 when done  */
#define MOTOR_ON        2	/* disk motor running */
#define RELAX           3	
#define READ_DATA_0     4	/* read from lower head */
#define SINGLE_SIDED    6	/* Should be 0 */
#define DRIVE_PRESENT   7	/* 0 if a drive is physically present */
#define DISK_IN         8	/* 0 if a disk is in the drive */
#define WRITE_PROT      9	/* 0 if write protected */
#define TRACK_ZERO      10	/* 0 if head is at track zero */
#define TACHO           11	/* pulsed disk speed indicator*/
#define READ_DATA_1     12	/* read from upper head */
#define SEEK_COMPLETE   14
#define HIGH_DENSITY	15	/* 0 if HD disk */

#endif   /* _H_SWIM_HW */
