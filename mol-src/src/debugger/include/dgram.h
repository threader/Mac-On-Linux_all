/* 
 *   Creation Date: <2000/09/16 12:30:36 samuel>
 *   Time-stamp: <2003/06/08 14:11:44 samuel>
 *   
 *	<dgram.h>
 *	
 *	MOL <-> debugger interface
 *   
 *   Copyright (C) 2000, 2001, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_DGRAM
#define _H_DGRAM

#define MOL_DBG_PACKET_MAGIC	0x13135559
#define MAX_DBG_PKG_SIZE	(0x1000 * 10)

#define BREAK_BUF_SIZE         256

#define MOL_SOCKET_NAME		"/tmp/.mol-socket"

typedef struct dgram_receiver dgram_receiver_t;

enum {
	kMDG_mregs = 1,
	kMDG_write_mregs,
	kMDG_connect,		/* char *sockname 	*/
	kMDG_disconnect,	/* char *sockname 	*/
	kMDG_yn_question,	/* int default 		*/
	kMDG_yn_answer,		/* int yes, char *str	*/
	kMDG_printm,		/* char *str 		*/
	kMDG_refresh_instwin,
	kMDG_refresh_dbgwin,
	kMDG_refresh_debugger,
	kMDG_in_ppc_mode,
	kMDG_read_dpage,	/* ea, context, use_dtrans */
	kMDG_dpage_data,	/* double page with data */
	kMDG_debug_action,
	kMDG_add_breakpoint,
	kMDG_is_breakpoint,	/* ea -- buf[BREAK_BUF_SIZE] */
	kMDG_dbg_cmd,
	kMDG_result,
};

typedef struct mol_dgram {
	ulong	magic;				/* Don't touch these */
	ulong	total_len;			/* two fields! */
	
	int	what;
	int	p0,p1,p2,p3;

	/* Convenience fields, initialized by the receiver */
	int	data_size;
	char	*data;
	struct	mol_dgram *next;

	/* the actual data goes here */
} mol_dgram_t;

#define MAX_CMD_NUM_ARGS	10
typedef struct {
	int 	argc;
	int	offs[MAX_CMD_NUM_ARGS];
	char	buf[512];
} remote_cmd_t;

extern mol_dgram_t	*receive_dgram( dgram_receiver_t *dr, int *err );
extern void 		free_dgram_receiver( dgram_receiver_t *dr );
extern dgram_receiver_t *create_dgram_receiver( int sock );

extern char 		*molsocket_name( void );	/* malloced */

extern int send_dgram_buf4( int fd, int what, const char *buf, int size, int p0, int p1, int p2, int p3 );

static inline int send_dgram_buf1( int fd, int what, const char *buf, int size, int p0 ) 
	{ return send_dgram_buf4( fd, what, buf, size, p0, 0, 0, 0 ); }
static inline int send_dgram_buf2( int fd, int what, const char *buf, int size, int p0, int p1 ) 
	{ return send_dgram_buf4( fd, what, buf, size, p0, p1, 0, 0 ); }
static inline int send_dgram_buf3( int fd, int what, const char *buf, int size, int p0, int p1, int p2 ) 
	{ return send_dgram_buf4( fd, what, buf, size, p0, p1, p2, 0 ); }
static inline int send_dgram_buf( int fd, int what, const char *buf, int size )
	{ return send_dgram_buf4( fd, what, buf, size, 0, 0, 0, 0); }

static inline int send_dgram_str( int fd, int what, const char *str )
	{ return send_dgram_buf1( fd, what, str, strlen(str)+1, 0 ); }
static inline int send_dgram_str1( int fd, int what, const char *str, int p1 )
	{ return send_dgram_buf1( fd, what, str, strlen(str)+1, p1 ); }

static inline int send_dgram( int fd, int what ) 
	{ return send_dgram_buf4(fd, what, NULL, 0, 0,0,0,0 ); }
static inline int send_dgram1( int fd, int what, int p0 ) 
	{ return send_dgram_buf4(fd, what, NULL, 0, p0,0,0,0 ); }
static inline int send_dgram2( int fd, int what, int p0, int p1 ) 
	{ return send_dgram_buf4(fd, what, NULL, 0, p0,p1,0,0 ); }
static inline int send_dgram3( int fd, int what, int p0, int p1, int p2 ) 
	{ return send_dgram_buf4(fd, what, NULL, 0, p0,p1,p2,0 ); }
static inline int send_dgram4( int fd, int what, int p0, int p1, int p2, int p3 )
	{ return send_dgram_buf4(fd, what, NULL, 0, p0,p1,p2,p3 ); }

#endif   /* _H_DGRAM */
