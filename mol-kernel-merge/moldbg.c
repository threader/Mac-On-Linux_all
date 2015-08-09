/* 
 *   Copyright (C) 2004 Samuel Rydh (samuel@ibrium.se)
 *   
 *   This program is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU General Public License
 *   version 2
 *   
 */

/* FIXME - cleanup */
#ifdef CONFIG_MOL_HOSTED
#include "kernel_vars.h"
#include "misc.h"
#include "context.h"
#include <stdarg.h>
#include "osi_calls.h"

int printm(const char *fmt, ...)
{
	char *p, buf[1024];	/* XXX: no buffer overflow protection... */
	va_list args;
	int i;

	va_start(args, fmt);
	i = vsprintf(buf, fmt, args);
	va_end(args);

	for (p = buf; *p; p++)
		OSI_PutC(*p);
	return i;
}

void debugger(int n)
{
	printm("<debugger: %x>\n", n);
	OSI_Debugger(n);
}

#endif
