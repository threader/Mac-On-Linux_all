/* 
 *   Creation Date: <2001/01/31 23:19:55 samuel>
 *   Time-stamp: <2003/06/08 13:58:04 samuel>
 *   
 *	<nub.h>
 *	
 *	Definitions for the MOL-side debugger
 *   
 *   Copyright (C) 2001, 2002, 2003 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifndef _H_NUB
#define _H_NUB

/* nub.c */
extern int	yn_question( char *prompt, int defanswer );

/* mmu_cmds.c */
extern void	add_mmu_cmds( void );



#endif   /* _H_NUB */
