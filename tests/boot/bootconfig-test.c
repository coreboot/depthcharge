// SPDX-License-Identifier: GPL-2.0

#include <string.h>

#include "boot/bootconfig.h"
#include "tests/test.h"

#define BR "\n"

char bootconfig_buf[512];
struct bootconfig bc;

static void test_bootconfig_api(void **state)
{
	struct bootconfig_trailer *trailer;
	struct bootconfig reinited_bc;
	char *init_params = "k0=v0" BR "k1=v1" BR;
	char *init_cmdline = "c0=v0 c1=\"v1\"      c2=v2";
	const char *expected = "k0=v0" BR "k1=v1" BR "c0=v0" BR "c1=\"v1\"" BR
			       "c2=v2" BR "testkey=testval" BR;

	/* Verify basic bootconfig API */
	bootconfig_init(&bc, bootconfig_buf, sizeof(bootconfig_buf));
	assert_int_equal(bootconfig_append_params(&bc, init_params, strlen(init_params)), 0);
	assert_int_equal(bootconfig_append_cmdline(&bc, init_cmdline), 0);
	assert_int_equal(bootconfig_append(&bc, "testkey", "testval"), 0);
	assert_string_equal(expected, bootconfig_buf);
	trailer = bootconfig_finalize(&bc, 0);
	assert_non_null(trailer);

	/*
	 * Verify that reinit correctly restore pointer and size of bootconfig
	 * space
	 */
	bootconfig_reinit(&reinited_bc, trailer);
	assert_ptr_equal(reinited_bc.start, bc.start);
	assert_int_equal(reinited_bc.size, bc.size);
}

static void test_bootconfig_params_validation(void **state)
{
	char *init_params = "k0=v0";

	/* No space in bootconfig area */
	bootconfig_init(&bc, bootconfig_buf, sizeof(struct bootconfig_trailer) + 1);
	assert_int_equal(bootconfig_append_params(&bc, init_params, strlen(init_params)), -1);
	assert_int_equal(bootconfig_append_cmdline(&bc, "too long"), -1);
	assert_int_equal(bootconfig_append(&bc, "too", "long"), -1);
}

static void test_bootconfig_unmatched_quote(void **state)
{
	char *init_cmdline_bad = "c0=\"v0";

	bootconfig_init(&bc, bootconfig_buf, sizeof(bootconfig_buf));
	assert_int_equal(bootconfig_append_cmdline(&bc, init_cmdline_bad), -1);
}

#define TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup)

static int setup(void **state)
{
	memset(&bc, 0, sizeof(bc));
	memset(bootconfig_buf, 0, sizeof(bootconfig_buf));
	return 0;
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		TEST(test_bootconfig_api),
		TEST(test_bootconfig_params_validation),
		TEST(test_bootconfig_unmatched_quote),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
