/* Create mol disk images, common definitions
 * Copyright 2007 - Joseph Jezak
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


#ifndef __MOL_IMG_H__
#define __MOL_IMG_H__

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif

int create_img_qcow(char *, int64_t);
int create_img_raw(char *, int64_t);

#define RAW_IMAGE 0
#define QCOW_IMAGE 1

#define SIZE_MB			1024 * 1024
#define SIZE_GB			1024 * 1024 * 1024

#endif /* __MOL_IMG_H__ */
