/* Read / Write Vector Wrappers */

#include "mol_config.h"
#include <limits.h>
#include <sys/uio.h>
#include "disk.h"

int wrap_read(bdev_desc_t *bdev, struct iovec *vec, int count);
int wrap_write(bdev_desc_t *bdev, struct iovec *vec, int count);
