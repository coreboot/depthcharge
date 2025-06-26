/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _MOCKS_TEST_BLOCKDEV_H
#define _MOCKS_TEST_BLOCKDEV_H

#include <commonlib/list.h>

BlockDev *new_test_blockdev(char *storage, unsigned int storage_size,
			    unsigned int block_size);

void free_test_blockdev(BlockDev *bdev);

#endif /* _MOCKS_TEST_BLOCKDEV_H */
