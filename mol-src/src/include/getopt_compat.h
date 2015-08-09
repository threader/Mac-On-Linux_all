/* 
 *   Creation Date: <2001/12/28 20:07:52 samuel>
 *   Time-stamp: <2001/12/28 20:30:44 samuel>
 *   
 *	<getopt.h>
 *	
 *	getopt_long compatibility glue
 *   
 *   Copyright (C) 2001 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   as published by the Free Software Foundation
 *   
 */

#ifdef __linux__
#include <getopt.h>
#else

struct option {
	char	*optname;
	int 	has_arg;
	int	*unimplemented_flag;
	int	val;
};

extern int getopt_long( int argc, char **argv, const char *opts, 
		    struct option *opt, int *opt_index );

#endif	/* __linux__ */
