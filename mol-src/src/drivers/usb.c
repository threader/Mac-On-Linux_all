/* 
 *   Creation Date: <2000/10/24 00:29:26 samuel>
 *   Time-stamp: <2003/08/11 20:03:28 samuel>
 *   
 *	<usb.c>
 *	
 *	USB host controller (OHCI compatible)
 *   
 *   Copyright (C) 2000, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#include "mol_config.h"
#include <stddef.h>
#include "pci.h"
#include "booter.h"
#include "driver_mgr.h"
#include "promif.h"
#include "debugger.h"
#include "ioports.h"
#include "byteorder.h"
#include "memory.h"
#include "pic.h"
#include "timer.h"
#include "usb-client.h"
#include "ohci_hw.h"
#include "res_manager.h"

/*
 * MacOS does strange things if the device/type matches OPTi/82c861 (the bulk list
 * is added to the control list and only the later is used). Probably due to an errata...
 */

#define	NDP			8		/* #downstream ports */
#define SOF_PERIOD_MS		37		/* frame period (37 ms) */ 
#define USB_DEBUG		0

/* cu->submitted field */
#define SUBMITTED		1		/* submitted */
#define SUBMITTED_DONE		2		/* submitted and completed */
#define SUBMITTED_DISCARD	3		/* discard at completion */

/* list type */
#define CONTROL_LIST		0x100
#define BULK_LIST		0x200
#define INTERRUPT_LIST		0x400
#define INTERRUPT_IND_MASK	0x1f		/* 32 different lists... */

#define RECOVER_CURB(u)		(priv_ctrl_urb_t*)((char*)(u) - offsetof(priv_ctrl_urb_t, ucurb))
#define RECOVER_URB(u)		(priv_urb_t*)((char*)(u) - offsetof(priv_urb_t, uurb))

/* in-flight transfer descriptors */
typedef struct priv_urb {
	struct priv_urb	*next;
	usb_device_t	*dev;			/* associated device (convenience) */

	int		list_type;		/* which list this urb is on */
	int		ed_mphys;		/* mphys of td */
	int		td_mphys;		/* should always be first td of ed */
	ohci_ed_t	ed;			/* B.E. copy */
	ohci_td_t	td;			/* B.E. copy */

	urb_t		uurb;			/* user urb fields */
} priv_urb_t;

/* in-flight control transfers */
typedef struct priv_ctrl_urb {
	struct priv_ctrl_urb *next;
	usb_device_t	*dev;

	int		submitted;		/* user has seen this request */
	int		xcnt;			/* #bytes transfered to/from data urbs */
	int		status;			/* -1 == in progress */
	
	urb_t		ucurb;			/* user urb fields */
} priv_ctrl_urb_t;


/* client USB device */
struct usb_device {
	int		fa;			/* assigned USB function address */
	int		id;
	int		port;			/* downstream port */
	
	usb_ops_t	ops;
	void		*refcon;

	priv_ctrl_urb_t *curbs;			/* in-flight control urbs */
	struct usb_device *next;
};

/* host controller state */
typedef struct {
	int		irq;			/* irq# */
	int		bar;
	int		range_id;
	int		timer_id;		/* SOF periodic timer */

	int		r[ NUM_HC_REGS + NDP ];	/* big endian */
	hcca_t		*hcca;

	int		irq_enable;
	int		irq_status;
	int		irq_line;

	usb_device_t	*devs;
	priv_urb_t	*urbs;			/* submitted urbs */

	int		next_periodic_ind;
	int		opti_workaround;	/* fix for OPTi hc driver */
} ohci_t;

static ohci_t	hc;

#define GET_HCFS()	(hc.r[HC_CONTROL] & HCFS_MASK)
#define SET_HCFS(x)	hc.r[HC_CONTROL] |= (x & HCFS_MASK)
#define IS_OPERATIONAL() ((hc.r[HC_CONTROL] & HCFS_MASK) == HCFS_OPERATIONAL )

static void		process_endpoint( ohci_ed_t *ed, int ed_mphys, int list_type );
static void		poll_lists( void );

#if 0
static char *regnames[ NUM_HC_REGS + NDP ] = {
	"REV", "CONTROL", "CMD_STATUS", "INT_STATUS", "INT_ENABLE", "INT_DISABLE",
	"HCCA", "PERIOD_CUR_ED", "CTRL_HEAD_ED", "CTRL_CUR_ED",
	"BULK_HEAD_ED", "CUR_BULK_ED", "DONE_HEAD", "FM_INTERVAL", 
	"FM_REMAINING", "FM_NUMBER", "PER_START", "LS_THRESHOLD",
	"RH_DESC_A", "RH_DESC_B", "RH_STATUS", "DPORT0", "DPORT1", "DPORT2",
	"DPORT3"
};
#endif

static void
fix_irq( void )
{
	int b = (hc.irq_enable & INT_MIE) && (hc.irq_enable & hc.irq_status);
	if( b == hc.irq_line )
		return;
	hc.irq_line = b;
	if( b ) {
		/* printm("USB IRQ\n"); */
		irq_line_hi( hc.irq );
	} else
		irq_line_low( hc.irq );
}


/************************************************************************/
/*	port handling							*/
/************************************************************************/

static usb_device_t *
dev_from_port( int port )
{
	usb_device_t *d;
	for( d=hc.devs; d && d->port != port; d=d->next )
		;
	return d;
}

static usb_device_t *
dev_from_fa( int fa )
{
	usb_device_t *d;

	for( d=hc.devs; d && d->fa != fa; d=d->next )
		;
	return d;
}

static void
update_port_status( void )
{
	int i, ev, new, old;

	for( i=0; i<NDP; i++ ) {
		new = old = hc.r[HC_RH_PORTBASE + i];
		if( dev_from_port(i) ) {
			new |= PORT_CCS;
		} else {
			/* clear everything except power bit */
			new &= PORT_PPS;
		}
		ev = 0;
		if( (old^new) & PORT_CCS )
			ev |= PORT_CSC;
		if( (old^new) & PORT_PES )
			ev |= PORT_PESC;
		/* generate a resume event due to a connect/disconnec? */
		if( (hc.r[HC_RH_STATUS] & RH_STATUS_DRWE) )
			if( (ev & PORT_CSC) && GET_HCFS() == HCFS_SUSPEND ) {
				hc.irq_status |= INT_RD;
				SET_HCFS( HCFS_RESUME );
			}

		if( ev ) {
			new |= ev;
			hc.irq_status |= INT_RHSC;
			fix_irq();
		}
		hc.r[HC_RH_PORTBASE + i] = new;
	}
}


/************************************************************************/
/*	misc								*/
/************************************************************************/

static void
hc_reset( int soft ) 
{
	int save_ctrl = 0;

	if( soft ) {
		printm("HC soft reset\n");
		save_ctrl = hc.r[HC_CONTROL];
	}
	memset( hc.r, 0, sizeof(hc.r) );
	hc.r[ HC_CONTROL ] = save_ctrl & ~(CTRL_IR | CTRL_RWC /*| CTRL_RWE*/);
	hc.r[ HC_CONTROL ] |= soft ? HCFS_SUSPEND : HCFS_RESET;
	hc.r[ HC_FM_INTERVAL ] = 0x2edf; /* FSLargestDataPacket? */
	hc.r[ HC_LS_THRESHOLD ] = 0x628;
	hc.r[ HC_RH_DESC_A ] = NDP | POTPGT_2MS * 10;
	
	hc.hcca = NULL;

	update_port_status();
}

static void
update_done_head( void )
{
	u32 dh = hc.r[HC_DONE_HEAD] ;
	
	if( !hc.hcca || !dh || (hc.irq_status & INT_WDH) )
		return;

	dh |= (~INT_WDH & hc.irq_status & hc.irq_enable)? 1 : 0;
	hc.hcca->done_head = cpu_to_le32( dh );
	hc.r[HC_DONE_HEAD] = 0;
	hc.irq_status |= INT_WDH;
	fix_irq();
}


/************************************************************************/
/*	endpoint/descriptor accesss					*/
/************************************************************************/

static void
get_ed( ohci_ed_t *dest, int mphys )
{
	ulong *s = (ulong*)transl_mphys(mphys);
	int *d = (int*)dest;
	if( s ) {
		d[0] = ld_le32(s);
		d[1] = ld_le32(s+1);	
		d[2] = ld_le32(s+2);
		d[3] = ld_le32(s+3);
	}
}

static void
put_ed( int mphys, ohci_ed_t *dest )
{
	ulong *d = (ulong*)transl_mphys(mphys);
	u32 *s = (u32 *)dest;
	if( d ) {
		st_le32(d, s[0]);
		st_le32(d+1, s[1]);
		st_le32(d+2, s[2]);
		st_le32(d+3, s[3]);
	}
}

static void
get_td( ohci_td_t *dest, int mphys )
{
	get_ed( (ohci_ed_t*)dest, mphys );
}

static void
put_td( int mphys, ohci_td_t *src ) 
{
	put_ed( mphys, (ohci_ed_t*)src );
}

static int
get_td_size( ohci_td_t *td )
{
	if( !td->cbp )
		return 0;
	if( (td->be & ~0xfff) == (td->cbp & ~0xfff) )		
		return td->be - td->cbp + 1;
	return (0x1001 - (td->cbp & 0xfff)) + (td->be & 0xfff);
}

/* assertion: td countains count bytes */
static void
get_td_data( char *dest, ohci_td_t *td, int count )
{
	int s, res=0;
	char *p;

	if( !td->cbp || !(p=transl_mphys(td->cbp)) )
		return;

	s = 0x1000 - ((int)p & 0xfff);
	if( count > s ) {
		res = count - s;
		count = s;
	}
	memcpy( dest, p, count );
	if( res && (p=transl_mphys(td->be & ~0xfff)) )
		memcpy( dest+count, p, res );
}
/* assertion: td countains count bytes */
static void
put_td_data( ohci_td_t *td, char *src, int count )
{
	int s, res=0;
	char *p;

	//printm("put_td_data: %d\n", count );
	if( !td->cbp || !(p=transl_mphys(td->cbp)) )
		return;

	s = 0x1000 - ((int)p & 0xfff);
	if( count > s ) {
		res = count - s;
		count = s;
	}
	memcpy( p, src, count );
	if( res && (p=transl_mphys(td->be & ~0xfff)) )
		memcpy( p, src+count, res );
}


/************************************************************************/
/*	debug 								*/
/************************************************************************/

#if 0
static int __dbg
dump_td_( int mphys )
{
	ohci_td_t td;
	uint s;
	int i;
	
	get_td( &td, mphys );
	
	printm("  TD <%08x>  ", (int)mphys );
	printm("%s%s%s DI:%d EC:%d CC:%d ", 
	       (td.t & 2)? "(c) " : (td.t & 1)? "DATA1 " : "DATA0 ",
	       (!td.dp) ? "SETUP " : (td.dp == 1) ? "OUT " : "IN ",
	       (!td.r) ? "R " : "",
	       td.di, td.ec, td.cc );

	s = get_td_size( &td );

	printm("Size: %d\n", s );
	if( td.dp != 2 && s ) {
		u8 *p = (u8*)transl_mphys( td.cbp );
		if( !p ) {
			printm("--- NULL ---\n");
			return 0;
		}
		printm("       [ ");
		for( i=0; i<s && i<16; i++ ) {
			printm("%02x ", *p++ );
			if( !((int)p & 0xfff) )
				p = (u8*)transl_mphys( td.be & ~0xfff );
		}
		printm("]\n");
	}
	return td.next;

}

static void __dbg
dump_td( int mphys, int tail_mphys )
{
	while( mphys && mphys != tail_mphys )
		mphys = dump_td_( mphys );
}

static void __dbg
dump_ed( int mphys, int all )
{
	ohci_ed_t ed;
	int *p;
	
	if( !mphys ) {
		printm("---------------------------------\n");
		return;
	}
	get_ed( &ed, mphys );
	
	printm("ED <%08x>: %d/%d", mphys, ed.fa, ed.en );
	if( !(p=(int*)transl_mphys(mphys)) )
		printm(" BAD MPHYS %x\n", mphys );
	else
		printm(" [%08x] \n", *p );
	printm("    %s%s%s%s%s%s MPS:%d \n", 
	       (ed.d==1) ? "OUT " : (ed.d == 2)? "IN " : "",
	       ed.k ? "SKIP " : "",
	       (ed.headp & HEADP_H) ? "HALT " : "",
	       (ed.headp & HEADP_C) ? "C " : "",
	       ed.s ? "LOWSPEED " : "",
	       ed.f ? "ISO " : "",
	       ed.mps );
	dump_td( ed.headp, ed.tailp );
	if( ed.nexted && all )
		dump_ed( ed.nexted, 1 );
}
#endif


/************************************************************************/
/*	handle simple urbs						*/
/************************************************************************/

static void
unlink_urb( priv_urb_t *u )
{
	priv_urb_t **ul;

	for( ul=&hc.urbs ; *ul && *ul != u ; ul=&(**ul).next )
		;
	if( !*ul )
		fatal("urb not found!\n");
	*ul = u->next;
}

/* get the mphys of the list */
static int
get_list_head( int list_type ) 
{
	int mphys;

	if( list_type == BULK_LIST )
		mphys = hc.r[HC_BULK_HEAD_ED];
	else if( list_type == CONTROL_LIST )
		mphys = hc.r[HC_CNTRL_HEAD_ED];
	else if( (list_type & INTERRUPT_LIST) && hc.hcca ) {
		mphys = hc.hcca->int_table[list_type & INTERRUPT_IND_MASK];
		mphys = cpu_to_le32(mphys);
	} else {
		printm("??? bad list (%d)\n", list_type);
		mphys = 0;
	}
	return mphys;
}

/* assertion: first TD of an active ED */
static void
complete_urb_( priv_urb_t *u, int status )
{
	int mphys = get_list_head( u->list_type );
	ohci_ed_t ed, *_ed;
	ohci_td_t td;

	unlink_urb( u );

	union {
		u32 * e;
		ulong * l;
	}nexted;

	/* verify that the urb is still hot (i.e. on the lists) */
	for( _ed=NULL; mphys && mphys != u->ed_mphys; ) {
		if( !(_ed=transl_mphys(mphys)) )
			break;
		nexted.e = &(_ed->nexted);
		mphys = ld_le32(nexted.l);
	}
	if( !mphys || !(_ed=transl_mphys(mphys)) )
		goto bad;
	get_ed( &ed, mphys );

	/* verify that the endpoint is hot... */
	if( ed.k || (ed.headp & HEADP_H) )
		goto bad;

	/*  ...and that the descriptor is the first one */
	if( (ed.headp & ~0xf) != u->td_mphys )
		goto bad;

	/* we must refetch and verify the TD since the ED might have been stopped */
	get_td( &td, u->td_mphys );
	if( td.cbp != u->td.cbp || td.be != u->td.be )
		goto bad;

	/* copy data from urb buffer */
	if( status == CC_NOERROR ) {
		if( u->uurb.is_read && u->uurb.actcount )
			put_td_data( &td, (char *)u->uurb.buf, u->uurb.actcount );

		/* update td fields */
		if( u->uurb.actcount == u->uurb.size ) {
			td.cbp = 0;
		} else {
			int cbp = td.cbp + u->uurb.actcount;
			if( (td.cbp & ~0xfff) != (cbp & ~0xfff) )
				cbp = (cbp & 0xfff) | (td.be & ~0xfff);
			td.cbp = cbp;

			/* underrun allowed? */
			if( !td.r && u->uurb.is_read )
				status = CC_DATA_UNDERRUN;
		}
		td.ec = 0;
	} else {
		if( status == CC_DEV_NOT_RESPONDING || status == CC_CRC
		    || status == CC_BIT_STUFFING || status == CC_PID_CHECK_FAILURE )
			td.ec = 3;
	}

	/* unlink from endpoint (XXX: fix HEADP_C bit) */
	ed.headp = td.next | ((status == CC_NOERROR)? 0 : HEADP_H);
	_ed->headp = cpu_to_le32( ed.headp );

	/* add td to DONE list and update td */
	td.next = hc.r[HC_DONE_HEAD];
	hc.r[HC_DONE_HEAD] = u->td_mphys;
	td.cc = status;
	put_td( u->td_mphys, &td );

	/* finally, write the done head */
	update_done_head();
	if( status == CC_NOERROR )
		process_endpoint( &ed, u->ed_mphys, u->list_type );

	/* printm("COMPLETION %x (%d)\n", (int)u, status ); */
	free( u );
	return;
 bad:
	free( u );
	/* just to be on the safe side */
	poll_lists();
}

static void
discard_urb( priv_urb_t *u ) 
{
	/* printm("discard_urb %x\n", (int)u ); */
	unlink_urb( u );
	free( u );
}


/************************************************************************/
/*	control urb handling						*/
/************************************************************************/

static void
free_ctrl_urb( priv_ctrl_urb_t *cu )
{
	priv_ctrl_urb_t **cup = &cu->dev->curbs;

	for( ; *cup && *cup != cu ; cup=&(**cup).next )
		;
	if( *cup ) {
		*cup = cu->next;

		if( cu->submitted != SUBMITTED ) {
			/* we cant release the cu; it might still be in use! */
			free( cu );
		} else {
			/* tell user we want to abort (done asynchronously) */
			cu->submitted = SUBMITTED_DISCARD;
			if( cu->dev->ops.abort )
				(*cu->dev->ops.abort)( &cu->ucurb, cu->dev->refcon );
		}
	} else {
		printm("free_ctrl_urb: bad urb!\n");
	}
}


/* client has completed transaction */
static void
finish_ctrl_urb( priv_ctrl_urb_t *cu, priv_urb_t *u )
{
	int status = cu->status;
	
	//dump_td_( u->td_mphys );

	if( !(cu->ucurb.buf[0] & 0x80) ) {
		/* write transaction... must be status phase
		 * since all other stages are handled synchronously
		 */
		if( u->uurb.size || !u->uurb.is_read ) {
			printm("unexpected state\n");
			status = CC_STALL;
		}
		free_ctrl_urb( cu );
	} else {
		/* read transaction */
		if( u->uurb.is_read ) {
			/* xfer data? */
			int s = u->uurb.size;
			if( s > cu->ucurb.actcount + 8 - cu->xcnt )
				s = cu->ucurb.actcount + 8 - cu->xcnt;
			memcpy( u->uurb.buf, &cu->ucurb.buf[cu->xcnt], s );
			u->uurb.actcount = s;
			cu->xcnt += s;
			status = CC_NOERROR;
			/* request incomplete; don't free ctrl urb */
		} else {
			/* status packet, make sure it is a null packet */
			if( u->uurb.size )
				status = CC_STALL;
			free_ctrl_urb( cu );
		}
	}
	complete_urb_( u, status );
}

static int
hook_ctrl_urb_write( priv_ctrl_urb_t *cu, priv_urb_t *u )
{
	char *p = (char *)cu->ucurb.buf;
	usb_device_t *dev;
	int fa;
	
	/* intercept SET_ADDRESS */
	if( !cu->ucurb.en && !p[0] && p[1] == 5 ) {
		dev = cu->dev;
		fa = (p[3] << 8) | p[2];
		cu->status = CC_NOERROR;
		
		finish_ctrl_urb( cu, u );
		if( USB_DEBUG )
			printm("Changing device address: %d -> %d\n", dev->fa, fa );
		dev->fa = fa;
		return 1;
	}
	return 0;
}

static void
do_submit_ctrl( priv_urb_t *u, int dir )
{
	usb_device_t *dev = u->dev;
	priv_ctrl_urb_t *cu;
	int abort;
	
	/* state information available? */
	for( cu=dev->curbs ; cu && cu->ucurb.en != u->ed.en ; cu=cu->next )
		;
	/* yes... */
	if( cu && dir == kDirSETUP ) {
		printm("stale config urb\n");
		free_ctrl_urb( cu );
		cu = NULL;
	}
	/* SETUP transaction */
	if( !cu ) {
		char *p = (char *)u->uurb.buf;
		int size, is_read;

		if( dir != kDirSETUP ) {
			printm("out-of-sync ctrl req\n");
			complete_urb_( u, CC_STALL );
			return;
		}
		if( u->uurb.size != 8 ) {
			printm("Bad SETUP size\n");
			complete_urb_( u, CC_STALL );
			return;
		}
		size = ((p[7] << 8) | p[6]) + 8;
		is_read = (p[0] & 0x80) ? 1:0;

		cu = (priv_ctrl_urb_t*)malloc( sizeof(*cu) + size );
		memcpy( cu->ucurb.buf, p, 8 );
		cu->next = dev->curbs;
		dev->curbs = cu;
		cu->dev = dev;
		cu->ucurb.en = u->ed.en;
		cu->ucurb.size = size;
		cu->ucurb.type = kCtrlType;
		cu->ucurb.is_read = is_read;
		cu->ucurb.actcount = 0;
		cu->xcnt = 8;

		/* device-to-host requests are submitted immediately */
		cu->submitted = is_read ? SUBMITTED : 0;
		cu->status = -1;

		/* printm("CONTROL-SUBMIT\n"); */

		/* complete the SETUP urb */
		u->uurb.actcount = 8;
		complete_urb_( u, CC_NOERROR );

		if( is_read )
			(*dev->ops.submit)( &cu->ucurb, dev->refcon );
		return;
	}

	abort = 0;
	if( !(cu->ucurb.buf[0] & 0x80) ) {
		/* handle host-to-device (writes) */
		do {
			/* status packet already seen? */
			if( cu->submitted ) {
				abort = CC_STALL;
				break;
			}
			/* always accept 0-length out packets */
			if( !u->uurb.is_read && !cu->xcnt ) {
				complete_urb_( u, CC_NOERROR );
				break;
			}
			/* handle status packets */
			if( cu->xcnt == cu->ucurb.size ) {
				/* bad status packet? */
				if( !u->uurb.is_read || u->uurb.size ) {
					abort = CC_STALL;
					break;
				} 
				/* valid status packet */
				cu->submitted = SUBMITTED_DONE;
				if( !hook_ctrl_urb_write(cu, u) ) {
					cu->submitted = SUBMITTED;
					(*dev->ops.submit)( &cu->ucurb, dev->refcon );
				}
				break;
			}
			/* data-overflow? */
			if( cu->xcnt + u->uurb.size > cu->ucurb.size ) {
				abort = CC_STALL;
				break;
			}
			/* normal data xfer */
			memcpy( &cu->ucurb.buf[cu->xcnt], u->uurb.buf, u->uurb.size );
			cu->xcnt += u->uurb.size;
			u->uurb.actcount = u->uurb.size;
			complete_urb_( u, CC_NOERROR );
		} while(0);
	} else {
		/* handle device-to-host (reads) */
		if( cu->status >= 0 )
			finish_ctrl_urb( cu, u );
	}

	if( abort ) {
		printm("ctrl-urb abort %d\n", abort );
		complete_urb_( u, abort );
		free_ctrl_urb( cu );
		return;
	}
}


/************************************************************************/
/*	list processing (control, bulk, interrupt)			*/
/************************************************************************/

/* assumes the first td has NOT been issued */
static void
process_endpoint( ohci_ed_t *ed, int ed_mphys, int list_type )
{
	int size, type, dir, td_mphys;
	ohci_td_t td;
	priv_urb_t *u;

	/* might be empty */
	td_mphys = ed->headp & ~0xf;
	if( td_mphys == ed->tailp )
		return;

	/* get td and dir */
	get_td( &td, td_mphys );		
	dir = (!ed->d || ed->d==3) ? td.dp : ed->d;

	/* determine transfer type */
	type = (!ed->en) ? kCtrlType :
		(list_type & INTERRUPT_LIST)? kInterruptType : 
		(list_type == BULK_LIST)? kBulkType : kCtrlType;
	
	/* OPTi driver puts bulk descriptors on the control list */
	if( hc.opti_workaround && list_type == CONTROL_LIST && ed->en )
		type = kBulkType;

	/* submit urb */
	size = get_td_size( &td );
	u = malloc( sizeof(priv_urb_t) + size );

	u->dev = dev_from_fa( ed->fa );
	u->ed_mphys = ed_mphys;
	u->td_mphys = td_mphys;
	u->td = td;
	u->ed = *ed;
	u->list_type = list_type;
	u->uurb.en = ed->en;
	u->uurb.type = type;
	u->uurb.size = size;
	u->uurb.actcount = 0;
	u->uurb.is_read = (dir == kDirIN);

	if( !u->uurb.is_read && size )
		get_td_data( (char *) u->uurb.buf, &td, size );

	u->next = hc.urbs;
	hc.urbs = u;

	if( !u->dev ) {
		printm("no such dev: %d\n", ed->fa );
		complete_urb_( u, CC_DEV_NOT_RESPONDING );
		return;
	}

	/* printm("SUBMIT %d (type %d) %x <ed: %x> dev=%x\n", ed->fa, type, (int)u, ed_mphys, (int)u->dev ); */
	if( type == kCtrlType )
		do_submit_ctrl( u, dir );
	else
		(*u->dev->ops.submit)( &u->uurb, u->dev->refcon );
}

static int
process_list( int ed_mphys, int list_type )
{
	int td_mphys, ret=0;
	priv_urb_t *u;
	ohci_ed_t ed;

	for( ; ed_mphys ; ed_mphys=ed.nexted ) {
		get_ed( &ed, ed_mphys );

		td_mphys = ed.headp & ~0xf;
		if( ed.k || (ed.headp & HEADP_H) || ed.tailp == td_mphys )
			continue;
		/* isochronous ed terminates periodic list */
		if( ed.f )
			break;
		ret = 1;

		/* first TD already submitted? */
		for( u=hc.urbs; u; u=u->next )
			if( u->ed_mphys == ed_mphys && u->td_mphys == td_mphys )
				break;
		if( !u )
			process_endpoint( &ed, ed_mphys, list_type );
	}
	return ret;
}

static void
poll_lists( void ) 
{
	int ctrl, status;

	if( !IS_OPERATIONAL() )
		return;

	ctrl = hc.r[HC_CONTROL];
	status = hc.r[HC_CMD_STATUS];

	/* issue control list */
	if( ctrl & CTRL_CLE ) {
		if( status & CMD_CLF )
			hc.r[HC_CUR_CNTRL_ED] = hc.r[HC_CNTRL_HEAD_ED];
		if( !process_list(hc.r[HC_CNTRL_HEAD_ED], CONTROL_LIST) ) {
			/* list empty */
			hc.r[HC_CUR_CNTRL_ED] = 0;
			hc.r[HC_CMD_STATUS] &= ~CMD_CLF;
		}
	}

	/* issue bulk list */
	if( ctrl & CTRL_BLE ) {
		if( status & CMD_CLF )
			hc.r[HC_CUR_BULK_ED] = hc.r[HC_BULK_HEAD_ED];
		if( !process_list(hc.r[HC_BULK_HEAD_ED], BULK_LIST) ) {
			/* list empty */
			hc.r[HC_CUR_BULK_ED] = 0;
			hc.r[HC_CMD_STATUS] &= ~CMD_BLF;
		}
	}
}


/************************************************************************/
/*	periodic list handling						*/
/************************************************************************/

static void
rescan_periodic_list( void )
{
	int mphys, ind, ctrl = hc.r[ HC_CONTROL ];

	if( !hc.hcca )
		return;
	
	/* issue periodic list(s) */
	if( ctrl & CTRL_PLE ) {
		ind = hc.next_periodic_ind++ & INTERRUPT_IND_MASK;

		mphys = hc.hcca->int_table[ind];
		mphys = cpu_to_le32( mphys );
		process_list( mphys, INTERRUPT_LIST | ind );
	}
}

/* Interrupt urbs don't necessary complete in a timely fashion; in order
 * to prevent accumulation of stale (e.g. relinked) urbs, we check
 * the urb list now and then. (This is perhaps a bit overkill; in practice
 * this doesn't happen very often.)
 */
static void
clean_stale_interrupt_urbs( void )
{
	priv_urb_t *u;
	ohci_ed_t *_ed;
	int mphys;
	
	union {
		u32 * e;
		ulong * l;
	}nexted;

	if( !(hc.r[HC_CONTROL] & CTRL_PLE) )
		return;
	
	for( u=hc.urbs; u; u=u->next ) {
		if( !(u->list_type & INTERRUPT_LIST) )
			continue;

		mphys = get_list_head( u->list_type );
		for( _ed=NULL; mphys && mphys != u->ed_mphys;){
			if( !(_ed=transl_mphys(mphys)) )
				break;
			nexted.e = &(_ed->nexted);
			mphys = ld_le32(nexted.l);
		}

		if( mphys != u->ed_mphys ) {
			static int once=0;
			if( once++ < 5 )
				printm("aborting stale interrupt urb\n");
			/* stale */
			if( u->dev->ops.abort )
				(*u->dev->ops.abort)( &u->uurb, u->dev->refcon );

			/* The abort might modify the urb list. If there are more stale 
			 * urbs, we handle them next time we are invoked. No hurry.
			 */
			break;
		}
	}	
}


/************************************************************************/
/*	user urb completeion 						*/
/************************************************************************/

static void
complete_ctrl_urb( priv_ctrl_urb_t *cu, int status )
{
	priv_urb_t *u;
	cu->status = status;

	if( cu->submitted == SUBMITTED_DISCARD ) {
		/* this urb has been aborted */
		free( cu );
		return;
	}
	cu->submitted = SUBMITTED_DONE;

	/* walk the control list and see if we can complete something */
	if( !IS_OPERATIONAL() ) {
		free_ctrl_urb( cu );
		return;
	}
	/* delay completion if list is disabled */
	if( !(hc.r[HC_CONTROL] & CTRL_CLE) ) {
		/* XXX: we might want to handle this differently */
		printm("ctrlurb discard (list disabled)\n");
		free_ctrl_urb( cu );
		return;
	}
	
	/* find pending urb */
	for( u=hc.urbs; u && u->uurb.en != cu->ucurb.en; u=u->next )
		;
	if( u )
		finish_ctrl_urb( cu, u );
}

static void
complete_data_urb( priv_urb_t *u, int status )
{
	int ctrl = hc.r[HC_CONTROL];
	int discard = 0;

	/* walk the control list and see if we can complete something */
	discard = !IS_OPERATIONAL()
		|| (u->list_type == BULK_LIST && !(ctrl & CTRL_BLE))
		|| ((u->list_type & INTERRUPT_LIST) && !(ctrl & CTRL_PLE));

	if( !discard )
		complete_urb_( u, status );
	else {
		printm("USB discard (list disabled)\n");
		discard_urb( u );		
	}
}

void
complete_urb( urb_t *uurb, int status )
{
	if( uurb->type == kCtrlType )
		complete_ctrl_urb( RECOVER_CURB(uurb), status );
	else
		complete_data_urb( RECOVER_URB(uurb), status );
}


/************************************************************************/
/*	start of frame							*/
/************************************************************************/

/* assertion: HCFS_OPERATIONAL */
static void
sof_timer( int id, void *dummy, int lost ) 
{
	static int cnt;
	int elapsed, old, new;

	hc.irq_status |= INT_SF;
	fix_irq();
	old = hc.r[HC_FM_NUMBER];

	/* use realtime frame increments (avoids startup delay) */ 
	elapsed = (lost + 1) * SOF_PERIOD_MS;
	new = (old + elapsed) & 0xffff;
	
	/* frame number overflow? (bit15 flipped) */
	if( (old^new) & 0x8000 )
		hc.irq_status |= INT_FNO;

	hc.r[HC_FM_NUMBER] = new;
	if( hc.hcca )
		hc.hcca->frame_num = cpu_to_le32( new );

	/* save some cycles and rescan only now and then */
	if( !(cnt++ & 0x7) )
		rescan_periodic_list();
	if( !(cnt & 0x3f) )
		clean_stale_interrupt_urbs();
	
	/* should not be necessary... */
	/* update_done_head(); */
	/* poll_lists( void ); */
}


/************************************************************************/
/*	I/O								*/
/************************************************************************/

static ulong
reg_io_read( ulong addr, int len, void *usr ) 
{
	unsigned int r = (addr - hc.bar) >> 2;
	int ret;
	
	if( r >= NUM_HC_REGS + NDP || len != 4) {
		printm("strange USB read\n");
		return 0;
	}
	ret = hc.r[r];

	switch( r ) {
	case HC_REV:
		ret = 0x11;
		break;
	case HC_INT_ENABLE:
	case HC_INT_DISABLE:
		ret = hc.irq_enable;
		break;
	case HC_INT_STATUS:
		ret = hc.irq_status;
		break;
	case HC_FM_REMAINING:
		/* xxx should perhaps implement */
		break;
	default:
		break;
	}
	//printm("OHCI-read  %-12s: %08x\n", regnames[r], ret );
	return cpu_to_le32( ret );
}

		
static void
port_write( int port, int val ) 
{
	int old, reg = hc.r[port + HC_RH_PORTBASE];
	
	old = reg;
	if( val & PORT_CCS ) {
		/* clear port enable */
		reg &= ~PORT_PES;
	}
	if( reg & PORT_CCS ) {
		/* device is present */
		reg |= val & (PORT_PES | PORT_PSS);
		if( val & PORT_PES ) {
			/* enable port */
		}	
		if( val & PORT_PSS ) {
			/* suspend port */
		}
		if( val & PORT_PRS ) {
			usb_device_t *dev; 
			if( (dev=dev_from_port(port)) ) {
				dev->fa = 0;
				if( dev->ops.reset )
					(*dev->ops.reset)( dev->refcon );
				/* printm("PORT %d reset\n", port ); */
			}
			/* port reset... clear flag when done */
			reg &= ~PORT_PRS;
			reg |= PORT_PRSC;
			/* clear suspend */
			reg &= ~PORT_PSS;
			/* ...and enable port */
			reg |= PORT_PES;
		}
	} else if( val & (PORT_PSS | PORT_PES | PORT_PRS) ) {
		printm("Port not connected!\n");
		/* error, port not connected */
		reg |= PORT_CSC;
	}
	if( (val & PORT_LSDA) ) {
		/* turn off power local */
		/* printm("local power off [%d]\n", port ); */
		if( 1/*local_power*/ ) {
			reg &= ~PORT_PPS;
		}
	} else if( (val & PORT_PPS) ) {
		/* turn on power local */
		/* printm("local power on [%d]\n", port ); */
		if( 1/*local_power*/ )
			reg |= PORT_PPS;
	}
	if( (reg & PORT_PSS) && (val & PORT_POCI) ) {
		/* resume port */
		reg &= ~PORT_PSS;
		reg |= PORT_PSSC;
	}
	reg &= ~(val & (PORT_CSC | PORT_PESC | PORT_PSSC | PORT_OCIC | PORT_PRSC));

	if( (reg & ~old) & (PORT_CSC | PORT_PESC | PORT_PSSC | PORT_OCIC | PORT_PRSC) )
		hc.irq_status |= INT_RHSC;
	hc.r[port + HC_RH_PORTBASE] = reg;
}


static void
reg_io_write( ulong addr, ulong data, int len, void *usr )
{
	unsigned int r = (addr - hc.bar) >> 2;
	int old, val = le32_to_cpu( data );
	
	if( r >= NUM_HC_REGS + NDP || len != 4 ) {
		printm("strange USB write\n");
		return;
	}

	//printm("OHCI-write %-12s: %08x\n", regnames[r], val );

	old = hc.r[r];
	switch( r ) {
	case HC_REV:		/* read-only registers */
	case HC_FM_REMAINING:
	case HC_FM_NUMBER:
	case HC_DONE_HEAD:
	case HC_PERIOD_CUR_ED:
		val = old;
		break;
	case HC_CONTROL:
		hc.r[r] = val;
		/* enable bits for the various lists, functional state */
		if( (val & HCFS_MASK) != (old & HCFS_MASK) ) {
			if( (old & HCFS_MASK) == HCFS_OPERATIONAL )
				pause_ptimer( hc.timer_id );
			
			switch( val & HCFS_MASK ) {
			case HCFS_RESET:
				/* printm("USB HC RESET\n"); */
				break;
			case HCFS_RESUME:
				/* assertion: FS_SUSPEND */
				/* printm("FS_RESUME\n"); */
				break;
			case HCFS_OPERATIONAL:
				/* printm("FS_OPERATIONAL\n"); */
				update_done_head();
				poll_lists();
				break;
			case HCFS_SUSPEND:
				/* assertion: FS_OPERATIONAL (or software reset) */
				/* printm("FS_SUSPEND\n"); */
				break;
			}
			if( IS_OPERATIONAL() )
				set_ptimer( hc.timer_id, SOF_PERIOD_MS * 1000 );
		}
		val = hc.r[r];
		break;
	case HC_CMD_STATUS:
		if( val & CMD_HCR ) {	/* host controller reset */
			hc_reset(1);
			val = hc.r[r];
			break;
		}
		if( val & CMD_CLF ) {
			hc.r[r] |= CMD_CLF;
			/* printm("control list filled\n"); */
		}
		if( val & CMD_BLF ) {
			hc.r[r] |= CMD_BLF;
		 	/* printm("bulk list filled\n"); */
		}
		if( val & (CMD_CLF | CMD_BLF) )
			poll_lists();
		
		/* ownership change request */
		if( val & CMD_OCR ) {
			printm("ownership change request\n");
			/* INT_OC should be tied to zero if the SMI pin is unimplemented */
			/* old |= CMD_OCR; */
			/* hc.irq_status |= INT_OC; */
		}
		val = hc.r[r];
		break;
	case HC_INT_STATUS:
		hc.irq_status &= ~val;
		/* update done head from here rather from the sof-timer */
		if( (val & INT_WDH) )
			update_done_head();
		break;
	case HC_INT_ENABLE:
		hc.irq_enable |= val;
		break;
	case HC_INT_DISABLE:
		hc.irq_enable &= ~val;
		break;
	case HC_HCCA:
		val &= ~0xff;
		hc.hcca = val ? transl_mphys(val) : 0;
		break;
	case HC_CNTRL_HEAD_ED:
	case HC_BULK_HEAD_ED:
		val &= ~0xf;
		break;
	case HC_CUR_CNTRL_ED:
		val &= ~0xf;
		break;
	case HC_CUR_BULK_ED:
		val &= ~0xf;
		break;
	case HC_FM_INTERVAL:
	case HC_PERIODIC_START:
	case HC_LS_THRESHOLD:
	case HC_RH_DESC_B:
		/* no side effects */
		break;
	case HC_RH_DESC_A:
		val &= ~(DESC_A_NDP_MASK | DESC_A_DT);
		val |= old & (DESC_A_NDP_MASK | DESC_A_DT);
		break;
	case HC_RH_STATUS:
		if( val & RH_STATUS_LPS ) {
			/* printm("global power off\n"); */
			/* turn off global power */
		} else if( val & RH_STATUS_LPSC ) {
			/* printm("global power on\n"); */
			/* turn on global power */
		}
		val &= ~(RH_STATUS_LPS | RH_STATUS_LPSC);
		if( val & RH_STATUS_OCIC )
			old &= RH_STATUS_OCIC;
		if( val & RH_STATUS_CRWE )
			old &= RH_STATUS_DRWE;
		val &= ~(RH_STATUS_OCI | RH_STATUS_OCIC | RH_STATUS_CRWE );
		val |= old & (RH_STATUS_OCI | RH_STATUS_OCIC | RH_STATUS_DRWE );
		break;
	default:
		if( r < HC_RH_PORTBASE || r >= HC_RH_PORTBASE + NDP ) {
			printm("Unimplemented register %d\n", r );
			break;
		}
		port_write( r - HC_RH_PORTBASE, val );
		val = hc.r[r];
		break;
	}
	hc.r[r] = val;
	fix_irq();
}

static void
reg_io_print( int isread, ulong addr, ulong data, int len, void *usr )
{
	addr -= hc.bar;
	printm("OHCI %s offs 0x%3x: %08x\n", isread ? "read " : "write",
	       (int)addr, (int)le32_to_cpu(data) );
}

static io_ops_t ohci_ops = {
	.read	= reg_io_read,
	.write	= reg_io_write,
	.print	= reg_io_print,
};


/************************************************************************/
/*	client iface							*/
/************************************************************************/

usb_device_t *
register_usb_device( usb_ops_t *ops, void *refcon )
{
	usb_device_t *d;
	int port;

	for( port=0; port<NDP && dev_from_port(port); port++ )
		;
	if( port >= NDP ) {
		printm("No free USB ports\n");
		return NULL;
	}
	d = malloc( sizeof(*d) );
	memset( d, 0, sizeof(*d) );

	/* we expect a reset before address relocation */
	d->fa = -1;

	d->ops = *ops;
	d->port = port;
	d->refcon = refcon;

	d->next = hc.devs;
	hc.devs = d;

	/* printm("USB device connect (port %d)\n", d->port ); */
	update_port_status();
	return d;
}

void
unregister_usb_device( usb_device_t *d )
{
	usb_device_t **dd;
	priv_ctrl_urb_t *cu;
	priv_urb_t *u;

	/* printm("USB device disconnect (port %d)\n", d->port ); */
	while( (cu=d->curbs) ) {
		d->curbs = cu->next;
		free( cu );
	}
	for( u=hc.urbs; u ; ) {
		if( u->dev != d )
			u=u->next;
		else {
			discard_urb( u );
			u = hc.urbs;
		}
	}
	for( dd=&hc.devs; *dd && *dd != d; dd=&(**dd).next )
		;
	if( !*dd ) {
		printm("device not found (!)\n");
		return;
	}
	*dd = d->next;
	free( d );
	update_port_status();
}


/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static void
map_usb_bar( int regnum, ulong base, int add_mapp, void *usr )
{
	if( hc.range_id )
		remove_io_range( hc.range_id );
	hc.range_id = add_io_range( base, 0x1000, "OHCI", 0, &ohci_ops, NULL );
	hc.bar = base;
}

static int 
usb_init( void )
{
	mol_device_node_t *dn;
	pci_addr_t addr;
	irq_info_t irqinfo;
	int val;
	
	if( !is_newworld_boot() && !is_osx_boot() )
		return 0;
	if( !(dn=prom_find_devices("usb")) )
		return 0;
	if( (addr=add_pci_device_from_dn(dn, NULL)) < 0 || prom_irq_lookup(dn, &irqinfo) != 1 )
		return 0;

	if( !is_newworld_boot() && !get_bool_res("enable_usb") ) {
		prom_delete_node( dn );
		return 0;
	}

	hc.irq = irqinfo.irq[0];
	set_pci_base( addr, 0, ~(0x1000-1), map_usb_bar );
	prom_get_int_property( dn, "vendor-id", &val );
	hc.opti_workaround = ( val == 0x1045 );

	hc.timer_id = new_ptimer( sof_timer, NULL, 1 /* autoresume */ );
	hc_reset(0);
	
	printm("OHCI USB controller registered %s\n", hc.opti_workaround? "[OPTi]" : "");

	usbdev_init();
	return 1;
}

static void
usb_cleanup( void )
{
	usbdev_cleanup();

	free_ptimer( hc.timer_id );
	
	while( hc.devs )
		unregister_usb_device( hc.devs );
}

driver_interface_t usb_driver = {
	"OHCI", usb_init, usb_cleanup
};
