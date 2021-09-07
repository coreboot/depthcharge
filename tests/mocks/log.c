// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>

#include <vboot/ui.h>

static uint32_t ui_log_init_log_page_count(void)
{
	return mock_type(uint32_t);
}

/*
 * This mock function uses the function ui_log_init_log_page_count above to
 * retrieve the value that will be set to log->page_count. Use a will_return to
 * that function to specify the value.
 */
vb2_error_t ui_log_init(enum ui_screen screen, const char *locale_code,
			const char *str, struct ui_log_info *log)
{
	assert_non_null(log);

	check_expected(screen);
	check_expected_ptr(locale_code);
	check_expected_ptr(str);
	log->page_count = ui_log_init_log_page_count();
	return mock_type(vb2_error_t);
}
