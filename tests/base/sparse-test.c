// SPDX-License-Identifier: GPL-2.0

#include "base/sparse.h"
#include "tests/test.h"
#include "test_blockdev.h"

/*
 * Test sparse file generated using following commands:
 * $ git clone https://github.com/dlenski/PySIMG.git
 * $ cd PySIMG
 * $ python3
 * >>> import pysimg
 * >>> outf = open("sparse_image", 'wb')
 * >>> writer =  pysimg.SimgWriter(outf, blocksize=16, debug=1, dont_care=[b'FFFF'])
 * >>> writer.write(b'firstblock::::::')
 * >>> writer.write(b'secondblock:::::')
 * >>> writer.write(b'nextblockfill:::')
 * >>> writer.write(b'ffffffffffffffff')
 * >>> writer.write(b'nextblockskip:::')
 * >>> writer.write(b'FFFFFFFFFFFFFFFF')
 * >>> writer.write(b'lastblock:::::::')
 * >>> writer.flush()
 * >>> writer.close()
 * >>> outf.close()
 * >>> exit()
 * $ xxd -i sparse_image
 *
 * img2simg wasn't used, because it doesn't allow to use less than 1024 block size
 */

static unsigned char sparse_image[] = {
	0x3a, 0xff, 0x26, 0xed, 0x01, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x0c, 0x00,
	0x10, 0x00, 0x00, 0x00, 0x07, 0x00, 0x00, 0x00, 0x05, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xc1, 0xca, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
	0x3c, 0x00, 0x00, 0x00, 0x66, 0x69, 0x72, 0x73, 0x74, 0x62, 0x6c, 0x6f,
	0x63, 0x6b, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a, 0x73, 0x65, 0x63, 0x6f,
	0x6e, 0x64, 0x62, 0x6c, 0x6f, 0x63, 0x6b, 0x3a, 0x3a, 0x3a, 0x3a, 0x3a,
	0x6e, 0x65, 0x78, 0x74, 0x62, 0x6c, 0x6f, 0x63, 0x6b, 0x66, 0x69, 0x6c,
	0x6c, 0x3a, 0x3a, 0x3a, 0xc2, 0xca, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
	0x10, 0x00, 0x00, 0x00, 0x66, 0x66, 0x66, 0x66, 0xc1, 0xca, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00, 0x6e, 0x65, 0x78, 0x74,
	0x62, 0x6c, 0x6f, 0x63, 0x6b, 0x73, 0x6b, 0x69, 0x70, 0x3a, 0x3a, 0x3a,
	0xc3, 0xca, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00,
	0xc1, 0xca, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x1c, 0x00, 0x00, 0x00,
	0x6c, 0x61, 0x73, 0x74, 0x62, 0x6c, 0x6f, 0x63, 0x6b, 0x3a, 0x3a, 0x3a,
	0x3a, 0x3a, 0x3a, 0x3a
};

struct sparse_test_state {
	BlockDev *test_bdev;
};

/* Reset mock data (for use before each test) */
static int setup(void **state)
{
	struct sparse_test_state *sts = malloc(sizeof(*sts));

	if (sts == NULL)
		return -1;

	sts->test_bdev = NULL;
	*state = sts;

	return 0;
}

/* Free data allocated by test */
static int teardown(void **state)
{
	struct sparse_test_state *sts = *state;

	if (sts->test_bdev)
		free_test_blockdev(sts->test_bdev);

	return 0;
}

static void prepare_storage_and_expected(char *storage, char *expected, size_t size,
					 size_t expected_offset)
{
	for (int i = 0; i < size; i++) {
		storage[i] = i & 0xff;
		expected[i] = i & 0xff;
	}

	memcpy(expected + expected_offset + 0 * 16, "firstblock::::::", 16);
	memcpy(expected + expected_offset + 1 * 16, "secondblock:::::", 16);
	memcpy(expected + expected_offset + 2 * 16, "nextblockfill:::", 16);
	memcpy(expected + expected_offset + 3 * 16, "ffffffffffffffff", 16);
	memcpy(expected + expected_offset + 4 * 16, "nextblockskip:::", 16);
	memcpy(expected + expected_offset + 6 * 16, "lastblock:::::::", 16);
}

static void test_sparse_not_aligned_block_size(void **state)
{
	struct sparse_test_state *sts = *state;
	char storage[1024];
	sts->test_bdev = new_test_blockdev(storage, sizeof(storage), 32);

	enum gpt_io_ret ret = write_sparse_image(sts->test_bdev, 2, 10, sparse_image,
						 sizeof(sparse_image));

	assert_int_equal(ret, GPT_IO_SPARSE_BLOCK_SIZE_NOT_ALIGNED);
}

static void test_sparse_block_size_smaller(void **state)
{
	struct sparse_test_state *sts = *state;
	char storage[1024];
	char expected[1024];
	sts->test_bdev = new_test_blockdev(storage, sizeof(storage), 8);

	prepare_storage_and_expected(storage, expected, sizeof(storage), 8 * 4);

	enum gpt_io_ret ret = write_sparse_image(sts->test_bdev, 4, 24, sparse_image,
						 sizeof(sparse_image));

	assert_int_equal(ret, 0);
	assert_memory_equal(storage, expected, sizeof(expected));
}

static void test_sparse_block_size_equal(void **state)
{
	struct sparse_test_state *sts = *state;
	char storage[1024];
	char expected[1024];
	sts->test_bdev = new_test_blockdev(storage, sizeof(storage), 16);

	prepare_storage_and_expected(storage, expected, sizeof(storage), 16 * 3);

	enum gpt_io_ret ret = write_sparse_image(sts->test_bdev, 3, 12, sparse_image,
						 sizeof(sparse_image));

	assert_int_equal(ret, 0);
	assert_memory_equal(storage, expected, sizeof(expected));
}

#define TEST(test_function_name) \
	cmocka_unit_test_setup_teardown(test_function_name, setup, teardown)

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST(test_sparse_not_aligned_block_size),
		TEST(test_sparse_block_size_smaller),
		TEST(test_sparse_block_size_equal),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
