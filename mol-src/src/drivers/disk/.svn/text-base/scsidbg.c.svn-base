/* 
 *   Creation Date: <2004/03/23 22:19:41 samuel>
 *   Time-stamp: <2004/04/03 11:25:48 samuel>
 *   
 *	<scsidbg.c>
 *	
 *	SCSI debug
 *   
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 */


#include "mol_config.h"
#include <scsi/sg.h>
#include <sys/ioctl.h>
#include "scsi-client.h"
#include "disk.h"

static struct {
	int		cmd;
	char		*name;
} cmd_names[] = {
	{ 0x00,		"TEST UNIT READY" 		},
	{ 0x03,		"REQUEST SENSE" 		},
	{ 0x04,		"FORMAT UNIT" 			},
	{ 0x12,		"INQUIRY" 			},
	{ 0x1B,		"START STOP UNIT"		},
	{ 0x1E,		"P/A MEDIUM REMOVAL"		},
	{ 0x23,		"READ FORMAT CAPACITIES"	},
	{ 0x25,		"READ CAPACITY" 		},
	{ 0x28,		"READ (10)" 			},
	{ 0x2A,		"WRITE (10)" 			},
	{ 0x2B,		"SEEK (10)" 			},
	{ 0x2E,		"WRITE AND VERIFY"		},
	{ 0x2F,		"VERIFY" 			},
	{ 0x35,		"SYNCHRONIZE CACHE"		},
	{ 0x42,		"READ SUB CHANNEL"		},
	{ 0x43,		"READ TOC/PMA/ATIP"		},
	{ 0x45,		"PLAY AUDIO" 			},
	{ 0x46,		"GET CONFIGURATION"		},
	{ 0x47,		"PLAY AUDIO MSF"		},
	{ 0x4A,		"GET EVENT/STATUS NOTIF."	},
	{ 0x4B,		"PAUSE / RESUME"		},
	{ 0x4E,		"STOP PLAY/SCAN"		},
	{ 0x51,		"READ DISC INFO"		},
	{ 0x52,		"READ TRACK INFO"		},
	{ 0x53,		"RESERVE TRACK"			},
	{ 0x54,		"SEND OPC INFO"			},
	{ 0x55,		"MODE SELECT (10)"		},
	{ 0x58,		"REPAIR TRACK"			},
	{ 0x5A,		"MODE SENSE (10)"		},
	{ 0x5B,		"CLOSE TRACK/SESSION"		},
	{ 0x5C,		"READ BUFFER CAPACITY"		},
	{ 0x5D,		"SEND CUE SHEET"		},
	{ 0xA1,		"BLANK"				},
	{ 0xA2,		"SEND EVENT"			},
	{ 0xA3,		"SEND KEY"			},
	{ 0xA4,		"REPORT KEY"			},
	{ 0xA5,		"PLAY AUDIO (12)"		},
	{ 0xA6,		"LOAD/UNLOAD CD/DVD"		},
	{ 0xA7,		"SET READ AHEAD"		},
	{ 0xA8,		"READ (12)"			},
	{ 0xAA,		"WRITE (12)"			},
	{ 0xAC,		"GET PERFORMANCE"		},
	{ 0xAD,		"READ DVD STRUCTURE"		},
	{ 0xB6,		"SET STREAMING"			},
	{ 0xB9,		"READ CD MSF"			},
	{ 0xBA,		"SCAN"				},
	{ 0xBB,		"SET CD SPEED"			},
	{ 0xBD,		"MECHANISM STATUS"		},
	{ 0xBE,		"READ CD"			},
	{ 0xBF,		"SEND DVD STRUCTURE"		},

	/* ATAPI devices might not support these */
	{ 0x1a,		"MODE SENSE (6)"		},
	{ 0x15,		"MODE SELECT (6)"		},
};

static struct {
	int		sense;
	int		critical;
	char		*error;
} sense_tab[] = {
	{ 0x50404, 0,	"Format in progress",				},

	{ 0x20401, 0,	"LU is in progress of becoming ready"		},
	{ 0x20408, 0,	"LU not ready, long write in progress"		},
	{ 0x20404, 0,	"LU not ready, format in progress"		},
	{ 0x23002, 0,	"Cannot read medium - unknown format"		},
	{ 0x23a00, 0,	"Medium not present",				},

	{ 0x52400, 1,	"Invalid field in CDB",				},
	{ 0x52000, 1,	"Invalid Command Operation Code",		},

	{ 0x62800, 0,	"Not ready to ready change, medium may have changed" 	},
	{ 0x62900, 0,  	"Power ON, reset, or bus-device reset occured"	},
};

static void 
dump_cmd_name( scsi_ureq_t *u )
{
	int i;
	for( i=0; i<sizeof(cmd_names)/sizeof(cmd_names[0]); i++ ) {
		if( u->cdb[0] == cmd_names[i].cmd ) {
			printm( C_GREEN "%-20s"C_NORMAL, cmd_names[i].name );
			return;
		}
	}
	printm(C_RED "Unknown SCSI command %02x" C_NORMAL, u->cdb[0] );
}

static void
dump_sg( scsi_ureq_t *u, int count, char *str )
{
	int s, i, j;

	printm("%s [ ", str );
	for( s=0, i=0; i<u->n_sg; i++ ) {
		char *p = u->iovec[i].iov_base;
		for( j=0; s<u->size && s<count && j<u->iovec[i].iov_len; j++, s++ ) {
			printm("%02x ", p[j] );
			if( (s % 20) == 19 )
				printm("\n      ");
		}
	}
	printm("]\n");
}

static void
dump_sense( int sense )
{
	int i;
	for( i=0; i<sizeof(sense_tab)/sizeof(sense_tab[0]); i++ ) {
		if( sense_tab[i].sense == sense ) {
			printm("%s%s\n" C_NORMAL, sense_tab[i].critical ? C_RED : C_NORMAL,
			       sense_tab[i].error );
			return;
		}
	}
	printm( C_RED "Sense %x\n" C_NORMAL, sense );
}

void
scsidbg_dump_cmd( scsi_ureq_t *u )
{
	int i;

	dump_cmd_name( u );

	printm("%5d [ ", u->size );
	for( i=0; i<u->cdb_len; i++ )
		printm("%02x ", u->cdb[i] );
	while( i++ < 12 )
		printm("-- " );
	printm("]  ");

	if( !u->scsi_status )
		printm( C_GREEN "OK\n" C_NORMAL );
	else {
		if( u->sb_actlen >= 14)
			dump_sense( (u->sb[2] & 0xf) << 16 | (u->sb[12] << 8) | u->sb[13] );
		else
			printm( C_RED "[STATUS %x]\n" C_NORMAL, u->scsi_status );
	}
	
	if( !u->scsi_status && u->act_size )
		dump_sg( u, 31, u->is_write ? "OUT" : "IN " );
}
