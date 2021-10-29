// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <string.h>
#include <tests/test.h>

#include "tests/vboot/common.h"
#include "vboot/ui.h"

/* Mock functions */
vb2_error_t ui_load_bitmap(enum ui_archive_type type, const char *file,
			   const char *locale_code, struct ui_bitmap *bitmap)
{
	check_expected(file);
	return VB2_SUCCESS;
}

/* Test functions */
static void test_ui_get_bitmap(void **state)
{
	const struct {
		char *name;
		int focused;
		char *expected_file;
	} cases[] = {
		{ "a.bmp", 0, "a.bmp" },
		{ "a.bmp", 1, "a_focus.bmp" },
		{ "a", 0, "a" },
		{ "a", 1, "a_focus" },
	};
	int i;
	struct ui_bitmap bitmap;

	for (i = 0; i < ARRAY_SIZE(cases); i++) {
		memset(&bitmap, 0, sizeof(bitmap));
		expect_string(ui_load_bitmap, file, cases[i].expected_file);
		ASSERT_VB2_SUCCESS(ui_get_bitmap(cases[i].name, NULL,
						 cases[i].focused, &bitmap));
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_ui_get_bitmap),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
