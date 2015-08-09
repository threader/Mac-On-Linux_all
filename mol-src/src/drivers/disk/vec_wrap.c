/* Vector wrapper for regular read functions in other implementations */

#include "vec_wrap.h"

/* Read */
int wrap_read(bdev_desc_t *bdev, struct iovec *vec, int count)
{
	int i;
	u8 *buf, *buf_p;
	ssize_t total_bytes;	/* Total bytes requested */

	/* Find the toal bytes we need to read in */
	total_bytes = 0;
	for(i=0; i<count; i++)
	{
		if(total_bytes + vec[i].iov_len < SSIZE_MAX)
			total_bytes += vec[i].iov_len;
		else
			return -EINVAL;
	}

	/* Allocate memory for the read */
	buf_p = buf = malloc(total_bytes);
	if(!buf)
		return -EINVAL;

	/* Do the read */
	if(bdev->real_read(bdev, buf, total_bytes) != total_bytes) 
		return -EINVAL;

	/* Fill the vectors */
	for(i=0; i<count; i++) {
		memcpy(vec[i].iov_base, buf_p, vec[i].iov_len);
		buf_p += vec[i].iov_len;
	}

	/* Free the memory */
	free(buf);
	return total_bytes;

}

/* Write */
int wrap_write(bdev_desc_t *bdev, struct iovec *vec, int count)
{
	int i;
	u8 *buf, *buf_p;
	ssize_t total_bytes;

	/* Find the toal bytes we need to write out */
	total_bytes = 0;
	for(i=0; i<count; i++)
	{
		if(total_bytes + vec[i].iov_len < SSIZE_MAX)
			total_bytes += vec[i].iov_len;
		else
			return -EINVAL;
	}
	
	/* Allocate memory for the write */
	buf_p = buf = malloc(total_bytes);
	
	/* Empty the vectors */
	for(i=0; i<count; i++) {
		memcpy(buf_p, vec[i].iov_base, vec[i].iov_len);
		buf_p += vec[i].iov_len;
	}

	/* Do the write */
	if(bdev->real_write(bdev, buf, total_bytes) != total_bytes) {
		return -EINVAL;
	}
	
	/* Free the memory */
	free(buf);
	return total_bytes;

}
