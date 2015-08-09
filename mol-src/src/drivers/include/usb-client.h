/* 
 *   Creation Date: <2003/06/25 08:44:40 samuel>
 *   Time-stamp: <2003/07/15 14:43:08 samuel>
 *   
 *	<usb-client.h>
 *	
 *	Client USB interface
 *   
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_USB_CLIENT
#define _H_USB_CLIENT

enum { kBulkType=0, kCtrlType, kInterruptType };

/* status/error messages (ohci) */
#define	CC_NOERROR			0
#define CC_CRC				1
#define CC_BIT_STUFFING			2
#define CC_DATA_TOGGLE_MISMATCH		3
#define CC_STALL			4
#define CC_DEV_NOT_RESPONDING		5
#define CC_PID_CHECK_FAILURE		6
#define CC_UNEXPECTED_PID		7
#define CC_DATA_OVERRUN			8
#define CC_DATA_UNDERRUN		9
#define CC_BUFFER_OVERRUN		12
#define CC_BUFFER_UNDERRUN		13

typedef struct usb_device usb_device_t;

typedef struct {
	int		en;			/* endpoint# */

	int		type;
	int		is_read;		/* direction */
	
	int		size;			/* size (including 8-byte setupheader) */
	int		actcount;
	unsigned char	buf[0];
} urb_t;

typedef struct {
	void		(*submit)( urb_t *urb, void *refcon );
	void		(*abort)( urb_t *urb, void *refcon );

	void		(*reset)( void *refcon );
} usb_ops_t;

extern usb_device_t 	*register_usb_device( usb_ops_t *ops, void *refcon );
extern void		unregister_usb_device( usb_device_t *dev );
extern void		complete_urb( urb_t *urb, int status );

/* clients */
#ifdef CONFIG_USBDEV
extern void		usbdev_init( void );
extern void		usbdev_cleanup( void );
#else
static inline void	usbdev_init( void ) {};
static inline void	usbdev_cleanup( void ) {};
#endif

#endif   /* _H_USB_CLIENT */
