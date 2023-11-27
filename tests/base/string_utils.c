// SPDX-License-Identifier: GPL-2.0
//
#include <stdint.h>
#include <string.h>
#include "tests/test.h"
#include "base/string_utils.h"

static void test_set_key_value_in_separated_list(void **state)
{
	char *ret1, *ret2;
	char *expected;

	/* Test parameter validation */
	ret1 = set_key_value_in_separated_list("", NULL, "value1", "\n");
	assert_null(ret1);
	ret1 = set_key_value_in_separated_list("", "key1", NULL, "\n");
	assert_null(ret1);
	ret1 = set_key_value_in_separated_list("", "key1", "value1", NULL);
	assert_null(ret1);

	/* Add to null string */
	ret1 = set_key_value_in_separated_list(NULL, "key1", "value1", "\n");
	expected = "key1=value1";
	assert_int_equal(memcmp(ret1, expected, sizeof(expected)), 0);

	/* Add to empty string */
	ret1 = set_key_value_in_separated_list("", "key1", "value1", "\n");
	expected = "key1=value1";
	assert_int_equal(memcmp(ret1, expected, sizeof(expected)), 0);

	/* Add to existing string */
	ret1 = set_key_value_in_separated_list("key1=value1", "key2", "value2", "\n");
	expected = "key1=value1\nkey2=value2";
	assert_int_equal(memcmp(ret1, expected, sizeof(expected)), 0);

	/* Modify key value */
	ret2 = set_key_value_in_separated_list(ret1, "key2", "newvalue2", "\n");
	expected = "key1=value1\nkey2=newvalue2";
	assert_int_equal(memcmp(ret2, expected, sizeof(expected)), 0);

	ret1 = set_key_value_in_separated_list(ret2, "key1", "newvalue1", "\n");
	expected = "key1=newvalue1\nkey2=newvalue2";
	assert_int_equal(memcmp(ret1, expected, sizeof(expected)), 0);
}


int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test_setup(test_set_key_value_in_separated_list, NULL)
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
