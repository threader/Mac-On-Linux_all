/* Create mol disk images 
 * Copyright 2006, 2007 - Joseph Jezak
 *
 * Based create routines on qemu's qemu-img
 * Copyright (c) 2003 Fabrice Bellard
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "mol-img.h"

void help(void);

void help(void) {
	printf ("Usage: mol-img [options] output.img\n");
	printf ("Options\n");
	printf ("\t--type=TYPE\tBuild a disk image of a certain type (listed below)\n");
	printf ("\t--size=SIZE\tSize in bytes, postfix with M or G for megabytes or gigabytes\n");
	printf ("\t--help\t\tThis help text\n\n");
	printf ("Available Image Types: raw, qcow\n");
	exit(0);
}

int main(int argc, char **argv) {
	int type = QCOW_IMAGE;
	char file[256] = "mol.img";
	/* Default to 512M */
	int64_t size = 512 * SIZE_MB;
	int len;
	int64_t multiplier = 1;

	/* Parse command line arguments */
	if (argc > 1) {
		int args;
		for (args = 1; args < argc; args++) {
			if (!strncmp(argv[args], "--type", 6)) {
				if (!strncmp(argv[args] + 7, "raw", 3))
				    	type = RAW_IMAGE;
				if (!strncmp(argv[args] + 7, "qcow", 3))
				    	type = QCOW_IMAGE;
			}
			else if (!strncmp(argv[args], "--size",6)) {
				len = strlen(argv[args] + 7);			
				if (!strncmp(argv[args] + 6 + len, "M", 1))
					multiplier = SIZE_MB;		
				else if (!strncmp(argv[args] + 6 + len, "G", 1))
					multiplier = SIZE_GB;
				size = atoi(argv[args] + 7) * multiplier;
			}
			else if (!strncmp(argv[args], "--help",6)) {
				help();
			}
			/* Unknown option */
			else if (!strncmp(argv[args], "--",2)) {
				help();
			}
			/* Assume it's a filename */
			else {
				len = strlen(argv[args]);
				
				if (len < 256) {
					strncpy(file, argv[args], len);	
				}
				else {
					printf("Invalid filename!\n");
					exit(1);
				}
			}
		}
	}
	else {
		help();
	}
	
	if (type == RAW_IMAGE)
		create_img_raw(file, size);
	else if (type == QCOW_IMAGE)
		create_img_qcow(file, size);
	else
		exit(-1);
	exit(0);
}
