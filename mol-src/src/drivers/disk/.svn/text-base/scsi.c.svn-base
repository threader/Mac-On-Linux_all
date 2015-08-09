/* 
 *   Creation Date: <2003/07/09 12:49:43 samuel>
 *   Time-stamp: <2004/03/25 17:15:56 samuel>
 *   
 *	<scsi.c>
 *	
 *	SCSI host controller
 *   
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#include "mol_config.h"
#include <stddef.h>
#include "promif.h"
#include "memory.h"
#include "pic.h"
#include "os_interface.h"
#include "driver_mgr.h"
#include "osi_driver.h"
#include "pci.h"
#include "booter.h"
#include "scsi_sh.h"
#include "scsi-client.h"


#define MAX_NUM_TARGETS		8

/* client device */
struct scsi_dev {
	scsi_ops_t	ops;
	int		privsize;		/* private client data size */
	void		*refcon;
	char		*last_sense;

	int		hack_allow_read;	/* allow READ_X to this unit */
};

/* wrapped request */
typedef struct wreq {
	struct wreq	*next;
	scsi_req_t	*r;

	scsi_ureq_t	u;			/* must be last */
} wreq_t;

/* controller state */
static struct {
	int		irq;

	int		first_completed;	/* mphys of completed req chain */
	int		*completed_tail;	/* where to queue completed requests */

	scsi_dev_t	*targets[ MAX_NUM_TARGETS ];
} sc;

static inline wreq_t *RECOVER_WREQ( scsi_ureq_t *u ) {
	return (wreq_t*)((char*)u - offsetof(wreq_t, u)); 
}

static void
complete( scsi_dev_t *dev, wreq_t *w )
{
	scsi_req_t *r = w->r;
	int i;
	/* dev might be NULL */

	/* best to clear the return buffer if there is an error... */
	if( w->u.scsi_status && !w->u.is_write )
		for( i=0; i<w->u.n_sg; i++ )
			memset( w->u.iovec[i].iov_base, 0, w->u.iovec[i].iov_len );

	/* construct sense information if it is missing (hrm... should we really do this?) */
	if( !w->u.sb_actlen && w->u.scsi_status == SCSI_STATUS_CHECK_CONDITION ) {
		char *p = w->u.sb;
		memset( w->u.sb, 0, sizeof(w->u.sb) );
		p[0] = 0x70;
		p[2] = 2;		/* XXX: it this the best error to return? */
		p[12] = p[13] = 0;
		w->u.sb_actlen = 18;
	}

	/* dump debug info */
	if( r->adapter_status != kAdapterStatusNoTarget )
		scsidbg_dump_cmd( &w->u );

	/* copy sense data */
	if( w->u.sb_actlen ) {
		int i, s;
		/* save a copy (if we receive a REQUEST SENSE request later on) */
		if( !dev->last_sense )
			dev->last_sense = malloc(sizeof(w->u.sb));
		if( dev->last_sense )
			memcpy( dev->last_sense, w->u.sb, w->u.sb_actlen );
		
		/* copy to client */
		for( s=0, i=0; i<2; i++ ) {
			char *p = transl_mphys( r->sense[i].base );
			int size = r->sense[i].size;
			while( p && size-- && s < w->u.sb_actlen )
				*p++ = w->u.sb[s++];
		}
	} else if( dev ) {
		char *sp = dev->last_sense;
		
		/* if this a REQUEST_SENSE and we have sense data, then use it */
		if( sp && r->cdb[0] == 0x03 ) {
			int i, s, size = sp[7] + 8;
			if( w->u.size < size )
				size = w->u.size;

			for( s=0, i=0; i<w->u.n_sg; i++ ) {
				char *p = w->u.iovec[i].iov_base;
				int segsize = w->u.iovec[i].iov_len;
				while( segsize-- && s < size )
					*p++ = sp[s++];
			}
			w->u.act_size = s;
			/* printm("Copied %d sense data bytes\n", s ); */
			/* printm("Sense: %x\n", sp[2] <<16 | sp[12] <<8 | sp[13] ); */
		}

		/* discard (obsolete) sense data */
		if( dev->last_sense )
			free( dev->last_sense );
		dev->last_sense = NULL;
	}

	r->act_size = w->u.act_size;
	r->ret_sense_len = w->u.sb_actlen;
	r->scsi_status = w->u.scsi_status;

	r->next_completed = 0;
	
	*sc.completed_tail = r->client_addr;
	sc.completed_tail = &r->next_completed;

	irq_line_hi( sc.irq );
	free( w );
}


/* HACK: There are two kernel problems we work around here:
 *
 * - 2.6.5 hangs if the (obsolete) commands MODE_SENSE and MODE_SELECT are issued
 *
 * - IDE timeouts are not handled very well; a timeout might render the CD (or the complete system)
 * useless. In particular, the case when a read command is issued before the CD has been
 * spun up/configured properly must be addressed. Mac OS 9 typically issue a READ before
 * the CD is configured...
 */
static int
hack_execute_hook( scsi_dev_t *dev, wreq_t *w )
{
	int cmd = w->u.cdb[0];

	if( cmd == SCSI_CMD_MODE_SENSE || cmd == SCSI_CMD_MODE_SELECT )
		goto abort;
	if( dev->hack_allow_read )
		return 0;
	if( cmd == SCSI_CMD_READ_10 || cmd == SCSI_CMD_READ_12 || cmd == SCSI_CMD_READ_6 )
		goto abort;
	if( cmd != SCSI_CMD_INQUIRY )
		dev->hack_allow_read = 1;
	return 0;
abort:
	w->u.scsi_status = SCSI_STATUS_CHECK_CONDITION;
	complete( dev, w );
	return 1;
}

static int
osip_scsi_submit( int selector, int *params )
{
	scsi_req_t *r = transl_mphys( params[0] );
	int i, j, nsg, len, pdata_len;
	const sglist_t *sglist;
	scsi_dev_t *dev;
	wreq_t *w;
	char *p;
	
	if( !r )
		return 1;

	dev = ((uint)r->target < MAX_NUM_TARGETS) ? sc.targets[r->target] : NULL;

	/* allocate data (wreq, iovec, priv_data) */
	nsg = r->n_sg;
	if( nsg < 0 || r->cdb_len > sizeof(r->cdb) )
		return 1;
	pdata_len = dev ? dev->privsize : 0;
	if( !(w=malloc(sizeof(wreq_t) + nsg*sizeof(struct iovec) + pdata_len)) )
		return 1;

	/* fill in some fields */
	w->r = r;
	w->u.pdata = &w->u.iovec[nsg];
	w->u.is_write = r->is_write;
	w->u.cdb_len = r->cdb_len;
	for( i=0; i<r->cdb_len; i++ )
		w->u.cdb[i] = r->cdb[i];
	while( i < 12 )
		w->u.cdb[i++] = 0;

	/* copy scatter and gather list */
	len = i = j = 0;
	sglist = &r->sglist;
	while( i < nsg ) {
		int s = sglist->vec[j].size;
		if( !(p=transl_mphys_(sglist->vec[j].base)) )
			goto bad;

		w->u.iovec[i].iov_base = p;
		w->u.iovec[i++].iov_len = s;
		len += s;

		if( ++j == sglist->n_el ) {
			int next = sglist->next_sglist;
			if( !next || !(sglist=transl_mphys_(next)) )
				break;
			j = 0;
		}
	}
	if( r->size < len )
		len = r->size;
	else if( r->size > len )
		goto bad;

	w->u.n_sg = i;
	w->u.size = len;

	/* return parameters */
	w->u.sb_actlen = 0;
	w->u.act_size = 0;
	w->u.scsi_status = 0;

	if( !r->lun && dev ) {
		r->adapter_status = 0;
		/* we need to fix a few things */
		if( !hack_execute_hook(dev, w) )
			(*dev->ops.execute)( &w->u, dev->refcon );
	} else {
		r->adapter_status = kAdapterStatusNoTarget;
		complete( dev, w );
	}
	return 0;
 bad:
	printm("issue-bad\n");
	free( w );
	return 1;
}

static int
osip_scsi_ack( int sel, int *params )
{
	int ret;
	if( !irq_line_low(sc.irq) )
		return 0;

	ret = sc.first_completed | SCSI_ACK_IRQFLAG;

	sc.first_completed = 0;
	sc.completed_tail = &sc.first_completed;

	return ret;
}

static int
osip_scsi_cntrl( int sel, int *params )
{
	switch( params[0] ) {
	case SCSI_CTRL_INIT:
	case SCSI_CTRL_EXIT:
		sc.first_completed = 0;
		sc.completed_tail = &sc.first_completed;
		break;
	default:
		return -1;
	}
	return 0;
}


/************************************************************************/
/*	client interface						*/
/************************************************************************/

void
complete_scsi_req( scsi_dev_t *dev, scsi_ureq_t *u )
{
	wreq_t *w = RECOVER_WREQ( u );
	complete( dev, w );
}

scsi_dev_t *
register_scsidev( scsi_ops_t *ops, int privsize, void *refcon )
{
	scsi_dev_t *d;
	int i;
	
	for( i=0; i<MAX_NUM_TARGETS && (i==7 || sc.targets[i]); i++ )
		;
	if( i == MAX_NUM_TARGETS || !(d=malloc(sizeof(*d))) )
		return NULL;
	memset( d, 0, sizeof(*d) );

	d->ops = *ops;
	d->refcon = refcon;
	d->privsize = privsize;
	
	sc.targets[i] = d;
	return d;
}

void
unregister_scsidev( scsi_dev_t *d )
{
	int i;
	for( i=0; i<MAX_NUM_TARGETS && sc.targets[i] != d; i++ )
		;
	if( i == MAX_NUM_TARGETS ) {
		printm("bad scsidev!\n");
		return;
	}
	sc.targets[i] = NULL;

	if( d->last_sense )
		free( d->last_sense );
	free( d );
}



/************************************************************************/
/*	init / cleanup							*/
/************************************************************************/

static const osi_iface_t osi_iface[] = {
        { OSI_SCSI_CNTRL,	osip_scsi_cntrl		},
        { OSI_SCSI_SUBMIT,	osip_scsi_submit	},
        { OSI_SCSI_ACK,		osip_scsi_ack		},
};

static void
scsi_cleanup( void )
{
	cd_scsi_cleanup();
	sg_scsi_cleanup();
}

static int
scsi_init( void )
{
	static pci_dev_info_t pci_config = { 0x1000, 0x0003, 0x02, 0x0, 0x0100 };
	pci_dev_info_t *pcii = is_classic_boot() ? &pci_config : NULL;
	mol_device_node_t *dn = prom_find_devices("mol-scsi");
	int i;
	
	if( !dn )
		return 0;
	if( !register_osi_driver("scsi", "mol-scsi", pcii, &sc.irq) ) {
		printm("Failed to register SCSI controller\n");
		return 0;
	}
	register_osi_iface( osi_iface, sizeof(osi_iface) );

	printm("SCSI devices:\n");

	/* sg_scsi must be initialized first in order to avoid double exports */
	sg_scsi_init();
	cd_scsi_init();

	for( i=0; i<MAX_NUM_TARGETS && !sc.targets[i]; i++ )
		;
	if( i == MAX_NUM_TARGETS ) {
		printm("    <No SCSI Devices>\n");
		prom_delete_node( dn );
		scsi_cleanup();
		return 0;
	}
	printm("\n");
	return 1;
}

driver_interface_t scsi_driver = {
	"scsi", scsi_init, scsi_cleanup
};
