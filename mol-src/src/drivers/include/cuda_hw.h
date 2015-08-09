/*
 * Definitions for talking to the CUDA.  The CUDA is a microcontroller
 * which controls the ADB, system power, RTC, and various other things.
 *
 * Copyright (C) 1996, Paul Mackerras.
 * Copyright (C) 1999, Samuel Rydh
 */

#ifndef _H_CUDA_HW
#define _H_CUDA_HW

/* First byte sent to or received from CUDA */
#define ADB_PACKET	0
#define CUDA_PACKET	1
#define ERROR_PACKET	2
#define TIMER_PACKET	3
#define POWER_PACKET	4
#define MACIIC_PACKET	5
#define PMU_PACKET	6

/* CUDA commands (2nd byte) */
#define CUDA_WARM_START			0
#define CUDA_AUTOPOLL			1
#define CUDA_GET_6805_ADDR		2
#define CUDA_GET_TIME			3
#define CUDA_GET_PRAM			7
#define CUDA_SET_6805_ADDR		8
#define CUDA_SET_TIME			9
#define CUDA_POWERDOWN			0xa
#define CUDA_POWERUP_TIME		0xb
#define CUDA_SET_PRAM			0xc
#define CUDA_MS_RESET			0xd
#define CUDA_SEND_DFAC			0xe
#define CUDA_BATTERY_SWAP_SENSE    	0x10
#define CUDA_RESET_SYSTEM		0x11
#define CUDA_SET_IPL_LEVEL		0x12
#define CUDA_FILE_SERVER_FLAG		0x13
#define CUDA_SET_AUTO_RATE		0x14
#define CUDA_GET_AUTO_RATE		0x16
#define CUDA_SET_DEVICE_LIST		0x19
#define CUDA_GET_DEVICE_LIST		0x1a
#define CUDA_SET_ONE_SECOND_MODE   	0x1b
#define CUDA_CMD_1C			0x1c
#define CUDA_SET_POWER_MESSAGES		0x21
#define CUDA_GET_SET_IIC		0x22
#define CUDA_ENABLE_DISABLE_WAKEUP 	0x23
#define CUDA_TIMER_TICKLE      		0x24
#define CUDA_COMBINED_FORMAT_IIC   	0x25
#define CUDA_CMD_26			0x26
#define CUDA_POWERFAIL_TIME		0x27

#endif   /* _H_CUDA_HW */
