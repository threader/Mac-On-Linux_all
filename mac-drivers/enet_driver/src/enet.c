/* 
 *   Creation Date: <2002/11/23 17:53:53 samuel>
 *   Time-stamp: <2002/12/07 17:10:27 samuel>
 *   
 *	<enet.c>
 *	
 *	MOL ethernet driver, based upon 
 *
 *	   Simple loopback driver utilizing Mentat DLPI Template Code (dlpiether.c)
 *	   This file, combined with dlpiether.c (or linked against the Shared LIbrary
 *	   containing dlpiether.c) is a functioning loopback driver for Open Transport.
 *	   This file also serves as a template for writing the hardware-specific
 *	   code for a driver which will use dlpiether.c.  Comments in the code and
 *	   the accompanying documentation, Mentat DLPI Driver Template, provide
 *	   necessary information.
 *	   loopback.c 1.2 Last Changed 29 Mar 1996
 *
 *	   Copyright: (C) 1996 by Mentat Inc.
 *
 *
 *	MOL adaption by BenH:
 *
 *	Copyright (C) 1999 Benjamin Herrenschmidt (bh40@calva.net)
 *
 *	Copyright (C) 2002 by Samuel Rydh
 */

#include "enet.h"
#include "logger.h"
#include "LinuxOSI.h"
#include "misc.h"


InterruptMemberNumber
mol_enet_irq( InterruptSetMember ISTmember, void *refCon, UInt32 theIntCount )
{
	dle_t *dle = &globals.board_dle;
	UInt32 status, packet_length;
	mblk_t *mp;

	if( !OSI_EnetControl( globals.osi_id, kEnetAckIRQ ) )
		return kIsrIsNotComplete;
	
	OTEnterInterrupt();

	status = OSI_EnetGetStatus( globals.osi_id );

	/* Check for receive packets availability */
	for( ; status & kEnetStatusRxPacket; status=OSI_EnetGetStatus(globals.osi_id)) {
		packet_length = (status & kEnetStatusRxLengthMask);
		if( packet_length > MAX_PACKET_SIZE) {
			lprintf("Packet too big (%d)\n", packet_length);
			OSI_EnetGetPacket(globals.osi_id, NULL);
			dle->dle_istatus.receive_discards++;
			continue;
		}
		
		/* Perhaps use esballoc() to avoid data copy? */
		mp = allocb(packet_length, BPRI_LO);
		if( !mp) {
			static int warned=0;
			if( warned < 3 )
				lprintf("Can't allocate packet datas (%d)\n", packet_length);
			else if(warned==3) 
				lprintf("Turning off allocation warnings.");
			warned++;
			
			OSI_EnetGetPacket(globals.osi_id, NULL);
			dle->dle_istatus.receive_discards++;
			continue;
		}

		/* Copy the data into mp->b_rptr. */
		if( OSI_EnetGetPacket(globals.osi_id, GetPhysicalAddr(/*mp->b_rptr*/globals.rx_buf))) {
			freemsg(mp);
			dle->dle_istatus.receive_discards++;
			continue;
		}
		OTMemcpy(mp->b_rptr, globals.rx_buf, packet_length);
		mp->b_wptr = mp->b_rptr + packet_length;
				
		/* Deliver the packet. */
		dle_inbound(dle, mp);
	}
	OTLeaveInterrupt();
	
	return kIsrIsComplete;
}

OTInt32
mol_enet_close( queue_t *q, OTInt32 flag, cred_t *credp )
{
	/*
	 * NOTE: there is probably not much else that needs to be done
	 * in this routine.  If you need to know about the number of
	 * open instances, dle_refcount is the number of streams still
	 * referencing the device (decremented in dle_close).
	 */
	return dle_close(q);
}

void
mol_enet_hw_address_filter_reset( void *dummy, dle_addr_t *addr_list, ulong addr_cnt, ulong promisc_cnt, 
				  ulong multi_promisc_cnt, ulong accept_broadcast, ulong accept_error )
{
	dle_t *dle = &globals.board_dle;
	unsigned char *first_phys_addr=0;
	ulong i, phys_addr_cnt=0;
	dle_addr_t *dlea;
	
	for( i=0; i<globals.multi_count; i++) {
		OSI_EnetDelMulticast(globals.osi_id, GetPhysicalAddr(&globals.multi_addrs[i]));
	}
	globals.multi_count = 0;
	
	/* Calculate the new logical address filter for multicast addresses. */
	for( dlea = addr_list; dlea; dlea = dlea->dlea_next) {
		if( dlea->dlea_addr[0] & 0x1) {
			/*
			 * If the address is a multicast address, then set
			 * the right bits in the new filter.
			 */
			 if( globals.multi_count < MAX_MULTICAST) {
			 	i = globals.multi_count++;
			 	globals.multi_addrs[i] = *((multi_addr_t*)dlea->dlea_addr);
				OSI_EnetAddMulticast(globals.osi_id, GetPhysicalAddr(&globals.multi_addrs[i]));
			 }
		} else {
			/*
			 * Additional physical address.  This does not happen
			 * with the first version of the DLPI template code.
			 * However, at some future time, we may want to
			 * support multiple physical addresses on one
			 * hardware tap.
			 */
			if( !first_phys_addr)
				first_phys_addr = dlea->dlea_addr;
			phys_addr_cnt++;
		}
	}

	if( first_phys_addr) {
		/*
		 * If there were any physical, non-multicast addresses in
		 * the list, then check to see if the first one is different
		 * from the current one.  If so, copy the new address into
		 * dle_current_addr and take whatever steps are necessary
		 * with the hardware to change the physical address.
		 */
		;
	}

	/*
	 * Compare the new address filter with the old one.  If the new
	 * one is different, then set the hardware appropriately.
	 */

	/*
	 * If there are multiple physical addresses, then the board
	 * probably needs to be put in a promiscuous state.
	 */
	if( phys_addr_cnt > 1)
		promisc_cnt |= 1;

	if( promisc_cnt || multi_promisc_cnt)
		/* Set the board into promiscuous mode. */;
	else
		/* Clear any promiscuous mode that may be set*/;
	/*
	 * Save the promiscuous setting in the board structure so that
	 * updates to the hardware registers from mol_enet_start or other
	 * routines will be correct.
	 */
}

void
mol_enet_hw_start( void *dummy )
{
	/* Kick the board alive and allow receive interrupts. */
	OSI_EnetControl( globals.osi_id, kEnetCommandStart );
}

void
mol_enet_hw_stop( void *dummy )
{
	/* Turn off interrupts and leave the hardware disabled. */
	OSI_EnetControl( globals.osi_id, kEnetCommandStop );
}

OTInt32
mol_enet_open( queue_t *q, dev_t *devp, OTInt32 flag, OTInt32 sflag, cred_t *credp )
{
	int ret_code;

	/*
	 * This routine probably does not need to do anything other
	 * than call dle_open.  dle_refcnt is incremented.
	 */
	ret_code = dle_open( &globals.board_dle, q, devp, flag, sflag, credp, sizeof(mol_enet_t) );
	return ret_code;
}

/* 
 * Read-side service routine.
 * Pass all inbound packets upstream.  Messages are placed on
 * the read-side queue by dle_inbound.  Unless the hardware
 * requires something special, no additional code should be
 * required.  NOTE: Additional messages may be added to the
 * queue while this routine is running. If canputnext() fails,
 * messages are freed rather than put back on the queue.  
 * This is necessary since dle_inbound() will continue to add 
 * messages without checking flow control (it can't call canputnext()
 * from interrupt context).
 */
long
mol_enet_rsrv( queue_t *q )
{
	mblk_t *mp;

	while( (mp=getq(q)) ) {
		/*
		 * Private message to be passed back to the dlpiether
		 * code.  This interface is required for supporting
		 * 802.2 XID and Test packets.
		 */
		if( mp->b_datap->db_type == M_CTL) {
			dle_rsrv_ctl(q, mp);
		} else if( canputnext(q))
			putnext(q, mp);
		else {
			freemsg(mp);
			flushq(q, FLUSHDATA);
			break;
		}
	}
	return 0;
}

/* write-side put routine. Only handles M_DATA, handing others to dlpiether. */
OTInt32
mol_enet_wput( queue_t *q, mblk_t *mp )
{
	unsigned char *xmt_buf;
	long remaining, total;
	mol_enet_t *loop;
	mblk_t *first_mp;

	loop = (mol_enet_t *)q->q_ptr;
	
	if( mp->b_datap->db_type != M_DATA ) {
		mp = dle_wput(q, mp);
		if( !mp )
			return 0;
		switch( mp->b_datap->db_type ) {
		case M_DATA:
			break;		/* it's ready to send */
		case M_IOCNAK:
			/*
			 * Any driver private ioctl's come back from
			 * dle_wput() as an M_IOCNAK with ioc_error
			 * set to EINVAL; the rest of the original M_IOCTL
			 * is intact (including ioc_cmd & trailing M_DATAs).
			 * It may be processed here.
			 */
			 qreply(q,mp);
			 return 0;
		default:
			/* dle_wput() has formatted the reply for us */
			qreply(q, mp);
			return 0;
		}
	}

	/*
	 * Copy the packet to transmit buffer.  This is an example
	 * showing the copies and the calculation of the length
	 * of the packet.  Code for actual hardware devices will
	 * obviously need to be updated, at least to initialize
	 * xmt_buf to point to the hardware transmit area.
	 */
	remaining = MAX_PACKET_SIZE;
	first_mp = mp;
	xmt_buf = globals.xmt_buf;
	do {
		unsigned char	*rptr = mp->b_rptr;
		int	len = mp->b_wptr - rptr;
		if( len <= 0)
			continue;
		if( remaining < len ) {
			/* packet too large */
			mp = dle_wput_ud_error(first_mp, DL_UNDELIVERABLE, 0);
			if( mp)
				qreply(q, mp);
			return 0;
		}
		remaining -= len;
		xmt_buf += len;

		/* XXX no point using noncacheable memory... */
		//board_bcopy_to_noncached(rptr, xmt_buf - len, len);
		OTMemcpy( xmt_buf - len, rptr, len );
	} while( (mp=mp->b_cont) && remaining );

	total = MAX_PACKET_SIZE - remaining;

	/* Fill in the 802 length field if it is zero. */
	{
	unsigned char *up;
	up = &xmt_buf[-total];
	if( !(up[12] | up[13]) ) {
		ulong adjusted_total = total - 14;
		up[12] = (unsigned char)(adjusted_total >> 8);
		up[13] = (unsigned char)(adjusted_total & 0xff);
	}
	}

	if( total < MIN_PACKET_SIZE ) {
		/*
		 * If the packet is less than the minimum transmit
		 * size, then you need to pad the packet.  Hopefully
		 * this is just of matter of setting the hardware
		 * to pad automatically.
		 */
		 ;
	}
	
	/* Transmit the packet defined by mp->b_rptr to mp->b_wptr. */
	if( OSI_EnetSendPacket(globals.osi_id, GetPhysicalAddr(globals.xmt_buf), total)) {
		((dle_t *)loop->mol_enet_dle)->dle_istatus.transmit_discards++;
		lprintf("Transmit discarded\n");
		freemsg( first_mp ); 
		/* XXX, should it be: freemsg( mp ); */
		return 0;
	}

	/* Increment the appropriate MIB interface statistics. */
	((dle_t *)loop->mol_enet_dle)->dle_istatus.bytes_sent += total;
	if( first_mp->b_rptr[0] & 0x1 ) {
		unsigned char *rptr = first_mp->b_rptr;
		if( (rptr[0] & rptr[1] & rptr[2] & rptr[3] & rptr[4] & rptr[5]) == 0xFF)
			((dle_t *)loop->mol_enet_dle)->dle_istatus.broadcast_frames_sent++;
		else
			((dle_t *)loop->mol_enet_dle)->dle_istatus.multicast_frames_sent++;
	} else
		((dle_t *)loop->mol_enet_dle)->dle_istatus.unicast_frames_sent++;

	freemsg( first_mp );

	return 0;
}
