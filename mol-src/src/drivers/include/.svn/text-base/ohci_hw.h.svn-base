/* 
 *   Creation Date: <2003/04/30 00:06:36 samuel>
 *   Time-stamp: <2003/07/07 00:01:02 samuel>
 *   
 *	<ohci_hw.h>
 *	
 *	OHCI hardware definitions
 *   
 *   Copyright (C) 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_OHCI_HW
#define _H_OHCI_HW

#define	HC_REV					0	/* 1.1 */
#define HC_CONTROL				1
#define  CTRL_CBSR_MASK			0x3		/* control/bulk service ration */
#define  CTRL_PLE			0x4		/* periodic list enable */
#define  CTRL_IE			0x8		/* isochronous enable */
#define  CTRL_CLE			0x10		/* control list enable */
#define  CTRL_BLE			0x20		/* bulk list enable */
#define	   HCFS_MASK		0xc0
#define    HCFS_RESET		0x00
#define	   HCFS_RESUME		0x40
#define	   HCFS_OPERATIONAL	0x80
#define	   HCFS_SUSPEND		0xc0
#define  CTRL_IR			0x100		/* interrupt routing */
#define  CTRL_RWC		   	0x200		/* remote wakeup connected */
#define  CTRL_RWE		   	0x400		/* remote wakeup enable */
#define HC_CMD_STATUS				2
#define  CMD_HCR			0x1		/* host controller reset */
#define  CMD_CLF			0x2		/* control list filled */
#define  CMD_BLF			0x4		/* bulk list filled */
#define  CMD_OCR			0x8		/* ownership change request */
#define  SOC_MASK			0x30000		/* scheduling overrun count */
#define  SOC_INC			0x10000
#define HC_INT_STATUS				3
#define HC_INT_ENABLE				4
#define HC_INT_DISABLE				5
#define  INT_SO				0x1		/* scheduling overrun */
#define  INT_WDH			0x2		/* writeback done head */
#define  INT_SF				0x4		/* start of frame */
#define  INT_RD				0x8		/* resume detected */
#define  INT_UE				0x10		/* unrecoverable error */
#define  INT_FNO			0x20		/* frame number overflow */
#define  INT_RHSC			0x40		/* root hub status change */
#define  INT_OC				0x40000000	/* ownership change, generates SMI or tied to 0 */
#define  INT_MIE			0x80000000	/* master interrupt enable/disable */
#define HC_HCCA					6
#define HC_PERIOD_CUR_ED			7
#define HC_CNTRL_HEAD_ED			8
#define HC_CUR_CNTRL_ED				9
#define HC_BULK_HEAD_ED				10
#define HC_CUR_BULK_ED				11
#define HC_DONE_HEAD				12
#define HC_FM_INTERVAL				13
#define HC_FM_REMAINING				14
#define HC_FM_NUMBER				15
#define HC_PERIODIC_START			16
#define HC_LS_THRESHOLD				17
#define HC_RH_DESC_A				18
#define  POTPGT_2MS			0x1000000
#define  DESC_A_NDP_MASK		0xff
#define  DESC_A_DT			0x400
#define HC_RH_DESC_B				19
#define HC_RH_STATUS				20
#define   RH_STATUS_LPS		        0x1			/* local power status */
#define	  RH_STATUS_OCI			0x2			/* over current indicator */
#define   RH_STATUS_DRWE		0x8000			/* device remote wakeup enable */
#define   RH_STATUS_LPSC	        0x10000			/* local power status change */
#define	  RH_STATUS_OCIC		0x20000			/* over current indicator changed */
#define	  RH_STATUS_CRWE		0x80000000
#define HC_RH_PORTBASE 				21
#define   PORT_CCS			0x1			/* current connect status */
#define   PORT_PES			0x2			/* port enable status */
#define   PORT_PSS			0x4			/* port suspend status */
#define	  PORT_POCI			0x8			/* power over current indicator */
#define	  PORT_PRS			0x10			/* port reset status */
#define	  PORT_PPS			0x100			/* port power status */
#define	  PORT_LSDA			0x200			/* low-speed device attached */
#define	  PORT_CSC			0x10000			/* connect status change */
#define	  PORT_PESC			0x20000			/* port enable status change */
#define	  PORT_PSSC			0x40000			/* port suspend status change */
#define	  PORT_OCIC			0x80000			/* over current indicator change */
#define	  PORT_PRSC			0x100000		/* port reset status change */
#define NUM_HC_REGS				21		/* without the HC_RH_PORT_STATUS vector */

typedef struct {
	u32	reserved:5;
	u32	mps:11;			/* maximum packet size */
	u32	f:1;			/* format (F=0 for control/bulk/int) */
	u32	k:1;			/* skip */
	u32	s:1;			/* speed */
	u32	d:2;			/* direction */
	u32	en:4;			/* endpoint number */
	u32	fa:7;			/* function address */
	u32	tailp;
	u32	headp;			/* low order bits contain C,H */
	u32	nexted;
} ohci_ed_t;

#define		ED_WORD0_K	0x4000	/* skip bit */

#define		HEADP_H		0x1	/* halted */
#define		HEADP_C		0x2	/* carry */

typedef struct {
	u32	cc:4;			/* condition code */
	u32	ec:2;			/* error count */
	u32	t:2;			/* data toggle */
	u32	di:3;			/* delay interrupt */
	u32	dp:2;			/* direction/PID */
	u32	r:1;			/* buffer rounding */
	u32	reserved:18;
	u32	cbp;			/* current buffer pointer */
	u32	next;			/* next xfer descriptor */
	u32	be;			/* buffer end */
} ohci_td_t;

typedef struct {			/* little endian */
	u32	int_table[32];
	u32	frame_num;		/* only 16-bits are used! */
	u32	done_head;		/* LSB used as irq-flag */	
	u32	reserved[29];
	u32	undefinied;
} hcca_t;

enum {
	kDirSETUP=0, kDirOUT, kDirIN, kDirReserved 
};

#endif   /* _H_OHCI_HW */
