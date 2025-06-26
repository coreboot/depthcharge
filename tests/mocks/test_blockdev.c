// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "drivers/storage/blockdev.h"
#include "tests/test.h"

struct test_blockdev {
	BlockDev bdev;
	char *storage;
};

static lba_t test_read(struct BlockDevOps *me, lba_t start, lba_t count, void *buffer)
{
	BlockDev *blockdev = container_of(me, BlockDev, ops);
	struct test_blockdev *tbdev = container_of(blockdev, struct test_blockdev, bdev);

	assert_in_range(start, 0, blockdev->block_count);
	assert_in_range(start + count, 0, blockdev->block_count);

	memcpy(buffer, tbdev->storage + blockdev->block_size * start,
	       blockdev->block_size * count);

	return count;
}

static lba_t test_write(struct BlockDevOps *me, lba_t start, lba_t count, const void *buffer)
{
	BlockDev *blockdev = container_of(me, BlockDev, ops);
	struct test_blockdev *tbdev = container_of(blockdev, struct test_blockdev, bdev);

	assert_in_range(start, 0, blockdev->block_count);
	assert_in_range(start + count, 0, blockdev->block_count);

	memcpy(tbdev->storage + blockdev->block_size * start, buffer,
	       blockdev->block_size * count);

	return count;
}

static lba_t test_erase(struct BlockDevOps *me, lba_t start, lba_t count)
{
	BlockDev *blockdev = container_of(me, BlockDev, ops);
	struct test_blockdev *tbdev = container_of(blockdev, struct test_blockdev, bdev);

	assert_in_range(start, 0, blockdev->block_count);
	assert_in_range(start + count, 0, blockdev->block_count);

	memset(tbdev->storage + blockdev->block_size * start, 0, blockdev->block_size * count);

	return count;
}

BlockDev *new_test_blockdev(char *storage, unsigned int storage_size, unsigned int block_size)
{
	struct test_blockdev *tbdev = malloc(sizeof(*tbdev));

	if (tbdev == NULL)
		fail_msg("Failed to allocate test_blockdev");

	tbdev->storage = storage;
	tbdev->bdev.block_size = block_size;
	tbdev->bdev.block_count = storage_size / block_size;
	tbdev->bdev.stream_block_count = storage_size / block_size;
	tbdev->bdev.ops.read = &test_read;
	tbdev->bdev.ops.write = &test_write;
	tbdev->bdev.ops.erase = &test_erase;
	tbdev->bdev.ops.new_stream = &new_simple_stream;

	return &tbdev->bdev;
}

void free_test_blockdev(BlockDev *bdev)
{
	struct test_blockdev *tbdev = container_of(bdev, struct test_blockdev, bdev);

	free(tbdev);
}
