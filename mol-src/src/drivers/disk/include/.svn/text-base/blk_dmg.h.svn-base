/* 
 * <blk-dmg.h>
 *	
 * Apple dmg block device read/write functions
 * Copyright (C) 2005 Joseph Jezak
 *   
 * Based on the QEMU Block driver for DMG images
 * 
 * Copyright (c) 2004 Johannes E. Schindelin
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

#ifndef _H_BLK_DMG
#define _H_BLK_DMG

#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif /* _LARGEFILE64_SOURCE */

#include <zlib.h>
#include "platform.h"
#include "byteorder.h"
#include "ablk_sh.h"
#include "ablk.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/uio.h>
#include <math.h>
#include "vec_wrap.h"

typedef struct BDRVDMGState {
    int fd;
    
    /* each chunk contains a certain number of sectors,
     * offsets[i] is the offset in the .dmg file,
     * lengths[i] is the length of the compressed chunk,
     * sectors[i] is the sector beginning at offsets[i],
     * sectorcounts[i] is the number of sectors in that chunk,
     * the sectors array is ordered
     * 0<=i<n_chunks */

    u32 n_chunks;
    u32* types;
    u64* offsets;
    u64* lengths;
    u64* sectors;
    u64* sectorcounts;
    u32 current_chunk;
    char* compressed_chunk;
    char* uncompressed_chunk;
    z_stream zstream;
    
    /* Current sector we've seeked to */
    long seek_sector;
} BDRVDMGState;

/* Private DMG Struct, bdev is the argument */
#define DMG_PRIV(x) ( (BDRVDMGState *)(x->priv) )

/* Function declarations */
int dmg_open(int fd, bdev_desc_t *bdev);
int dmg_read(bdev_desc_t *bdev, u8 *buf, int nb_bytes);
void dmg_close(bdev_desc_t *bdev);
int dmg_seek(bdev_desc_t *bdev, long block, long offset);

#endif
