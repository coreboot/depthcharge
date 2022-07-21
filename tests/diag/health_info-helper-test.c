// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "diag/health_info.h"
#include "mocks/util/commonparams.h"
#include "tests/test.h"

#include "diag/health_info.c"

#define DIAG_BUFFER_SIZE (64 * KiB)

static char info_buf[DIAG_BUFFER_SIZE];

static int setup(void **state)
{
	memset(info_buf, 0, sizeof(info_buf));
	reset_mock_workbuf = 1;
	return 0;
}

/* Test functions */
/* For stringify_device_lifetime() */

static void test_stringify_device_lifetime_not_defined(void **state)
{
	char *buf = info_buf;
	assert_ptr_not_equal(stringify_device_lifetime(buf, DIAG_BUFFER_SIZE,
						       0x0), buf);
	assert_string_equal(info_buf, "Not defined");
}

static void test_stringify_device_lifetime_used(void **state)
{
	char *buf = info_buf;
	assert_ptr_not_equal(stringify_device_lifetime(buf, DIAG_BUFFER_SIZE,
						       0x5), buf);
	assert_string_equal(info_buf, "40% - 50% device life time used");
}

static void test_stringify_device_lifetime_exceeded(void **state)
{
	char *buf = info_buf;
	assert_ptr_not_equal(stringify_device_lifetime(buf, DIAG_BUFFER_SIZE,
						       0xb), buf);
	assert_string_equal(info_buf,
			    "Exceeded its maximum estimated device life time");
}

static void test_stringify_device_lifetime_unknown(void **state)
{
	char *buf = info_buf;
	assert_ptr_not_equal(stringify_device_lifetime(buf, DIAG_BUFFER_SIZE,
						       0xff), buf);
	assert_string_equal(info_buf, "Unknown");
}

#define HEALTH_INFO_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

int main(void)
{
	const struct CMUnitTest tests[] = {
		HEALTH_INFO_TEST(test_stringify_device_lifetime_not_defined),
		HEALTH_INFO_TEST(test_stringify_device_lifetime_used),
		HEALTH_INFO_TEST(test_stringify_device_lifetime_exceeded),
		HEALTH_INFO_TEST(test_stringify_device_lifetime_unknown),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
