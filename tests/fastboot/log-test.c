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

/* Make test cases to peek on test_data using const pointers, so it cannot be modified */
const char *test_data;
const char *oldest_test_data;
#define TEST_DATA_SIZE (FASTBOOT_LOG_BUF_SIZE + 766)

/* Run this function once to setup content of the test data */
static int one_time_test_data_setup(void **state)
{
	static char internal_test_data[TEST_DATA_SIZE];

	for (int i = 0; i < TEST_DATA_SIZE; i++)
		internal_test_data[i] = (char)i;

	test_data = internal_test_data;

	return 0;
}

static void write_test_data_to_logger(struct fastboot_log *log)
{
	fastboot_log_set_active(log);
	/*
	 * It is a little white-box testing, first write move internal index to 1234. Next
	 * write is over FASTBOOT_LOG_BUF_SIZE, so index doesn't move and whole internal buffer
	 * is written. This way (index != (total_bytes % FASTBOOT_LOG_BUF_SIZE)), so it is more
	 * likely to catch accidental misuse of different indexes by making wrong assumptions
	 * about them (as they are usually in sync when writes are shorter than
	 * FASTBOOT_LOG_BUF_SIZE, but let's exercise the code in all cases).
	 */
	test_logger->write(test_data, 1234);
	test_logger->write(test_data, TEST_DATA_SIZE);
	assert_int_equal(fastboot_log_get_total_bytes(log), TEST_DATA_SIZE + 1234);

	/* First byte that is actually written to logger */
	oldest_test_data = test_data + TEST_DATA_SIZE - FASTBOOT_LOG_BUF_SIZE;
}

/* State for the test created with GET_BUF_TEST macro */
struct get_buf_test_state {
	/* Initial test case state */
	size_t request_num;
	size_t expected_num;
	int get_offset;
	/* Setup state */
	struct FastbootOps *fb;
	const char *expected_buf;
	uint64_t get_idx;
	/* Runtime state */
	const char *got_buf;
};

/* Setup function for GET_BUF_TEST */
static int setup_test_fb_log_get_buf(void **state)
{
	struct get_buf_test_state *ts = *state;

	ts->got_buf = NULL;

	/* Call generic setup */
	setup((void **)&ts->fb);

	write_test_data_to_logger(ts->fb->log);
	ts->get_idx = fastboot_log_get_oldest_available_byte(ts->fb->log) + ts->get_offset;
	if (ts->expected_num > 0) {
		/*
		 * Make sure, that test is configured properly and it won't read past
		 * test_data
		 */
		assert_int_less_than(ts->expected_num + ts->get_offset,
				     FASTBOOT_LOG_BUF_SIZE + 1);
		ts->expected_buf = oldest_test_data + ts->get_offset;
	} else {
		ts->expected_buf = NULL;
	}

	return 0;
}

/* Teardown function for GET_BUF_TEST */
static int teardown_test_fb_log_get_buf(void **state)
{
	struct get_buf_test_state *ts = *state;

	if (ts->got_buf)
		fastboot_log_drop_buf(ts->fb->log, ts->got_buf);

	/* Call generic teardown */
	return teardown((void **)&ts->fb);
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

	fastboot_log_set_active(fb->log);
	test_logger->write("test_string", 11);

	num = 5;
	got_buf = fastboot_log_get_buf(fb->log, 2, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, 5);
	assert_memory_equal(got_buf, "st_st", 5);
	fastboot_log_drop_buf(fb->log, got_buf);
}

static void test_fb_log_get_buf_clipped_num(void **state)
{
	struct FastbootOps *fb = *state;
	const char *got_buf;
	size_t num;

	fastboot_log_set_active(fb->log);
	test_logger->write("test_string", 11);

	num = 5;
	got_buf = fastboot_log_get_buf(fb->log, 8, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, 3);
	assert_memory_equal(got_buf, "ing", 3);
	fastboot_log_drop_buf(fb->log, got_buf);
}

static void test_fb_log_get_buf_reading_past(void **state)
{
	struct FastbootOps *fb = *state;
	const char *got_buf;
	size_t num;

	fastboot_log_set_active(fb->log);
	test_logger->write("test_string", 11);
	/* Reading past 'total_len' should be blocked */
	num = 5;
	got_buf = fastboot_log_get_buf(fb->log, 11, &num);
	assert_null(got_buf);
}

static void test_fb_log_iter_before_oldest(void **state)
{
	struct FastbootOps *fb = *state;
	uint64_t iter;

	write_test_data_to_logger(fb->log);
	iter = fastboot_log_get_iter(fb->log, 11);
	assert_int_equal(iter, UINT64_MAX);
}

static void test_fb_log_iter_after_total(void **state)
{
	struct FastbootOps *fb = *state;
	uint64_t iter;

	write_test_data_to_logger(fb->log);
	iter = fastboot_log_get_iter(fb->log, FASTBOOT_LOG_BUF_SIZE + 2100);
	assert_int_equal(iter, UINT64_MAX);
}

static void test_fb_log_iter_inc(void **state)
{
	struct FastbootOps *fb = *state;
	uint64_t iter;
	char got[10];

	fastboot_log_set_active(fb->log);
	test_logger->write("test_string", 11);

	iter = fastboot_log_get_iter(fb->log, 5);
	assert_int_equal(iter, 5);
	/* Get every byte up to the last one */
	for (int i = 0; i < 5; i++) {
		got[i] = fastboot_log_get_byte_at_iter(fb->log, iter);
		assert_true(fastboot_log_inc_iter(fb->log, &iter));
	}
	/* Stay on the last byte */
	for (int i = 5; i < sizeof(got) - 1; i++) {
		got[i] = fastboot_log_get_byte_at_iter(fb->log, iter);
		assert_false(fastboot_log_inc_iter(fb->log, &iter));
	}
	got[sizeof(got) - 1] = '\0';
	assert_string_equal(got, "stringggg");
}

static void test_fb_log_iter_dec(void **state)
{
	struct FastbootOps *fb = *state;
	uint64_t iter;
	char got[10];

	fastboot_log_set_active(fb->log);
	test_logger->write("test_string", 11);

	iter = fastboot_log_get_iter(fb->log, 5);
	assert_int_equal(iter, 5);
	/* Get every byte up to the first one */
	for (int i = 0; i < 5; i++) {
		got[i] = fastboot_log_get_byte_at_iter(fb->log, iter);
		assert_true(fastboot_log_dec_iter(fb->log, &iter));
	}
	/* Stay on the first byte */
	for (int i = 5; i < sizeof(got) - 1; i++) {
		got[i] = fastboot_log_get_byte_at_iter(fb->log, iter);
		assert_false(fastboot_log_dec_iter(fb->log, &iter));
	}
	got[sizeof(got) - 1] = '\0';
	assert_string_equal(got, "s_tsetttt");
}

static void test_fb_log_iter_both_ways(void **state)
{
	struct FastbootOps *fb = *state;
	uint64_t iter;
	char got[12];

	fastboot_log_set_active(fb->log);
	test_logger->write("test_string", 11);

	iter = fastboot_log_get_iter(fb->log, 5);
	assert_int_equal(iter, 5);
	for (int i = 0; i < 4; i++) {
		got[i] = fastboot_log_get_byte_at_iter(fb->log, iter);
		assert_true(fastboot_log_inc_iter(fb->log, &iter));
	}
	for (int i = 4; i < sizeof(got) - 1; i++) {
		got[i] = fastboot_log_get_byte_at_iter(fb->log, iter);
		assert_true(fastboot_log_dec_iter(fb->log, &iter));
	}
	got[sizeof(got) - 1] = '\0';
	assert_string_equal(got, "strinirts_t");
}

static void test_fb_log_iter_inc_with_wrap(void **state)
{
	struct FastbootOps *fb = *state;
	uint64_t iter;
	char last_byte = test_data[TEST_DATA_SIZE - 1];

	write_test_data_to_logger(fb->log);

	/*
	 * Total bytes is 2000 + FASTBOOT_LOG_BUF_SIZE, so 5000th byte is 3000th byte that was
	 * written
	 */
	iter = fastboot_log_get_iter(fb->log, 5000);
	for (int i = 3000; i < FASTBOOT_LOG_BUF_SIZE - 1; i++) {
		assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter),
				 oldest_test_data[i]);
		assert_true(fastboot_log_inc_iter(fb->log, &iter));
	}
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), last_byte);
	/* Not possible to iterate over the last byte */
	assert_false(fastboot_log_inc_iter(fb->log, &iter));
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), last_byte);
	assert_false(fastboot_log_inc_iter(fb->log, &iter));
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), last_byte);
}

static void test_fb_log_iter_dec_with_wrap(void **state)
{
	struct FastbootOps *fb = *state;
	uint64_t iter;

	write_test_data_to_logger(fb->log);

	/* Iter is 500 bytes away from the newest byte */
	iter = fastboot_log_get_iter(fb->log, FASTBOOT_LOG_BUF_SIZE + 1500);
	for (int i = FASTBOOT_LOG_BUF_SIZE - 500; i > 0; i--) {
		assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter),
				 oldest_test_data[i]);
		assert_true(fastboot_log_dec_iter(fb->log, &iter));
	}
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), *oldest_test_data);
	/* Not possible to iterate over the first byte */
	assert_false(fastboot_log_dec_iter(fb->log, &iter));
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), *oldest_test_data);
	assert_false(fastboot_log_dec_iter(fb->log, &iter));
	assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter), *oldest_test_data);
}

static void test_fb_log_iter_both_ways_with_wrap(void **state)
{
	struct FastbootOps *fb = *state;
	uint64_t iter;
	int lowest_idx;

	write_test_data_to_logger(fb->log);

	/* Make sure that test doesn't try to push iter past the first byte */
	lowest_idx = MAX(FASTBOOT_LOG_BUF_SIZE - 10000, 0);

	/* Iter is 500 bytes away from the newest byte */
	iter = fastboot_log_get_iter(fb->log, FASTBOOT_LOG_BUF_SIZE + 1500);
	for (int i = FASTBOOT_LOG_BUF_SIZE - 500; i > lowest_idx; i--) {
		assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter),
				 oldest_test_data[i]);
		assert_true(fastboot_log_dec_iter(fb->log, &iter));
	}
	for (int i = lowest_idx; i < FASTBOOT_LOG_BUF_SIZE - 400; i++) {
		assert_int_equal(fastboot_log_get_byte_at_iter(fb->log, iter),
				 oldest_test_data[i]);
		assert_true(fastboot_log_inc_iter(fb->log, &iter));
	}
}

/* This is generic test function for GET_BUF_TEST */
static void test_fb_log_get_buf_full(void **state)
{
	struct get_buf_test_state *ts = *state;
	size_t num = ts->request_num;

	ts->got_buf = fastboot_log_get_buf(ts->fb->log, ts->get_idx, &num);
	if (ts->expected_num == 0) {
		assert_null(ts->got_buf);
		return;
	}
	assert_non_null(ts->got_buf);
	assert_int_equal(num, ts->expected_num);
	assert_memory_equal(ts->got_buf, ts->expected_buf, ts->expected_num);
}

static void test_fb_log_get_buf_after_idx_clip_num(void **state)
{
	struct FastbootOps *fb = *state;
	const char *got_buf;
	size_t num;

	fastboot_log_set_active(fb->log);
	/*
	 * This is a corner case. We need to keep internal log index at 0, otherwise we will
	 * fall into "wrapped_clip_num" test case.
	 */
	test_logger->write(test_data, FASTBOOT_LOG_BUF_SIZE);
	test_logger->write(test_data, FASTBOOT_LOG_BUF_SIZE);

	num = SIZE_MAX;
	got_buf = fastboot_log_get_buf(fb->log, FASTBOOT_LOG_BUF_SIZE + 4000, &num);
	assert_non_null(got_buf);
	assert_int_equal(num, FASTBOOT_LOG_BUF_SIZE - 4000);
	assert_memory_equal(got_buf, test_data + 4000, num);
	fastboot_log_drop_buf(fb->log, got_buf);
}

/*
 * `_case` is a string literal which is suffix of the test name
 * `_get_offset` is an offset from the oldest available byte to get
 * `_request_num` is a number of bytes to get from log
 * `_expected_num` if non-zero it is an actual number of bytes gotten from log, otherwise
 *                 it is expected that fastboot_log_get_buf returns NULL
 */
#define GET_BUF_TEST(_case, _get_offset, _request_num, _expected_num) { \
	("test_fb_log_get_buf-" _case), \
	test_fb_log_get_buf_full, setup_test_fb_log_get_buf, teardown_test_fb_log_get_buf, \
	(&(struct get_buf_test_state) { \
		.request_num = (_request_num), \
		.expected_num = (_expected_num), \
		.get_offset = (_get_offset), \
	}), \
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
		TEST(test_fb_log_get_buf_clipped_num),
		TEST(test_fb_log_get_buf_reading_past),
		TEST(test_fb_log_iter_before_oldest),
		TEST(test_fb_log_iter_after_total),
		TEST(test_fb_log_iter_inc),
		TEST(test_fb_log_iter_dec),
		TEST(test_fb_log_iter_both_ways),
		TEST(test_fb_log_iter_inc_with_wrap),
		TEST(test_fb_log_iter_dec_with_wrap),
		TEST(test_fb_log_iter_both_ways_with_wrap),
		GET_BUF_TEST("before_oldest", -10, 20, 0),
		GET_BUF_TEST("after_total", FASTBOOT_LOG_BUF_SIZE + 10, 20, 0),
		GET_BUF_TEST("after_idx", 100, 300, 300),
		GET_BUF_TEST("before_idx", FASTBOOT_LOG_BUF_SIZE - 1000, 300, 300),
		GET_BUF_TEST("before_idx_clip_num",
			     FASTBOOT_LOG_BUF_SIZE - 1000, SIZE_MAX, 1000),
		GET_BUF_TEST("wrapped",
			     2000, FASTBOOT_LOG_BUF_SIZE - 3000, FASTBOOT_LOG_BUF_SIZE - 3000),
		GET_BUF_TEST("wrapped_clip_num", 2000, SIZE_MAX, FASTBOOT_LOG_BUF_SIZE - 2000),
		TEST(test_fb_log_get_buf_after_idx_clip_num),
	};
	return cmocka_run_group_tests(tests, one_time_test_data_setup, NULL);
}
