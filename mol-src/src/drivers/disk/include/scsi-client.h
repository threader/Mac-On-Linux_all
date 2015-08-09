/* 
 *   Creation Date: <2003/07/09 23:42:40 samuel>
 *   Time-stamp: <2004/03/25 16:41:54 samuel>
 *   
 *	<scsi-client.h>
 *	
 *	SCSI client interface
 *   
 *   Copyright (C) 2003, 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

#ifndef _H_SCSI_CLIENT
#define _H_SCSI_CLIENT

#include <sys/uio.h>

typedef struct scsi_dev scsi_dev_t;

typedef struct scsi_ureq  {
	struct scsi_ureq	*next;			/* may be used by client */
	void			*pdata;			/* private client data */
	
	unsigned char		is_write;
	unsigned char		scsi_status;		/* [O] SCSI status */
	unsigned char		filler;
	unsigned char		cdb_len;

	char			cdb[16];

	int			sb_actlen;		/* returned sense */
	char			sb[252];		/* sense buffer */

	int			size;			/* data size */
	int			act_size;		/* bytes transfered */
	int			n_sg;
	struct iovec		iovec[0];
} scsi_ureq_t;

typedef struct {
	void		(*execute)( scsi_ureq_t *r, void *refcon );
} scsi_ops_t;

extern scsi_dev_t 	*register_scsidev( scsi_ops_t *ops, int priv_size, void *refcon );
extern void	 	unregister_scsidev( scsi_dev_t *dev );
extern void		complete_scsi_req( scsi_dev_t *dev, scsi_ureq_t *r );


#ifdef __linux__
extern void		sg_scsi_init( void );
extern void		sg_scsi_cleanup( void );
extern void		cd_scsi_init( void );
extern void		cd_scsi_cleanup( void );
#else
static inline void	sg_scsi_init( void ) {}
static inline void	sg_scsi_cleanup( void ) {}
static inline void	cd_scsi_init( void ) {}
static inline void	cd_scsi_cleanup( void ) {}
#endif

#ifdef CONFIG_SCSIDEBUG
extern void 		scsidbg_dump_cmd( scsi_ureq_t *u );
#else
static inline void 	scsidbg_dump_cmd( scsi_ureq_t *u ) {}
#endif

/* careful! linux uses dowshifted variants of these... */
#define SCSI_STATUS_GOOD			0x00
#define SCSI_STATUS_CHECK_CONDITION		0x02
#define SCSI_STATUS_CONDITION_GOOD		0x04
#define SCSI_STATUS_BUSY			0x08
#define SCSI_STATUS_INTERMEDIATE_GOOD		0x10
#define SCSI_STATUS_INTERMEDIATE_C_GOOD		0x14
#define SCSI_STATUS_RESERVATION_CONFLICT	0x18
#define SCSI_STATUS_COMMAND_TERMINATED		0x22
#define SCSI_STATUS_QUEUE_FULL			0x28

/* some SCSI commands */
#define SCSI_CMD_READ_6				0x08
#define SCSI_CMD_READ_10			0x28
#define SCSI_CMD_READ_12			0x28
#define SCSI_CMD_INQUIRY			0x12
#define SCSI_CMD_MODE_SELECT			0x15
#define SCSI_CMD_MODE_SENSE			0x1a


#endif   /* _H_SCSI_CLIENT */
