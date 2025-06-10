// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "drivers/storage/blockdev.h"
#include "tests/test.h"

#define TEST_STORAGE_SIZE 256
#define TEST_STORAGE_BLOCK_SIZE 16
#define SENTINEL 0x5a

static BlockDev test_block_dev;
static char test_storage[TEST_STORAGE_SIZE];

static lba_t test_read(struct BlockDevOps *me, lba_t start, lba_t count, void *buffer)
{
	BlockDev *blockdev = (BlockDev *)me;

	assert_ptr_equal(blockdev, &test_block_dev);
	assert_in_range(start, 0, blockdev->block_count);
	assert_in_range(start + count, 0, blockdev->block_count);

	memcpy(buffer, test_storage + blockdev->block_size * start,
	       blockdev->block_size *count);

	return count;
}

static int setup(void **state)
{
	test_block_dev.block_size = TEST_STORAGE_BLOCK_SIZE;
	test_block_dev.block_count = TEST_STORAGE_SIZE / TEST_STORAGE_BLOCK_SIZE;
	test_block_dev.stream_block_count = TEST_STORAGE_SIZE / TEST_STORAGE_BLOCK_SIZE;
	test_block_dev.ops.read = &test_read;
	test_block_dev.ops.new_stream = &new_simple_stream;

	for (int i = 0; i < TEST_STORAGE_SIZE; i++)
		test_storage[i] = i & 0xff;

	return 0;
}

static void test_simple_stream_read_block_aligned(void **state)
{
	struct {
		char before[TEST_STORAGE_BLOCK_SIZE];
		char used[TEST_STORAGE_BLOCK_SIZE * 3];
		char after[TEST_STORAGE_BLOCK_SIZE];
	} buf;
	uint64_t ret;
	StreamOps *stream = test_block_dev.ops.new_stream(&test_block_dev.ops, 2, 3);

	memset(&buf, SENTINEL, sizeof(buf));

	ret = stream->read(stream, sizeof(buf.used), buf.used);
	stream->close(stream);

	assert_int_equal(ret, sizeof(buf.used));
	assert_memory_equal(buf.used, test_storage + TEST_STORAGE_BLOCK_SIZE * 2,
			    sizeof(buf.used));
	assert_filled_with(buf.before, SENTINEL, sizeof(buf.before));
	assert_filled_with(buf.after, SENTINEL, sizeof(buf.after));
}

static void test_simple_stream_read_end_not_aligned(void **state)
{
	struct {
		char before[TEST_STORAGE_BLOCK_SIZE];
		char used[9];
		char after[TEST_STORAGE_BLOCK_SIZE];
	} buf;
	uint64_t ret;
	StreamOps *stream = test_block_dev.ops.new_stream(&test_block_dev.ops, 2, 1);

	memset(&buf, SENTINEL, sizeof(buf));

	ret = stream->read(stream, sizeof(buf.used), buf.used);
	stream->close(stream);

	assert_int_equal(ret, sizeof(buf.used));
	assert_memory_equal(buf.used, test_storage + TEST_STORAGE_BLOCK_SIZE * 2,
			    sizeof(buf.used));
	assert_filled_with(buf.before, SENTINEL, sizeof(buf.before));
	assert_filled_with(buf.after, SENTINEL, sizeof(buf.after));
}

/*
 * Cmocka defines "skip" which is unfortunate, because one of StreamOps methods is "skip".
 * Undef "skip" to make possible calling this method from StreamOps.
 */
#undef skip

static void test_simple_stream_read_beginning_not_aligned(void **state)
{
	struct {
		char before[TEST_STORAGE_BLOCK_SIZE];
		char used[9];
		char after[TEST_STORAGE_BLOCK_SIZE];
	} buf;
	const int skip_bytes = 3;
	uint64_t ret_skip, ret;
	StreamOps *stream = test_block_dev.ops.new_stream(&test_block_dev.ops, 2, 1);

	memset(&buf, SENTINEL, sizeof(buf));

	ret_skip = stream->skip(stream, skip_bytes);
	ret = stream->read(stream, sizeof(buf.used), buf.used);
	stream->close(stream);

	assert_int_equal(ret_skip, skip_bytes);
	assert_int_equal(ret, sizeof(buf.used));
	assert_memory_equal(buf.used, test_storage + TEST_STORAGE_BLOCK_SIZE * 2 + skip_bytes,
			    sizeof(buf.used));
	assert_filled_with(buf.before, SENTINEL, sizeof(buf.before));
	assert_filled_with(buf.after, SENTINEL, sizeof(buf.after));
}

#define TEST_STREAM_READ(size) do { \
	memset(&buf, SENTINEL, sizeof(buf)); \
	ret = stream->read(stream, size, buf.used); \
	assert_int_equal(ret, size); \
	assert_memory_equal(buf.used, test_storage_ptr, size); \
	assert_filled_with(buf.before, SENTINEL, sizeof(buf.before)); \
	assert_filled_with(buf.used + (size), SENTINEL, \
			   sizeof(buf.after) + sizeof(buf.used) - (size)); \
	test_storage_ptr += size; \
} while (0)

static void test_simple_stream_read_multiple_not_aligned(void **state)
{
	char *test_storage_ptr = test_storage;
	struct {
		char before[TEST_STORAGE_BLOCK_SIZE];
		char used[TEST_STORAGE_BLOCK_SIZE * 3];
		char after[TEST_STORAGE_BLOCK_SIZE];
	} buf;
	uint64_t ret;
	StreamOps *stream = test_block_dev.ops.new_stream(&test_block_dev.ops, 2, 10);

	test_storage_ptr += TEST_STORAGE_BLOCK_SIZE * 2;

	ret = stream->skip(stream, 3);
	assert_int_equal(ret, 3);
	test_storage_ptr += 3;

	TEST_STREAM_READ(TEST_STORAGE_BLOCK_SIZE * 3);
	TEST_STREAM_READ(TEST_STORAGE_BLOCK_SIZE - 3);
	TEST_STREAM_READ(TEST_STORAGE_BLOCK_SIZE + 5);
	TEST_STREAM_READ(3);
	TEST_STREAM_READ(3);
	TEST_STREAM_READ(3);
	TEST_STREAM_READ(3);

	stream->close(stream);
}

static void test_simple_stream_read_over_the_end(void **state)
{
	struct {
		char before[TEST_STORAGE_BLOCK_SIZE];
		char used[TEST_STORAGE_BLOCK_SIZE];
		char after[TEST_STORAGE_BLOCK_SIZE];
	} buf;
	uint64_t ret;
	uint64_t ret_at_eof;
	StreamOps *stream = test_block_dev.ops.new_stream(&test_block_dev.ops, 2, 1);

	memset(&buf, SENTINEL, sizeof(buf));

	/* buf.used has size that we can read, try to request more data from the stream */
	ret = stream->read(stream, sizeof(buf.used) + 15, buf.used);
	/* try to read again */
	ret_at_eof = stream->read(stream, sizeof(buf.after), buf.after);
	stream->close(stream);

	assert_int_equal(ret, sizeof(buf.used));
	assert_int_equal(ret_at_eof, 0);
	assert_memory_equal(buf.used, test_storage + TEST_STORAGE_BLOCK_SIZE * 2,
			    sizeof(buf.used));
	assert_filled_with(buf.before, SENTINEL, sizeof(buf.before));
	assert_filled_with(buf.after, SENTINEL, sizeof(buf.after));
}

#define TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST(test_simple_stream_read_block_aligned),
		TEST(test_simple_stream_read_end_not_aligned),
		TEST(test_simple_stream_read_beginning_not_aligned),
		TEST(test_simple_stream_read_multiple_not_aligned),
		TEST(test_simple_stream_read_over_the_end),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
