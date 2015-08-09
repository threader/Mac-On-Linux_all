/* 
 *   <blk-dmg.c>
 *	
 *   Apple dmg block device read/write functions
 *   Copyright (C) 2005 Joseph Jezak
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
#include "blk_dmg.h"

/* Read 64 bit value */
static off_t read_off(int fd)
{
	u64 buffer;
	if(read(fd,&buffer,8)<8)
		return 0;
	return be64_to_cpu(buffer);
}

/* Read 32 bit value */
static off_t read_uint32(int fd)
{
	u32 buffer;
	if(read(fd,&buffer,4)<4)
		return 0;
	return be32_to_cpu(buffer);
}

/* Open dmg image */
int dmg_open(int fd, bdev_desc_t *bdev)
{
    u32 count;
    u32 max_compressed_size=1,max_sectors_per_chunk=1,i;
    bdev->priv = malloc(sizeof(BDRVDMGState));
    if(bdev->priv == NULL)
        goto fail;
    BDRVDMGState *s = DMG_PRIV(bdev);
    CLEAR(*s);

    off_t info_begin,info_end,last_in_offset,last_out_offset;

    /* Init the bdev struct */
    bdev->fd = fd;
    bdev->read = &wrap_read;
    bdev->real_read = &dmg_read;
    bdev->write = NULL;
    bdev->seek = &dmg_seek;
    bdev->close = &dmg_close;

    s->fd = fd;
    /* RO */
    bdev->flags &= ~BF_ENABLE_WRITE;
    s->n_chunks = 0;
    s->offsets = s->lengths = s->sectors = s->sectorcounts = 0;

    /* read offset of info blocks */
    if(lseek(s->fd,-0x1d8,SEEK_END)<0)
	goto fail;
    info_begin=read_off(s->fd);
    if(info_begin==0)
	goto fail;
    if(lseek(s->fd,info_begin,SEEK_SET)<0)
	goto fail;
    if(read_uint32(s->fd)!=0x100)
	goto fail;
    if((count = read_uint32(s->fd))==0)
	goto fail;
    info_end = info_begin+count;
    if(lseek(s->fd,0xf8,SEEK_CUR)<0)
	goto fail;

    /* read offsets */
    last_in_offset = last_out_offset = 0;
    while(lseek(s->fd,0,SEEK_CUR)<info_end) {
        u32 type;

	count = read_uint32(s->fd);
	if(count==0)
		goto fail;
	type = read_uint32(s->fd);
	if(type!=0x6d697368 || count<244)
	    lseek(s->fd,count-4,SEEK_CUR);
	else {
	    int new_size, chunk_count;
	    if(lseek(s->fd,200,SEEK_CUR)<0)
		goto fail;
	    chunk_count = (count-204)/40;
	    new_size = sizeof(u64) * (s->n_chunks + chunk_count);
	    s->types = realloc(s->types, new_size/2);
	    s->offsets = realloc(s->offsets, new_size);
	    s->lengths = realloc(s->lengths, new_size);
	    s->sectors = realloc(s->sectors, new_size);
	    s->sectorcounts = realloc(s->sectorcounts, new_size);

	    for(i=s->n_chunks;i<s->n_chunks+chunk_count;i++) {
		s->types[i] = read_uint32(s->fd);
		if(s->types[i]!=0x80000005 && s->types[i]!=1 && s->types[i]!=2) {
		    if(s->types[i]==0xffffffff) {
			last_in_offset = s->offsets[i-1]+s->lengths[i-1];
			last_out_offset = s->sectors[i-1]+s->sectorcounts[i-1];
		    }
		    chunk_count--;
		    i--;
		    if(lseek(s->fd,36,SEEK_CUR)<0)
			goto fail;
		    continue;
		}
		read_uint32(s->fd);
		s->sectors[i] = last_out_offset+read_off(s->fd);
		s->sectorcounts[i] = read_off(s->fd);
		s->offsets[i] = last_in_offset+read_off(s->fd);
		s->lengths[i] = read_off(s->fd);
		if(s->lengths[i]>max_compressed_size)
		    max_compressed_size = s->lengths[i];
		if(s->sectorcounts[i]>max_sectors_per_chunk)
		    max_sectors_per_chunk = s->sectorcounts[i];
	    }
	    s->n_chunks+=chunk_count;
	}
    }
    bdev->size = s->n_chunks * 512;

    /* initialize zlib engine */
    if(!(s->compressed_chunk=(char*)malloc(max_compressed_size+1)))
	goto fail;
    if(!(s->uncompressed_chunk=(char*)malloc(512*max_sectors_per_chunk)))
	goto fail;
    if(inflateInit(&s->zstream) != Z_OK)
	goto fail;

    s->current_chunk = s->n_chunks;
    
    return 0;

fail:
    return -1;
    
}

static inline int is_sector_in_chunk(BDRVDMGState* s,
		u32 chunk_num,int sector_num)
{
    if(chunk_num>=s->n_chunks || s->sectors[chunk_num]>sector_num ||
	    s->sectors[chunk_num]+s->sectorcounts[chunk_num]<=sector_num)
	return 0;
    else
	return -1;
}

static inline u32 search_chunk(BDRVDMGState* s,int sector_num)
{
    /* binary search */
    u32 chunk1=0,chunk2=s->n_chunks,chunk3;
    while(chunk1!=chunk2) {
	chunk3 = (chunk1+chunk2)/2;
	if(s->sectors[chunk3]>sector_num)
	    chunk2 = chunk3;
	else if(s->sectors[chunk3]+s->sectorcounts[chunk3]>sector_num)
	    return chunk3;
	else
	    chunk1 = chunk3;
    }
    return s->n_chunks; /* error */
}

static inline int dmg_read_chunk(BDRVDMGState *s, int sector_num)
{
    if(!is_sector_in_chunk(s, s->current_chunk, sector_num)) {
	int ret;
	u32 chunk = search_chunk(s,sector_num);

	if(chunk>=s->n_chunks)
	    return -1;

	s->current_chunk = s->n_chunks;
	switch(s->types[chunk]) {
	case 0x80000005: { /* zlib compressed */
	    int i;

	    ret = lseek(s->fd, s->offsets[chunk], SEEK_SET);
	    if(ret<0)
		return -1;

	    /* we need to buffer, because only the chunk as whole can be
	     * inflated. */
	    i=0;
	    do {
		ret = read(s->fd, s->compressed_chunk+i, s->lengths[chunk]-i);
		if(ret<0 && errno==EINTR)
		    ret=0;
		i+=ret;
	    } while(ret>=0 && ret+i<s->lengths[chunk]);

	    if (ret != s->lengths[chunk])
		return -1;
	
	    s->zstream.next_in = (unsigned char *) (s->compressed_chunk);
	    s->zstream.avail_in = s->lengths[chunk];
	    s->zstream.next_out = (unsigned char *) (s->uncompressed_chunk);
	    s->zstream.avail_out = 512*s->sectorcounts[chunk];
	    ret = inflateReset(&s->zstream);
	    if(ret != Z_OK)
		return -1;
	    ret = inflate(&s->zstream, Z_FINISH);
	    if(ret != Z_STREAM_END || s->zstream.total_out != 512*s->sectorcounts[chunk])
		return -1;
	    break; }
	case 1: /* copy */
	    ret = read(s->fd, s->uncompressed_chunk, s->lengths[chunk]);
	    if (ret != s->lengths[chunk])
		return -1;
	    break;
	case 2: /* zero */
	    memset(s->uncompressed_chunk, 0, 512*s->sectorcounts[chunk]);
	    break;
	}
	s->current_chunk = chunk;
    }
    return 0;
}

int dmg_read(bdev_desc_t *bdev, u8 *buf, int nb_bytes)
{
    BDRVDMGState *s = DMG_PRIV(bdev);
    int i;
    int nb_sectors = ceil(nb_bytes / 512);

    for(i=0;i<nb_sectors;i++) {
	u32 sector_offset_in_chunk;
	if(dmg_read_chunk(s, s->seek_sector+i) != 0)
	    return -1;
	sector_offset_in_chunk = s->seek_sector+i-s->sectors[s->current_chunk];
	memcpy(buf+i*512, s->uncompressed_chunk+sector_offset_in_chunk*512, 512);
    }
    return nb_bytes;
}

int dmg_seek(bdev_desc_t *bdev, long seek_block, long offset){
    DMG_PRIV(bdev)->seek_sector = (seek_block + offset)/512ULL;
    return 0;
}

void dmg_close(bdev_desc_t *bdev)
{
    BDRVDMGState *s = DMG_PRIV(bdev);
    if(s->n_chunks>0) {
	free(s->types);
	free(s->offsets);
	free(s->lengths);
	free(s->sectors);
	free(s->sectorcounts);
    }
    free(s->compressed_chunk);
    free(s->uncompressed_chunk);
    inflateEnd(&s->zstream);
    free(s);
}
