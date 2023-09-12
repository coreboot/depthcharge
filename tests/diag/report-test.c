// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "diag/common.h"
#include "diag/diag_internal.h"
#include "tests/vboot/context.h"
#include "tests/test.h"

#define EVENT_SIZE (sizeof(union elog_event_cros_diag_log))
#define BUF_SIZE (EVENT_SIZE * DIAG_REPORT_EVENT_MAX * 2)

uint8_t buf[BUF_SIZE];
size_t event_num;

static int setup(void **state)
{
	memset(buf, 0, sizeof(buf));
	diag_report_clear();
	event_num = 0;
	return 0;
}

/* Helper functions */

static void check_event(uint8_t type, uint8_t result, uint16_t time_s)
{
	check_expected(type);
	check_expected(result);
	check_expected(time_s);
}

#define EXPECT_EVENT(_type, _result, _time_s) do {			       \
	expect_value(check_event, type, (_type));			       \
	expect_value(check_event, result, (_result));			       \
	expect_value(check_event, time_s, (_time_s));			       \
	event_num++;							       \
} while (0)

static void dump_report_and_verify_events(size_t buf_size)
{
	size_t offset;
	union elog_event_cros_diag_log *events;
	int i;

	if (buf_size > BUF_SIZE)
		fail_msg("The expected buf_size exceeds maximum buffer size");

	offset = diag_report_dump(buf, buf_size);
	events = (union elog_event_cros_diag_log *)buf;

	assert_int_equal(offset, event_num * sizeof(*events));
	assert_true(offset <= buf_size);

	for (i = 0; i < event_num; i++)
		check_event(events[i].type, events[i].result, events[i].time_s);
}

/* Test functions */

static void test_report_base(void **state)
{
	const uint8_t test_types[] = {
		ELOG_CROS_DIAG_TYPE_STORAGE_HEALTH,
		ELOG_CROS_DIAG_TYPE_STORAGE_TEST_SHORT,
		ELOG_CROS_DIAG_TYPE_STORAGE_TEST_EXTENDED,
		ELOG_CROS_DIAG_TYPE_MEMORY_QUICK,
		ELOG_CROS_DIAG_TYPE_MEMORY_FULL,
	};
	const uint8_t test_results[] = {
		ELOG_CROS_DIAG_RESULT_PASSED,
		ELOG_CROS_DIAG_RESULT_ERROR,
		ELOG_CROS_DIAG_RESULT_FAILED,
		ELOG_CROS_DIAG_RESULT_ABORTED,
		ELOG_CROS_DIAG_RESULT_ABORTED,
	};
	int i;

	will_return_maybe(timer_raw_value, 0);
	for (i = 0; i < ARRAY_SIZE(test_types); i++) {
		assert_int_equal(diag_report_start_test(test_types[i]), 0);
		assert_int_equal(diag_report_end_test(test_results[i]), 0);
	}
	for (i = ARRAY_SIZE(test_types) - 1; i >= 0; i--)
		EXPECT_EVENT(test_types[i], test_results[i], 0);
	dump_report_and_verify_events(BUF_SIZE);
}

static void test_report_time(void **state)
{
	const uint8_t type = ELOG_CROS_DIAG_TYPE_STORAGE_HEALTH;
	const uint8_t result = ELOG_CROS_DIAG_RESULT_PASSED;
	const uint16_t time = 123;

	will_return(timer_raw_value, 0);
	will_return(timer_raw_value, time * USECS_PER_SEC);

	assert_int_equal(diag_report_start_test(type), 0);
	assert_int_equal(diag_report_end_test(result), 0);

	EXPECT_EVENT(type, result, time);
	dump_report_and_verify_events(BUF_SIZE);
}

/* Ensure we keep the latest events */
static void test_report_events_overflow(void **state)
{
	const uint8_t type = ELOG_CROS_DIAG_TYPE_STORAGE_HEALTH;
	const uint8_t result = ELOG_CROS_DIAG_RESULT_PASSED;
	int i;

	/* The first DIAG_REPORT_EVENT_MAX events' time_s are all 0 */
	for (i = 0; i < DIAG_REPORT_EVENT_MAX; i++) {
		will_return(timer_raw_value, 0);  /* start */
		will_return(timer_raw_value, 0);  /* end */
	}
	/* The other events' time_s are all 1 */
	for (i = 0; i < DIAG_REPORT_EVENT_MAX - 1; i++) {
		will_return(timer_raw_value, i * USECS_PER_SEC);
		will_return(timer_raw_value, (i + 1) * USECS_PER_SEC);
	}

	for (i = 0; i < DIAG_REPORT_EVENT_MAX * 2 - 1; i++) {
		assert_int_equal(diag_report_start_test(type), 0);
		assert_int_equal(diag_report_end_test(result), 0);
	}

	/* We should see (DIAG_REPORT_EVENT_MAX) events, the oldest one's time_s
	   is 0, and all other events' time_s are 1. */
	for (i = 0; i < DIAG_REPORT_EVENT_MAX - 1; i++)
		EXPECT_EVENT(type, result, 1);
	EXPECT_EVENT(type, result, 0);
	dump_report_and_verify_events(BUF_SIZE);
}

static void test_report_error_out_of_bound(void **state)
{
	will_return_maybe(timer_raw_value, 0);
	/* Type out of bound */
	assert_int_equal(
		diag_report_start_test(DIAG_REPORT_EVENT_TYPE_MAX + 1), -1);
	/* Result out of bound */
	assert_int_equal(
		diag_report_start_test(ELOG_CROS_DIAG_TYPE_STORAGE_HEALTH), 0);
	assert_int_equal(
		diag_report_end_test(DIAG_REPORT_EVENT_RESULT_MAX + 1), -1);
}

static void test_report_error_time_out_of_bound(void **state)
{
	const uint8_t type = ELOG_CROS_DIAG_TYPE_STORAGE_HEALTH;
	const uint8_t result = ELOG_CROS_DIAG_RESULT_PASSED;

	will_return(timer_raw_value, 0);
	will_return(timer_raw_value,
		    ((uint64_t)UINT16_MAX + 1) * USECS_PER_SEC);

	assert_int_equal(diag_report_start_test(type), 0);
	assert_int_equal(diag_report_end_test(result), 0);

	EXPECT_EVENT(type, result, UINT16_MAX);
	dump_report_and_verify_events(BUF_SIZE);
}

static void test_report_redundant_commands(void **state)
{
	const uint8_t type1 = ELOG_CROS_DIAG_TYPE_STORAGE_HEALTH;
	const uint8_t type2 = ELOG_CROS_DIAG_TYPE_STORAGE_TEST_SHORT;
	const uint8_t result = ELOG_CROS_DIAG_RESULT_PASSED;

	will_return_maybe(timer_raw_value, 0);

	assert_int_equal(diag_report_start_test(type1), 0);
	/* This should mark type1 as error */
	assert_int_equal(diag_report_start_test(type2), 0);
	assert_int_equal(diag_report_end_test(result), 0);
	/* This should be ignored */
	assert_int_equal(diag_report_end_test(ELOG_CROS_DIAG_RESULT_FAILED), 0);

	EXPECT_EVENT(type2, result, 0);
	EXPECT_EVENT(type1, ELOG_CROS_DIAG_RESULT_ERROR, 0);
	dump_report_and_verify_events(BUF_SIZE);
}

static void test_report_small_buffer(void **state)
{
	const size_t expect_num = 3;
	const size_t small_buf = EVENT_SIZE * expect_num;
	const uint8_t type = ELOG_CROS_DIAG_TYPE_STORAGE_HEALTH;
	const uint8_t result = ELOG_CROS_DIAG_RESULT_PASSED;
	int i;

	will_return_maybe(timer_raw_value, 0);

	for (i = 0; i < expect_num * 10; i++) {
		assert_int_equal(diag_report_start_test(type), 0);
		assert_int_equal(diag_report_end_test(result), 0);
	}

	for (i = 0; i < expect_num; i++)
		EXPECT_EVENT(type, result, 0);
	dump_report_and_verify_events(small_buf);
}

#define REPORT_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		REPORT_TEST(test_report_base),
		REPORT_TEST(test_report_time),
		REPORT_TEST(test_report_events_overflow),
		REPORT_TEST(test_report_error_out_of_bound),
		REPORT_TEST(test_report_error_time_out_of_bound),
		REPORT_TEST(test_report_redundant_commands),
		REPORT_TEST(test_report_small_buffer),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
