// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <stdio.h>

#include "drivers/storage/blockdev.h"
#include "drivers/storage/slice.h"
#include "tests/test.h"

#define TEST_STORAGE_SIZE 256
#define TEST_STORAGE_BLOCK_SIZE 16
#define SENTINEL 0x5a

static BlockDev test_parent_dev;
static char test_storage[TEST_STORAGE_SIZE];

static lba_t test_read(struct BlockDevOps *me, lba_t start, lba_t count, void *buffer)
{
	BlockDev *blockdev = (BlockDev *)me;

	assert_ptr_equal(blockdev, &test_parent_dev);
	assert_true(start <= blockdev->block_count);
	assert_true(count <= blockdev->block_count - start);

	memcpy(buffer, test_storage + blockdev->block_size * start,
	       blockdev->block_size * count);

	return count;
}

static int setup(void **state)
{
	test_parent_dev.name = "parent";
	test_parent_dev.block_size = TEST_STORAGE_BLOCK_SIZE;
	test_parent_dev.block_count = TEST_STORAGE_SIZE / TEST_STORAGE_BLOCK_SIZE;
	test_parent_dev.stream_block_count = TEST_STORAGE_SIZE / TEST_STORAGE_BLOCK_SIZE;
	test_parent_dev.ops.read = &test_read;
	test_parent_dev.removable = 1;

	for (int i = 0; i < TEST_STORAGE_SIZE; i++)
		test_storage[i] = i & 0xff;

	return 0;
}

static void test_slice_creation(void **state)
{
	BlockDev *slice;

	/* Valid slice */
	slice = new_blockdev_slice(&test_parent_dev, 2, 4);
	assert_non_null(slice);
	assert_int_equal(slice->block_size, test_parent_dev.block_size);
	assert_int_equal(slice->block_count, 4);
	assert_int_equal(slice->removable, test_parent_dev.removable);
	free(slice);

	/* Out of bounds slice: offset > parent->block_count */
	slice = new_blockdev_slice(&test_parent_dev, test_parent_dev.block_count + 1, 1);
	assert_null(slice);

	/* Out of bounds slice: size > parent->block_count - offset */
	slice = new_blockdev_slice(&test_parent_dev, 2, test_parent_dev.block_count - 1);
	assert_null(slice);

	/* Max valid slice */
	slice = new_blockdev_slice(&test_parent_dev, 0, test_parent_dev.block_count);
	assert_non_null(slice);
	free(slice);
}

static void test_slice_read(void **state)
{
	BlockDev *slice = new_blockdev_slice(&test_parent_dev, 2, 4);
	char buf[TEST_STORAGE_BLOCK_SIZE * 2];
	lba_t ret;

	assert_non_null(slice);

	/* Valid read: 2 blocks from start of slice (which is offset 2 of parent) */
	memset(buf, SENTINEL, sizeof(buf));
	ret = slice->ops.read(&slice->ops, 0, 2, buf);
	assert_int_equal(ret, 2);
	assert_memory_equal(buf, test_storage + TEST_STORAGE_BLOCK_SIZE * 2,
			    TEST_STORAGE_BLOCK_SIZE * 2);

	/* Valid read: 1 block from offset 1 of slice (which is offset 3 of parent) */
	memset(buf, SENTINEL, sizeof(buf));
	ret = slice->ops.read(&slice->ops, 1, 1, buf);
	assert_int_equal(ret, 1);
	assert_memory_equal(buf, test_storage + TEST_STORAGE_BLOCK_SIZE * 3,
			    TEST_STORAGE_BLOCK_SIZE);

	/* Out of bounds read: start > slice->block_count */
	ret = slice->ops.read(&slice->ops, 5, 1, buf);
	assert_int_equal(ret, 0);

	/* Out of bounds read: count > slice->block_count - start */
	ret = slice->ops.read(&slice->ops, 3, 2, buf);
	assert_int_equal(ret, 0);

	/* Overflow check: start + count would overflow if not careful */
	ret = slice->ops.read(&slice->ops, (lba_t)-1, 2, buf);
	assert_int_equal(ret, 0);

	free(slice);
}

#define TEST(test_function_name)  \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST(test_slice_creation),
		TEST(test_slice_read),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
