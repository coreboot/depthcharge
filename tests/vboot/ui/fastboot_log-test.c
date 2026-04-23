// SPDX-License-Identifier: GPL-2.0

#include <string.h>
#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/context.h>
#include <vboot/ui.h>

/* Mocks */
#define MOCK_TEXT_BOX_ROW	4
#define MOCK_TEXT_BOX_COL	10

vb2_error_t ui_get_log_textbox_dimensions(enum ui_screen screen,
					  const char *locale_code,
					  uint32_t *lines_per_page,
					  uint32_t *chars_per_line)
{
	check_expected(screen);
	check_expected_ptr(locale_code);

	*lines_per_page = MOCK_TEXT_BOX_ROW;
	*chars_per_line = MOCK_TEXT_BOX_COL;

	return VB2_SUCCESS;
}

struct fastboot_log {
	const char *buf;
	uint64_t oldest;
	uint64_t total;
};

uint64_t fastboot_log_get_total_bytes(const struct fastboot_log *log)
{
	return log->total;
}

uint64_t fastboot_log_get_oldest_available_byte(const struct fastboot_log *log)
{
	return log->oldest;
}

uint64_t fastboot_log_get_iter(const struct fastboot_log *log, uint64_t byte)
{
	if (byte < log->oldest || byte >= log->total)
		return UINT64_MAX;

	return byte - log->oldest;
}

bool fastboot_log_inc_iter(const struct fastboot_log *log, uint64_t *iter)
{
	if (*iter + 1 >= log->total - log->oldest)
		return false;
	*iter += 1;
	return true;
}

bool fastboot_log_dec_iter(const struct fastboot_log *log, uint64_t *iter)
{
	if (*iter == 0)
		return false;
	*iter -= 1;
	return true;
}

char fastboot_log_get_byte_at_iter(const struct fastboot_log *log, uint64_t iter)
{
	assert_true(iter < log->total - log->oldest);

	return log->buf[iter];
}

/* Tests */

static void test_fastboot_log_init(void **state)
{
	struct ui_log_info log = { 0 };

	expect_value(ui_get_log_textbox_dimensions, screen, UI_SCREEN_FASTBOOT);
	expect_string(ui_get_log_textbox_dimensions, locale_code, "en");

	ASSERT_VB2_SUCCESS(ui_fb_log_init(UI_SCREEN_FASTBOOT, "en", &log));
	assert_int_equal(log.type, UI_LOG_TYPE_FASTBOOT);
	assert_int_equal(log.impl.fastboot_log.bytes_on_screen, 0);
}

static const char buf[] = "PAGE0..abc"	/* First page */
			  "test..data\n"
			  "short\n"	/* Page last -4 */
			  "ab..bc\n"
			  "PAGE1op\n"	/* Page 1 (top) */
			  "aaa\n"
			  "test\n"	/* Page last -3 */
			  "ac.......x"
			  "PAGE2....."	/* Page 2 (top) */
			  "...abc...\n"
			  "\n"		/* Page last -2 */
			  "data\n"
			  "PAGE3....."	/* Page 3 (top) */
			  "page.witho"
			  "utanylineb"	/* Page last -1 */
			  "reak......"
			  "PAGE4..abc"	/* Page 4 (top) */
			  "one.two..."
			  "LASTPAGE\n"	/* Last page */
			  "\n"
			  "\n"
			  "last";

static void init_mock_fastboot_log(struct fastboot_log *fb_log, uint64_t oldest)
{
	fb_log->buf = buf;
	fb_log->oldest = oldest;
	fb_log->total = oldest + sizeof(buf) - 1;
}

static void test_fastboot_log_last_page(void **state)
{
	struct fastboot_log fb_log;
	struct ui_log_info log = { 0 };
	const char last_page_content[] = "LASTPAGE\n\n\nlast";
	uint64_t first_byte;

	init_mock_fastboot_log(&fb_log, 3);

	expect_value(ui_get_log_textbox_dimensions, screen, UI_SCREEN_FASTBOOT);
	expect_string(ui_get_log_textbox_dimensions, locale_code, "en");

	ASSERT_VB2_SUCCESS(ui_fb_log_init(UI_SCREEN_FASTBOOT, "en", &log));
	assert_int_equal(log.type, UI_LOG_TYPE_FASTBOOT);

	ui_fb_log_set_last_page(&log, &fb_log);
	assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor >= fb_log.oldest);
	assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor < fb_log.total);
	first_byte = log.impl.fastboot_log.top_of_screen_byte_anchor - fb_log.oldest;
	assert_int_equal(log.impl.fastboot_log.bytes_on_screen, sizeof(last_page_content) - 1);
	assert_memory_equal(buf + first_byte, last_page_content, sizeof(last_page_content) - 1);
}

static void test_fastboot_log_first_page(void **state)
{
	struct fastboot_log fb_log;
	struct ui_log_info log = { 0 };
	const char first_page_content[] = "PAGE0..abctest..data\nshort\nab..bc\n";
	uint64_t first_byte;

	init_mock_fastboot_log(&fb_log, 3);

	expect_value(ui_get_log_textbox_dimensions, screen, UI_SCREEN_FASTBOOT);
	expect_string(ui_get_log_textbox_dimensions, locale_code, "en");

	ASSERT_VB2_SUCCESS(ui_fb_log_init(UI_SCREEN_FASTBOOT, "en", &log));
	assert_int_equal(log.type, UI_LOG_TYPE_FASTBOOT);

	ui_fb_log_set_first_page(&log, &fb_log);
	assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor >= fb_log.oldest);
	assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor < fb_log.total);
	first_byte = log.impl.fastboot_log.top_of_screen_byte_anchor - fb_log.oldest;
	assert_int_equal(log.impl.fastboot_log.bytes_on_screen, sizeof(first_page_content) - 1);
	assert_memory_equal(buf + first_byte, first_page_content,
			    sizeof(first_page_content) - 1);
}

static void test_fastboot_log_first_to_last_page(void **state)
{
	struct fastboot_log fb_log;
	struct ui_log_info log = { 0 };
	const char *const page_content[] = {
		"PAGE0..abctest..data\nshort\nab..bc\n",
		"PAGE1op\naaa\ntest\nac.......x",
		"PAGE2........abc...\n\ndata\n",
		"PAGE3.....page.withoutanylinebreak......",
		"PAGE4..abcone.two...LASTPAGE\n\n",
		"LASTPAGE\n\n\nlast",
	};
	uint64_t first_byte;
	size_t page_len;

	init_mock_fastboot_log(&fb_log, 3);

	expect_value(ui_get_log_textbox_dimensions, screen, UI_SCREEN_FASTBOOT);
	expect_string(ui_get_log_textbox_dimensions, locale_code, "en");

	ASSERT_VB2_SUCCESS(ui_fb_log_init(UI_SCREEN_FASTBOOT, "en", &log));
	assert_int_equal(log.type, UI_LOG_TYPE_FASTBOOT);

	page_len = strlen(page_content[0]);
	ui_fb_log_set_first_page(&log, &fb_log);
	assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor >= fb_log.oldest);
	assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor < fb_log.total);
	first_byte = log.impl.fastboot_log.top_of_screen_byte_anchor - fb_log.oldest;
	assert_int_equal(log.impl.fastboot_log.bytes_on_screen, page_len);
	assert_memory_equal(buf + first_byte, page_content[0], page_len);

	for (int i = 1; i < 6; i++) {
		page_len = strlen(page_content[i]);
		ui_fb_log_set_next_page(&log, &fb_log);
		assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor >= fb_log.oldest);
		assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor < fb_log.total);
		first_byte = log.impl.fastboot_log.top_of_screen_byte_anchor - fb_log.oldest;
		assert_int_equal(log.impl.fastboot_log.bytes_on_screen, page_len);
		assert_memory_equal(buf + first_byte, page_content[i], page_len);
	}

	/* We stay at the end when we request next page */
	page_len = strlen(page_content[5]);
	ui_fb_log_set_next_page(&log, &fb_log);
	assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor >= fb_log.oldest);
	assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor < fb_log.total);
	first_byte = log.impl.fastboot_log.top_of_screen_byte_anchor - fb_log.oldest;
	assert_int_equal(log.impl.fastboot_log.bytes_on_screen, page_len);
	assert_memory_equal(buf + first_byte, page_content[5], page_len);
}

static void test_fastboot_log_last_to_first_page(void **state)
{
	struct fastboot_log fb_log;
	struct ui_log_info log = { 0 };
	const char *const page_content[] = {
		"LASTPAGE\n\n\nlast",
		"utanylinebreak......PAGE4..abcone.two...",
		"\ndata\nPAGE3.....page.witho",
		"test\nac.......xPAGE2........abc...\n",
		"short\nab..bc\nPAGE1op\naaa\n",
		"PAGE0..abctest..data\nshort\nab..bc\n",
	};
	uint64_t first_byte;
	size_t page_len;

	init_mock_fastboot_log(&fb_log, 3);

	expect_value(ui_get_log_textbox_dimensions, screen, UI_SCREEN_FASTBOOT);
	expect_string(ui_get_log_textbox_dimensions, locale_code, "en");

	ASSERT_VB2_SUCCESS(ui_fb_log_init(UI_SCREEN_FASTBOOT, "en", &log));
	assert_int_equal(log.type, UI_LOG_TYPE_FASTBOOT);

	page_len = strlen(page_content[0]);
	ui_fb_log_set_last_page(&log, &fb_log);
	assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor >= fb_log.oldest);
	assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor < fb_log.total);
	first_byte = log.impl.fastboot_log.top_of_screen_byte_anchor - fb_log.oldest;
	assert_int_equal(log.impl.fastboot_log.bytes_on_screen, page_len);
	assert_memory_equal(buf + first_byte, page_content[0], page_len);

	for (int i = 1; i < 6; i++) {
		page_len = strlen(page_content[i]);
		ui_fb_log_set_prev_page(&log, &fb_log);
		assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor >= fb_log.oldest);
		assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor < fb_log.total);
		first_byte = log.impl.fastboot_log.top_of_screen_byte_anchor - fb_log.oldest;
		assert_int_equal(log.impl.fastboot_log.bytes_on_screen, page_len);
		assert_memory_equal(buf + first_byte, page_content[i], page_len);
	}

	/* We stay at the first page when we request prev page */
	page_len = strlen(page_content[5]);
	ui_fb_log_set_prev_page(&log, &fb_log);
	assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor >= fb_log.oldest);
	assert_true(log.impl.fastboot_log.top_of_screen_byte_anchor < fb_log.total);
	first_byte = log.impl.fastboot_log.top_of_screen_byte_anchor - fb_log.oldest;
	assert_int_equal(log.impl.fastboot_log.bytes_on_screen, page_len);
	assert_memory_equal(buf + first_byte, page_content[5], page_len);
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_fastboot_log_init),
		cmocka_unit_test(test_fastboot_log_last_page),
		cmocka_unit_test(test_fastboot_log_first_page),
		cmocka_unit_test(test_fastboot_log_first_to_last_page),
		cmocka_unit_test(test_fastboot_log_last_to_first_page),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
