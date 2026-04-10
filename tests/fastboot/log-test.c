// SPDX-License-Identifier: GPL-2.0

#include "fastboot/fastboot.h"
#include "fastboot/log.h"
#include "tests/fastboot/fastboot_common_mocks.h"
#include "tests/test.h"
#include "vboot/ui.h"

/* Mocked functions */

static struct console_output_driver *test_logger;

void console_add_output_driver(struct console_output_driver *out)
{
	test_logger = out;
}

/* Reset mock data (for use before each test) */
static int setup(void **state)
{
	setup_test_fb();

	*state = &test_fb;

	fastboot_log_init(&test_fb);

	return 0;
}

static int teardown(void **state)
{
	struct FastbootOps *fb = *state;

	fastboot_log_release(fb);

	return 0;
}

/* Test functions start here */
static void test_fb_log_init(void **state)
{
	struct FastbootOps *fb = *state;

	assert_int_equal(fastboot_log_get_total_bytes(fb->log), 0);
	assert_int_equal(fastboot_log_get_oldest_available_byte(fb->log), 0);
	/* Logger should be inactive at the start */
	test_logger->write("test", 4);
	assert_int_equal(fastboot_log_get_total_bytes(fb->log), 0);
	assert_int_equal(fastboot_log_get_oldest_available_byte(fb->log), 0);
}

static void test_fb_log_write_no_active(void **state)
{
	struct FastbootOps *fb = *state;

	fastboot_log_set_active(fb->log);
	/* Check if setting active back to NULL is working */
	fastboot_log_set_active(NULL);
	test_logger->write("test", 4);
	assert_int_equal(fastboot_log_get_total_bytes(fb->log), 0);
	assert_int_equal(fastboot_log_get_oldest_available_byte(fb->log), 0);
}

static void test_fb_log_write_data(void **state)
{
	struct FastbootOps *fb = *state;
	const char *got_buf;
	size_t num = SIZE_MAX;

	fastboot_log_set_active(fb->log);
	test_logger->write("test", 4);
	assert_int_equal(fastboot_log_get_total_bytes(fb->log), 4);
	assert_int_equal(fastboot_log_get_oldest_available_byte(fb->log), 0);
	got_buf = fastboot_log_get_buf(fb->log, 0, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, 4);
	assert_memory_equal(got_buf, "test", num);
	fastboot_log_drop_buf(fb->log, got_buf);
}

static void test_fb_log_write_multiple(void **state)
{
	struct FastbootOps *fb = *state;
	const char *got_buf;
	size_t num = SIZE_MAX;

	fastboot_log_set_active(fb->log);
	test_logger->write("test\n", 5);
	test_logger->write("second", 6);
	test_logger->write(" and end\n", 9);
	assert_int_equal(fastboot_log_get_total_bytes(fb->log), 5 + 6 + 9);
	assert_int_equal(fastboot_log_get_oldest_available_byte(fb->log), 0);
	got_buf = fastboot_log_get_buf(fb->log, 0, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, 5 + 6 + 9);
	assert_memory_equal(got_buf, "test\nsecond and end\n", num);
	fastboot_log_drop_buf(fb->log, got_buf);
}

static void test_fb_log_write_more_than_buffer_size(void **state)
{
	struct FastbootOps *fb = *state;
	char buf[FASTBOOT_LOG_BUF_SIZE + 40];
	const char *got_buf;
	size_t num = SIZE_MAX;

	for (int i = 0; i < sizeof(buf); i++)
		buf[i] = (char)i;

	fastboot_log_set_active(fb->log);
	test_logger->write(buf, sizeof(buf));
	/* All (even unwritten) bytes should be count */
	assert_int_equal(fastboot_log_get_total_bytes(fb->log), sizeof(buf));
	assert_int_equal(fastboot_log_get_oldest_available_byte(fb->log), 40);
	/* Last FASTBOOT_LOG_BUF_SIZE bytes should be written */
	got_buf = fastboot_log_get_buf(fb->log, 40, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, FASTBOOT_LOG_BUF_SIZE);
	assert_memory_equal(got_buf, buf + sizeof(buf) - FASTBOOT_LOG_BUF_SIZE,
			    FASTBOOT_LOG_BUF_SIZE);
	fastboot_log_drop_buf(fb->log, got_buf);
}

static void test_fb_log_write_wrap_idx(void **state)
{
	struct FastbootOps *fb = *state;
	const char *got_buf;
	size_t num = SIZE_MAX;

	fb->log->idx = FASTBOOT_LOG_BUF_SIZE - 5;
	fb->log->total_len = FASTBOOT_LOG_BUF_SIZE;
	fastboot_log_set_active(fb->log);
	test_logger->write("test_string", 11);
	assert_int_equal(fastboot_log_get_total_bytes(fb->log), FASTBOOT_LOG_BUF_SIZE + 11);
	assert_int_equal(fastboot_log_get_oldest_available_byte(fb->log), 11);
	got_buf = fastboot_log_get_buf(fb->log, FASTBOOT_LOG_BUF_SIZE, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, 11);
	assert_memory_equal(got_buf, "test_string", num);
	fastboot_log_drop_buf(fb->log, got_buf);
}

static void test_fb_log_get_buf(void **state)
{
	struct FastbootOps *fb = *state;
	const char *got_buf;
	size_t num;

	for (int i = 0; i < FASTBOOT_LOG_BUF_SIZE; i++)
		fb->log->buf[i] = (char)i;

	fb->log->total_len = 10;
	fb->log->idx = 10;
	num = 5;
	got_buf = fastboot_log_get_buf(fb->log, 0, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, 5);
	assert_memory_equal(got_buf, fb->log->buf, 5);
	fastboot_log_drop_buf(fb->log, got_buf);

	/* Test 'num' is clipped to number of bytes which we actually can get */
	num = 5;
	got_buf = fastboot_log_get_buf(fb->log, 7, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, 3);
	assert_memory_equal(got_buf, fb->log->buf + 7, 3);
	fastboot_log_drop_buf(fb->log, got_buf);

	/* Reading past 'total_len' should be blocked */
	num = 5;
	got_buf = fastboot_log_get_buf(fb->log, 11, &num);
	assert_null(got_buf);

	/*
	 * idx and total_len doesn't need to be in sync after over FASTBOOT_LOG_BUF_SIZE bytes
	 * write. Setup that case.
	 */
	fb->log->total_len = FASTBOOT_LOG_BUF_SIZE + 2000;
	fb->log->idx = 1234;

	/* Reading before oldest byte should be blocked */
	num = 5;
	got_buf = fastboot_log_get_buf(fb->log, 11, &num);
	assert_null(got_buf);

	/* Read part of buffer after idx */
	num = 300;
	got_buf = fastboot_log_get_buf(fb->log, 2100, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, 300);
	assert_memory_equal(got_buf, fb->log->buf + 1234 + 100, 300);
	fastboot_log_drop_buf(fb->log, got_buf);

	/* Read part of buffer before idx */
	num = 300;
	got_buf = fastboot_log_get_buf(fb->log, FASTBOOT_LOG_BUF_SIZE + 1000, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, 300);
	/* We expect to get 1000 bytes before the oldest byte, so it is idx - 1000 */
	assert_memory_equal(got_buf, fb->log->buf + 234, 300);
	fastboot_log_drop_buf(fb->log, got_buf);

	/* Read part of buffer before idx with num clipping */
	num = SIZE_MAX;
	got_buf = fastboot_log_get_buf(fb->log, FASTBOOT_LOG_BUF_SIZE + 1000, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, 1000);
	assert_memory_equal(got_buf, fb->log->buf + 234, 1000);
	fastboot_log_drop_buf(fb->log, got_buf);

	/* Read wrapped buffer */
	num = FASTBOOT_LOG_BUF_SIZE - 3000;
	got_buf = fastboot_log_get_buf(fb->log, 4000, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, FASTBOOT_LOG_BUF_SIZE - 3000);
	assert_memory_equal(got_buf, fb->log->buf + 1234 + 2000, FASTBOOT_LOG_BUF_SIZE - 3234);
	assert_memory_equal(got_buf + FASTBOOT_LOG_BUF_SIZE - 3234, fb->log->buf, 234);
	fastboot_log_drop_buf(fb->log, got_buf);

	/* Read wrapped buffer with num clipping */
	num = SIZE_MAX;
	got_buf = fastboot_log_get_buf(fb->log, 4000, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, FASTBOOT_LOG_BUF_SIZE - 2000);
	assert_memory_equal(got_buf, fb->log->buf + 1234 + 2000, FASTBOOT_LOG_BUF_SIZE - 3234);
	assert_memory_equal(got_buf + FASTBOOT_LOG_BUF_SIZE - 3234, fb->log->buf, 1234);
	fastboot_log_drop_buf(fb->log, got_buf);

	/* Read part of buffer after idx with num clipping */
	fb->log->total_len = FASTBOOT_LOG_BUF_SIZE * 2 + 2000;
	fb->log->idx = 0;
	num = SIZE_MAX;
	got_buf = fastboot_log_get_buf(fb->log, FASTBOOT_LOG_BUF_SIZE + 4000, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, FASTBOOT_LOG_BUF_SIZE - 2000);
	assert_memory_equal(got_buf, fb->log->buf + 2000, num);
	fastboot_log_drop_buf(fb->log, got_buf);
}

static void test_fb_log_iter(void **state)
{
	struct FastbootOps *fb = *state;
	uint64_t iter;

	for (int i = 0; i < FASTBOOT_LOG_BUF_SIZE; i++)
		fb->log->buf[i] = (char)i;

	fb->log->total_len = 10;
	fb->log->idx = 10;
	iter = fastboot_log_get_iter(fb->log, 5);
	assert_int_equal(iter, 5);
	for (int i = 5; i < 9; i++) {
		assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[i]);
		assert_true(fastboot_log_inc_iter(fb->log, &iter));
	}
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[9]);
	/* Not possible to iterate over the last byte */
	assert_false(fastboot_log_inc_iter(fb->log, &iter));
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[9]);
	assert_false(fastboot_log_inc_iter(fb->log, &iter));
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[9]);
	for (int i = 9; i > 0; i--) {
		assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[i]);
		assert_true(fastboot_log_dec_iter(fb->log, &iter));
	}
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[0]);
	/* Not possible to iterate over the first byte */
	assert_false(fastboot_log_dec_iter(fb->log, &iter));
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[0]);
	assert_false(fastboot_log_dec_iter(fb->log, &iter));
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[0]);

	/* Iterator from invalid range */
	iter = fastboot_log_get_iter(fb->log, 11);
	assert_int_equal(iter, UINT64_MAX);

	/*
	 * idx and total_len doesn't need to be in sync after over FASTBOOT_LOG_BUF_SIZE bytes
	 * write. Setup that case.
	 */
	fb->log->total_len = FASTBOOT_LOG_BUF_SIZE + 2000;
	fb->log->idx = 1234;

	iter = fastboot_log_get_iter(fb->log, 5);
	assert_int_equal(iter, UINT64_MAX);

	iter = fastboot_log_get_iter(fb->log, FASTBOOT_LOG_BUF_SIZE + 2100);
	assert_int_equal(iter, UINT64_MAX);

	iter = fastboot_log_get_iter(fb->log, 5000);
	assert_int_equal(iter, 1234 + 3000);
	for (int i = 1234 + 3000; i < FASTBOOT_LOG_BUF_SIZE + 1233; i++) {
		assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter),
				 fb->log->buf[i % FASTBOOT_LOG_BUF_SIZE]);
		assert_true(fastboot_log_inc_iter(fb->log, &iter));
	}
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[1233]);
	/* Not possible to iterate over the last byte */
	assert_false(fastboot_log_inc_iter(fb->log, &iter));
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[1233]);
	assert_false(fastboot_log_inc_iter(fb->log, &iter));
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[1233]);
	for (int i = 1233 + FASTBOOT_LOG_BUF_SIZE; i > 1234; i--) {
		assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter),
				 fb->log->buf[i % FASTBOOT_LOG_BUF_SIZE]);
		assert_true(fastboot_log_dec_iter(fb->log, &iter));
	}
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[1234]);
	/* Not possible to iterate over the first byte */
	assert_false(fastboot_log_dec_iter(fb->log, &iter));
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[1234]);
	assert_false(fastboot_log_dec_iter(fb->log, &iter));
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), fb->log->buf[1234]);
}

#define TEST(test_function_name) \
	cmocka_unit_test_setup_teardown(test_function_name, setup, teardown)

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST(test_fb_log_init),
		TEST(test_fb_log_write_no_active),
		TEST(test_fb_log_write_data),
		TEST(test_fb_log_write_multiple),
		TEST(test_fb_log_write_more_than_buffer_size),
		TEST(test_fb_log_write_wrap_idx),
		TEST(test_fb_log_get_buf),
		TEST(test_fb_log_iter),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
