/* 
 * <blk-qcow.c>
 *	
 * qemu block device read/write functions
 *   
 * Copyright (C) 2005 Joseph Jezak
 *
 * Based on the QEMU Block driver for the QCOW format
 * 
 * Copyright (c) 2004 Fabrice Bellard
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

#include "blk_qcow.h"

static int decompress_cluster(BDRVQCowState *s, u64 cluster_offset);

int qcow_open(int fd, bdev_desc_t *bdev) 
{
    int len, i, shift;
    QCowHeader header;
    BDRVQCowState *s = NULL;
    // int len;
    
    bdev->priv = malloc( sizeof (BDRVQCowState) );
    if(bdev->priv == NULL)
	    goto fail;
    s = QCOW_PRIV(bdev);
    CLEAR( *s );

    /* Init the bdev struct */
    bdev->read = &wrap_read;
    bdev->write = &wrap_write;
    bdev->real_read = &qcow_read;
    bdev->real_write = &qcow_write;
    bdev->seek = &qcow_seek;
    bdev->close = &qcow_close;
    bdev->fd = fd;
    	
    s->fd = fd;
    s->backing_hd = -1;
    
    lseek(s->fd, 0, SEEK_SET);
    if (read(s->fd, &header, sizeof(header)) != sizeof(header))
        goto fail;

    header.magic = be32_to_cpu(header.magic);
    header.version = be32_to_cpu(header.version);
    header.backing_file_offset = be64_to_cpu(header.backing_file_offset);
    header.backing_file_size = be32_to_cpu(header.backing_file_size);
    header.mtime = be32_to_cpu(header.mtime);
    header.size = be64_to_cpu(header.size);
    header.crypt_method = be32_to_cpu(header.crypt_method);
    header.l1_table_offset = be64_to_cpu(header.l1_table_offset);

    if (header.magic != QCOW_MAGIC || header.version != QCOW_VERSION)
        goto fail;
    if (header.size <= 1 || header.cluster_bits < 9)
        goto fail;
    if (header.crypt_method > QCOW_CRYPT_AES)
        goto fail;
    s->crypt_method_header = header.crypt_method;
    if (s->crypt_method_header)
        bdev->flags &= BF_ENCRYPTED;
    s->cluster_bits = header.cluster_bits;
    s->cluster_size = 1 << s->cluster_bits;
    s->cluster_sectors = 1 << (s->cluster_bits - 9);
    s->l2_bits = header.l2_bits;
    s->l2_size = 1 << s->l2_bits;
    bdev->size = header.size; 
    s->cluster_offset_mask = (1LL << (63 - s->cluster_bits)) - 1;

    /* read the level 1 table */
    shift = s->cluster_bits + s->l2_bits;
    s->l1_size = (header.size + (1LL << shift) - 1) >> shift;
    s->l1_table_offset = header.l1_table_offset;
    s->l1_table = malloc(s->l1_size * sizeof(u64));
    if (!s->l1_table)
        goto fail;
    lseek(s->fd, s->l1_table_offset, SEEK_SET);
    if (read(s->fd, s->l1_table, s->l1_size * sizeof(u64)) != 
        s->l1_size * sizeof(u64))
        goto fail;
    for(i = 0;i < s->l1_size; i++) {
	    s->l1_table[i] = be64_to_cpu(s->l1_table[i]);
    }

    /* alloc L2 cache */
    s->l2_cache = malloc(s->l2_size * QCOW_L2_CACHE_SIZE * sizeof(u64));
    if (!s->l2_cache)
        goto fail;
    s->cluster_cache = malloc(s->cluster_size);
    if (!s->cluster_cache)
        goto fail;
    s->cluster_data = malloc(s->cluster_size);
    if (!s->cluster_data)
    s->cluster_cache_offset = -1;
    if (header.backing_file_offset != 0) {
        int not_used;
        
        len = header.backing_file_size;
        if (len > 1023) {
    	    printm("backing filename too long!\n");
    	    goto fail;
        }
        lseek(s->fd, header.backing_file_offset, SEEK_SET);
        if (read(s->fd, s->backing_file, len) != len)
            goto fail;
        s->backing_file[len] = '\0';
        if (s->backing_file[0] != '/') {
    	  printm("Sorry only absolute filename for backing file!\n");
    	  goto fail;
        }
	/* FIXME it will leak backing_hd on error */
        if( (s->backing_hd = disk_open(s->backing_file, 0, &not_used, 0)) < 0 ) {
    	  printm("Can't open backing file!\n");
    	  goto fail;
        }
    }
    return 0;

 fail:
    if(s->l1_table)
    	free(s->l1_table);
    if(s->l2_cache)
	free(s->l2_cache);
    if(s->cluster_cache)
	free(s->cluster_cache);
    if(s->cluster_data) 
	free(s->cluster_data);
#if 0
    // close is done by caller
    close(s->fd);
#endif    
    return -1;
}

/* The crypt function is compatible with the linux cryptoloop
   algorithm for < 4 GB images. NOTE: out_buf == in_buf is
   supported */
static void encrypt_sectors(BDRVQCowState *s, u64 sector_num,
                            u8 *out_buf, const u8 *in_buf,
                            int nb_sectors, int enc,
                            const AES_KEY *key)
{
    union {
        u64 ll[2];
        u8 b[16];
    } ivec;
    int i;

    for(i = 0; i < nb_sectors; i++) {
        ivec.ll[0] = cpu_to_le64(sector_num);
        ivec.ll[1] = 0;
        AES_cbc_encrypt(in_buf, out_buf, 512, key, 
                        ivec.b, enc);
        sector_num++;
        in_buf += 512;
        out_buf += 512;
    }
}

/* 
 * offset is in bytes
 * 
 * 'allocate' is:
 *
 * 0 to not allocate.
 *
 * 1 to allocate a normal cluster (for sector indexes 'n_start' to
 * 'n_end')
 *
 * 2 to allocate a compressed cluster of size
 * 'compressed_size'. 'compressed_size' must be > 0 and <
 * cluster_size 
 *
 * return 0 if not allocated.
 */
static u64 get_cluster_offset(BDRVQCowState *s, 
                                   u64 offset, int allocate,
                                   int compressed_size,
                                   int n_start, int n_end)
{
    int min_index, i, j, l1_index, l2_index;
    u64 l2_offset, *l2_table, cluster_offset, tmp;
    u32 min_count;
    int new_l2_table;

    /* Find the L1 Table Offset */
    l1_index = offset >> (s->l2_bits + s->cluster_bits);
    l2_offset = s->l1_table[l1_index];
    new_l2_table = 0;
    if (!l2_offset) {
        if (!allocate) 
            return 0;
        /* allocate a new l2 entry */
        l2_offset = lseek(s->fd, 0, SEEK_END);
        /* round to cluster size */
        l2_offset = (l2_offset + s->cluster_size - 1) & ~(s->cluster_size - 1);
        /* update the L1 entry */
        s->l1_table[l1_index] = l2_offset;
        tmp = cpu_to_be64(l2_offset);
        lseek(s->fd, s->l1_table_offset + l1_index * sizeof(tmp), SEEK_SET);
        if (write(s->fd, &tmp, sizeof(tmp)) != sizeof(tmp)) 
            return 0;
        new_l2_table = 1;
    }
    for(i = 0; i < QCOW_L2_CACHE_SIZE; i++) {
        if (l2_offset == s->l2_cache_offsets[i]) {
            /* increment the hit count */
            if (++s->l2_cache_counts[i] == 0xffffffff) {
                for(j = 0; j < QCOW_L2_CACHE_SIZE; j++) {
                    s->l2_cache_counts[j] >>= 1;
                }
            }
            l2_table = s->l2_cache + (i << s->l2_bits);
            goto found;
        }
    }
    /* not found: load a new entry in the least used one */
    min_index = 0;
    min_count = 0xffffffff;
    for(i = 0; i < QCOW_L2_CACHE_SIZE; i++) {
        if (s->l2_cache_counts[i] < min_count) {
            min_count = s->l2_cache_counts[i];
            min_index = i;
        }
    }
    l2_table = s->l2_cache + (min_index << s->l2_bits);
    lseek(s->fd, l2_offset, SEEK_SET);
    if (new_l2_table) {
        memset(l2_table, 0, s->l2_size * sizeof(u64));
        if (write(s->fd, l2_table, s->l2_size * sizeof(u64)) !=
            s->l2_size * sizeof(u64)) 
            return 0;
    } else {
        if (read(s->fd, l2_table, s->l2_size * sizeof(u64)) !=
            s->l2_size * sizeof(u64)) 
            return 0;
    }
    s->l2_cache_offsets[min_index] = l2_offset;
    s->l2_cache_counts[min_index] = 1;
 found:
    l2_index = (offset >> s->cluster_bits) & (s->l2_size - 1);
    cluster_offset = be64_to_cpu(l2_table[l2_index]);
    if (!cluster_offset ||
        ((cluster_offset & QCOW_OFLAG_COMPRESSED) && allocate == 1)) {
        if (!allocate) 
            return 0;
        /* allocate a new cluster */
        if ((cluster_offset & QCOW_OFLAG_COMPRESSED) &&
            (n_end - n_start) < s->cluster_sectors) {
            /* if the cluster is already compressed, we must
               decompress it in the case it is not completely
               overwritten */
            if (decompress_cluster(s, cluster_offset) < 0)
                return 0;
            cluster_offset = lseek(s->fd, 0, SEEK_END);
            cluster_offset = (cluster_offset + s->cluster_size - 1) &
                ~(s->cluster_size - 1);
            /* write the cluster content */
            lseek(s->fd, cluster_offset, SEEK_SET);
            if (write(s->fd, s->cluster_cache, s->cluster_size) !=
                s->cluster_size) 
                return -1;
        } else {
            cluster_offset = lseek(s->fd, 0, SEEK_END);
            if (allocate == 1) {
                /* round to cluster size */
                cluster_offset = (cluster_offset + s->cluster_size - 1) &
                    ~(s->cluster_size - 1);
		if(ftruncate(s->fd, cluster_offset + s->cluster_size)) 
			return -1;

                /* if encrypted, we must initialize the cluster
                   content which won't be written */
                if (s->crypt_method &&
                    (n_end - n_start) < s->cluster_sectors) {
                    u64 start_sect;
                    start_sect = (offset & ~(s->cluster_size - 1)) >> 9;
                    memset(s->cluster_data + 512, 0xaa, 512);
                    for(i = 0; i < s->cluster_sectors; i++) {
                        if (i < n_start || i >= n_end) {
                            encrypt_sectors(s, start_sect + i,
                                            s->cluster_data,
                                            s->cluster_data + 512, 1, 1,
                                            &s->aes_encrypt_key);
                            lseek(s->fd, cluster_offset + i * 512, SEEK_SET);
                            if (write(s->fd, s->cluster_data, 512) != 512)
                                return -1;
                        }
                    }
                }
            } else {
                cluster_offset |= QCOW_OFLAG_COMPRESSED |
                    (u64)compressed_size << (63 - s->cluster_bits);
            }
        }
        /* update L2 table */
        tmp = cpu_to_be64(cluster_offset);
        l2_table[l2_index] = tmp;
        lseek(s->fd, l2_offset + l2_index * sizeof(tmp), SEEK_SET);
        if (write(s->fd, &tmp, sizeof(tmp)) != sizeof(tmp))
            return 0;
    }
    return cluster_offset;    
}

static int decompress_buffer(u8 *out_buf, int out_buf_size,
                             const u8 *buf, int buf_size)
{
    z_stream strm1, *strm = &strm1;
    int ret, out_len;

    memset(strm, 0, sizeof(*strm));

    strm->next_in = (u8 *)buf;
    strm->avail_in = buf_size;
    strm->next_out = out_buf;
    strm->avail_out = out_buf_size;

    ret = inflateInit2(strm, -12);
    if (ret != Z_OK)
        return -1;
    ret = inflate(strm, Z_FINISH);
    out_len = strm->next_out - out_buf;
    if ((ret != Z_STREAM_END && ret != Z_BUF_ERROR) ||
        out_len != out_buf_size) {
        inflateEnd(strm);
        return -1;
    }
    inflateEnd(strm);
    return 0;
}
                              
static int decompress_cluster(BDRVQCowState *s, u64 cluster_offset)
{
    int ret, csize;
    u64 coffset;

    coffset = cluster_offset & s->cluster_offset_mask;
    if (s->cluster_cache_offset != coffset) {
        csize = cluster_offset >> (63 - s->cluster_bits);
        csize &= (s->cluster_size - 1);
        lseek(s->fd, coffset, SEEK_SET);
        ret = read(s->fd, s->cluster_data, csize);
        if (ret != csize) 
            return -1;
        if (decompress_buffer(s->cluster_cache, s->cluster_size,
                              s->cluster_data, csize) < 0) {
            return -1;
        }
        s->cluster_cache_offset = coffset;
    }
    return 0;
}

/* nb_bytes is the number of bytes to read in */
int qcow_read(bdev_desc_t *bdev, u8 * buf, int nb_bytes)
{
    BDRVQCowState *s = QCOW_PRIV(bdev);
    u64 sector_num = s->seek_sector;
    int ret, index_in_cluster, n;
    u64 cluster_offset;
    int nb_sectors = ceil(nb_bytes / 512ULL);
    u8 * buf_start = buf;
    
    while (nb_sectors > 0) {
        cluster_offset = get_cluster_offset(s, sector_num << 9, 0, 0, 0, 0);
        index_in_cluster = sector_num & (s->cluster_sectors - 1);
        n = s->cluster_sectors - index_in_cluster;
        if (n > nb_sectors)
            n = nb_sectors;
        if (!cluster_offset) {
            if (s->backing_hd) {
                /* read from the base image */
  	        if (pread(s->backing_hd, buf, n * 512, sector_num *512) < 0)
                    return -1;
            } else {
                memset(buf, 0, 512 * n);
            }
 
        } else if (cluster_offset & QCOW_OFLAG_COMPRESSED) {
            if (decompress_cluster(s, cluster_offset) < 0)
                return -1;
            memcpy(buf, s->cluster_cache + index_in_cluster * 512, 512 * n);
        } else {
            lseek(s->fd, cluster_offset + index_in_cluster * 512, SEEK_SET);
            ret = read(s->fd, buf, n * 512);
            if (ret != n * 512) 
                return -1;
            if (s->crypt_method) {
                encrypt_sectors(s, sector_num, buf, buf, n, 0,
                                &s->aes_decrypt_key);
            }
        }
        nb_sectors -= n;
        sector_num += n;
        buf += n * 512;
    }
    s->seek_sector = sector_num;
    return buf - buf_start;
}

int qcow_write(bdev_desc_t *bdev, u8 * buf, int nb_bytes)
{
    BDRVQCowState *s = QCOW_PRIV(bdev);
    int ret, index_in_cluster, n;
    u8 * buf_start = buf;
    u64 cluster_offset;
    u64 sector_num = s->seek_sector;
    int nb_sectors = ceil(nb_bytes / 512);
    
    while (nb_sectors > 0) {
        index_in_cluster = sector_num & (s->cluster_sectors - 1);
        n = s->cluster_sectors - index_in_cluster;
        if (n > nb_sectors)
            n = nb_sectors;
        cluster_offset = get_cluster_offset(s, sector_num << 9, 1, 0,
                                            index_in_cluster,
                                            index_in_cluster + n);
        if (!cluster_offset)
            return -1;
        lseek(s->fd, cluster_offset + index_in_cluster * 512, SEEK_SET);
        if (s->crypt_method) {
            encrypt_sectors(s, sector_num, s->cluster_data, buf, n, 1,
                            &s->aes_encrypt_key);
            ret = write(s->fd, s->cluster_data, n * 512);
        } else {
            ret = write(s->fd, buf, n * 512);
        }
        if (ret != n * 512)
            return -1;
        nb_sectors -= n;
        sector_num += n;
        buf += n * 512;
    }
    s->seek_sector = sector_num;
    s->cluster_cache_offset = -1; /* disable compressed cache */
    return buf - buf_start;
}

int qcow_seek(bdev_desc_t *bdev, long block, long offset){
    QCOW_PRIV(bdev)->seek_sector = (u64) (block + offset);
    return 0;
}

void qcow_close(bdev_desc_t *bdev)
{
    BDRVQCowState *s = QCOW_PRIV(bdev);
    fsync(s->fd);
    free(s->l1_table);
    free(s->l2_cache);
    free(s->cluster_cache);
    free(s->cluster_data);
    if (s->backing_hd != -1)
        close(s->backing_hd);
    free(s);
}
