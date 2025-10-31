// SPDX-License-Identifier: GPL-2.0

#include "boot/commandline.h"
#include "tests/test.h"

static void test_get_cmdline_uint32_value_basic(void **state)
{
	const char *cmdline = "param1=value1 bl.mtectrl=0x1234 param2=123 param3=0 "
			"quoted_param=\"0x5678\"";
	uint32_t value;
	int ret;

	ret = get_cmdline_uint32_value(cmdline, "bl.mtectrl", &value);
	assert_int_equal(ret, 0);
	assert_int_equal(value, 0x1234);

	ret = get_cmdline_uint32_value(cmdline, "param2", &value);
	assert_int_equal(ret, 0);
	assert_int_equal(value, 123);

	ret = get_cmdline_uint32_value(cmdline, "param3", &value);
	assert_int_equal(ret, 0);
	assert_int_equal(value, 0);

	ret = get_cmdline_uint32_value(cmdline, "nonexistent", &value);
	assert_int_equal(ret, -1);

	ret = get_cmdline_uint32_value(cmdline, "param1", &value); /* Not a number */
	assert_int_equal(ret, -1);

	ret = get_cmdline_uint32_value(cmdline, "quoted_param", &value);
	assert_int_equal(ret, 0);
	assert_int_equal(value, 0x5678);
}

static void test_get_cmdline_uint32_value_empty_cmdline(void **state)
{
	const char *cmdline = "";
	uint32_t value;
	int ret;

	ret = get_cmdline_uint32_value(cmdline, "bl.mtectrl", &value);
	assert_int_equal(ret, -1);
}

static void test_get_cmdline_uint32_value_null_cmdline(void **state)
{
	uint32_t value;
	int ret;

	ret = get_cmdline_uint32_value(NULL, "bl.mtectrl", &value);
	assert_int_equal(ret, -1);

	/* Test with null output pointer */
	ret = get_cmdline_uint32_value("bl.mtectrl=0x1234", "bl.mtectrl", NULL);
	assert_int_equal(ret, -1);
}

static void test_get_cmdline_uint32_value_key_at_end(void **state)
{
	const char *cmdline = "param1=value1 bl.mtectrl=0xABCD";
	uint32_t value;
	int ret;

	ret = get_cmdline_uint32_value(cmdline, "bl.mtectrl", &value);
	assert_int_equal(ret, 0);
	assert_int_equal(value, 0xABCD);
}

static void test_get_cmdline_uint32_value_key_with_spaces(void **state)
{
	const char *cmdline = " param1=value1  bl.mtectrl=0x1234   param3=value3";
	uint32_t value;
	int ret;

	ret = get_cmdline_uint32_value(cmdline, "bl.mtectrl", &value);
	assert_int_equal(ret, 0);
	assert_int_equal(value, 0x1234);
}

static void test_get_cmdline_uint32_value_inside_quotes(void **state)
{
	const char *cmdline = "someparam=\"this_is bl.mtectrl=0x45 in a value\" "
			"bl.mtectrl=0x1234";
	uint32_t value;
	int ret;

	ret = get_cmdline_uint32_value(cmdline, "bl.mtectrl", &value);
	assert_int_equal(ret, 0);
	assert_int_equal(value, 0x1234);
}

static void test_get_cmdline_uint32_value_only_inside_quotes(void **state)
{
	const char *cmdline = "someparam=\"this_is bl.mtectrl=0x45 in a value\"";
	uint32_t value;
	int ret;

	ret = get_cmdline_uint32_value(cmdline, "bl.mtectrl", &value);
	assert_int_equal(ret, -1);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_get_cmdline_uint32_value_basic),
		cmocka_unit_test(test_get_cmdline_uint32_value_empty_cmdline),
		cmocka_unit_test(test_get_cmdline_uint32_value_null_cmdline),
		cmocka_unit_test(test_get_cmdline_uint32_value_key_at_end),
		cmocka_unit_test(test_get_cmdline_uint32_value_key_with_spaces),
		cmocka_unit_test(test_get_cmdline_uint32_value_inside_quotes),
		cmocka_unit_test( test_get_cmdline_uint32_value_only_inside_quotes),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
