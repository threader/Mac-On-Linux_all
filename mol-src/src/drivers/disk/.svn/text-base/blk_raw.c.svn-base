/* 
 *   <blk-dmg.c>
 *	
 *   Raw block device read/write functions
 *   Note: Read/Write are vector based!
 *   Copyright (C) 2006 Joseph Jezak
 *   
 */   

#include "blk_raw.h"

int raw_open(int fd, bdev_desc_t *bdev) {
	/* Init the bdev struct */
	bdev->read = &raw_read;
	bdev->write = &raw_write;
	bdev->seek = &raw_seek;
	bdev->size = get_file_size_in_blks(fd) * 512ULL;
	bdev->close = NULL;	/* Nothing needed */
	bdev->fd = fd;

	/* Do what else here? */
	return 0;
}

/* Block to seek to */
int raw_seek(bdev_desc_t *bdev, long block, long offset) {
	return blk_lseek(bdev->fd, block, offset);
}

/* Read "count" blocks */
int raw_read(bdev_desc_t *bdev, struct iovec * vec, int count) {
	return readv(bdev->fd, vec, count);
}

/* Write "count" blocks */
int raw_write(bdev_desc_t *bdev, struct iovec * vec, int count) {
	return writev(bdev->fd, vec, count);
}
