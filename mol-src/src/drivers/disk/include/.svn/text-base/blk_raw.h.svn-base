/* 
 * <blk-raw.h>
 *	
 * Raw block device read/write functions
 * Copyright (C) 2005 Joseph Jezak
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

#ifndef _H_BLK_RAW
#define _H_BLK_RAW

#include "mol_config.h"
#include "disk.h"
#include "ablk.h"
#include "llseek.h"
#include <sys/uio.h>

/* Function declarations */
int raw_open(int fd, bdev_desc_t *bdev);
int raw_read(bdev_desc_t *bdev, struct iovec *vec, int nb_sectors);
int raw_write(bdev_desc_t *bdev, struct iovec *vec, int nb_sectors);
int raw_seek(bdev_desc_t *bdev, long block, long offset);

#endif
