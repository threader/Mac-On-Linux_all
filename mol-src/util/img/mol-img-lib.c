/* Create mol disk images - common code between python and console frontend
 * Copyright 2006,2007 - Joseph Jezak
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
#include "byteorder.h"
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdint.h>
#include <unistd.h>

#include "mol-img.h"

#ifndef O_BINARY
#define O_BINARY 0
#endif

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

/* QCOW defines from blk_qcow.h */
typedef struct QCowHeader {
    uint32_t magic;
    uint32_t version;
    uint64_t backing_file_offset;
    uint32_t backing_file_size;
    uint32_t mtime;
    uint64_t size; /* in bytes */
    uint8_t cluster_bits;
    uint8_t l2_bits;
    uint32_t crypt_method;
    uint64_t l1_table_offset;
} QCowHeader;

#define QCOW_MAGIC (('Q' << 24) | ('F' << 16) | ('I' << 8) | 0xfb)
#define QCOW_VERSION 1

#define QCOW_CRYPT_NONE 0
#define QCOW_CRYPT_AES  1

#define QCOW_OFLAG_COMPRESSED (1LL << 63)

/* Create empty qcow disk - size in bytes */
int create_img_qcow(char * file, int64_t size) {
	int header_size, l1_size, i, shift, fd;
	QCowHeader header;
	uint64_t tmp;
	int ret = -1;

	/* Create the file */
	fd = open(file, O_EXCL | O_NOFOLLOW | O_WRONLY | O_CREAT | O_TRUNC | O_BINARY | O_LARGEFILE, 0644);
	if (fd < 0) {
		printf("Unable to open the file: %s for writing.\n", file);
		return -1;
	}
    
	memset(&header, 0, sizeof(header));
	header.magic = cpu_to_be32(QCOW_MAGIC);
	header.version = cpu_to_be32(QCOW_VERSION);
	header.size = cpu_to_be64(size);
	header_size = sizeof(header);

	/* no backing file */
        header.cluster_bits = 12; /* 4 KB clusters */
        header.l2_bits = 9; /* 4 KB L2 tables */

	header_size = (header_size + 7) & ~7;
	shift = header.cluster_bits + header.l2_bits;
	l1_size = (size + (1LL << shift) - 1) >> shift;

	header.l1_table_offset = cpu_to_be64(header_size);
/* AES Stuff not yet implemented
	if (flags) 
		header.crypt_method = cpu_to_be32(QCOW_CRYPT_AES);
	else
*/
	header.crypt_method = cpu_to_be32(QCOW_CRYPT_NONE);

	/* write all the data */
	if (write(fd, &header, sizeof(header)) == sizeof(header)) {
		lseek(fd, header_size, SEEK_SET);
		tmp = 0;
		ret = 0;
		for(i = 0;i < l1_size; i++) {
			if (write(fd, &tmp, sizeof(tmp)) != sizeof(tmp)) {
				ret = -1;
				break;
			}
		}
	}
	if (ret == -1) {
		printf("Unable to write the file: %s.\n", file);
	}
	close(fd);
	return ret;
}

/* Create empty raw disk - size in bytes */
int create_img_raw(char * file, int64_t size) {
	int fd;
	int ret = 0;
	
	/* Create the file */
	fd = open(file, O_EXCL | O_NOFOLLOW | O_WRONLY | O_CREAT | O_TRUNC | O_BINARY | O_LARGEFILE, 0644);
	if (fd < 0) {
		printf("Unable to open the file: %s for writing.\n", file);
		return -1;
	}

	if (ftruncate(fd, size) < 0) {
		printf("Unable to truncate the file: %s.\n", file);
		ret = -1;
	}
	close(fd);
	return ret;
}
