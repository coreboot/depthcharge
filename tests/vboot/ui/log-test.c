// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/context.h>
#include <vboot/ui.h>

/* Tests */
#define MOCK_TEXT_BOX_ROW	2
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

static void test_log_init_empty_string(void **state)
{
	struct ui_log_info log = { 0 };

	expect_value(ui_get_log_textbox_dimensions, screen, UI_SCREEN_FIRMWARE_LOG);
	expect_string(ui_get_log_textbox_dimensions, locale_code, "en");

	ASSERT_VB2_SUCCESS(ui_log_init(UI_SCREEN_FIRMWARE_LOG, "en", "", &log));
	assert_int_equal(log.page_count, 0);
}

static void test_log_init_one_page(void **state)
{
	struct ui_log_info log = { 0 };
	char *buf;

	expect_value(ui_get_log_textbox_dimensions, screen, UI_SCREEN_FIRMWARE_LOG);
	expect_string(ui_get_log_textbox_dimensions, locale_code, "en");

	ASSERT_VB2_SUCCESS(ui_log_init(UI_SCREEN_FIRMWARE_LOG, "en", "stub", &log));
	assert_int_equal(log.page_count, 1);

	buf = ui_log_get_page_content(&log, 0);
	assert_string_equal(buf, "stub");
}

static const char *multi_lines_content(void)
{
	return "PAGE0..abc"	/* Page 0 */
	       "ab..bc\n"
	       "PAGE1op\n"	/* Page 1 */
	       "ac.......x"
	       "PAGE2....."	/* Page 2 */
	       "...abc...";
}

static void test_log_init_three_pages(void **state)
{
	struct ui_log_info log = { 0 };
	const char *string = multi_lines_content();
	char *buf;

	expect_value(ui_get_log_textbox_dimensions, screen, UI_SCREEN_FIRMWARE_LOG);
	expect_string(ui_get_log_textbox_dimensions, locale_code, "en");

	ASSERT_VB2_SUCCESS(ui_log_init(UI_SCREEN_FIRMWARE_LOG, "en", string, &log));

	assert_int_equal(log.page_count, 3);

	buf = ui_log_get_page_content(&log, 0);
	assert_string_equal(buf, "PAGE0..abc\nab..bc");

	buf = ui_log_get_page_content(&log, 1);
	assert_string_equal(buf, "PAGE1op\nac.......x");

	buf = ui_log_get_page_content(&log, 2);
	assert_string_equal(buf, "PAGE2.....\n...abc...");

	buf = ui_log_get_page_content(&log, 100);
	assert_null(buf);
}

static void test_log_set_empty_anchor(void **state)
{
	struct ui_log_info log = { 0 };
	const char *string = multi_lines_content();
	const struct ui_anchor_info *anchor_info;

	const char *const anchors[] = {};

	expect_value(ui_get_log_textbox_dimensions, screen, UI_SCREEN_FIRMWARE_LOG);
	expect_string(ui_get_log_textbox_dimensions, locale_code, "en");

	ASSERT_VB2_SUCCESS(ui_log_init(UI_SCREEN_FIRMWARE_LOG, "en", string, &log));

	ASSERT_VB2_SUCCESS(ui_log_set_anchors(&log, anchors, ARRAY_SIZE(anchors)));
	anchor_info = &log.anchor_info;
	assert_int_equal(anchor_info->total_count, 0);
	assert_null(anchor_info->per_page_count);
}

#define MAX_PAGES 10

struct anchor_test_state {
	const char *anchor;
	uint32_t expected_total_count;
	uint32_t expected_per_page_count[MAX_PAGES];
};

static void test_log_set_one_anchor(void **state)
{
	struct ui_log_info log = { 0 };
	const char *string = multi_lines_content();
	const struct ui_anchor_info *anchor_info;
	struct anchor_test_state *anchor_test_state = *state;
	const char *const anchors[] = { anchor_test_state->anchor };
	int i;

	expect_value(ui_get_log_textbox_dimensions, screen, UI_SCREEN_FIRMWARE_LOG);
	expect_string(ui_get_log_textbox_dimensions, locale_code, "en");

	ASSERT_VB2_SUCCESS(ui_log_init(UI_SCREEN_FIRMWARE_LOG, "en", string, &log));
	ASSERT_VB2_SUCCESS(ui_log_set_anchors(&log, anchors, ARRAY_SIZE(anchors)));

	anchor_info = &log.anchor_info;
	assert_int_equal(anchor_info->total_count,
			 anchor_test_state->expected_total_count);

	for (i = 0; i < log.page_count; i++) {
		assert_int_equal(anchor_info->per_page_count[i],
				 anchor_test_state->expected_per_page_count[i]);
	}
}

static void test_log_set_multi_anchors(void **state)
{
	struct ui_log_info log = { 0 };
	const char *string = multi_lines_content();
	const struct ui_anchor_info *anchor_info;
	const char *const anchors[] = {
		"a",
		"ab",
		"ac",
		"abc",
	};

	expect_value(ui_get_log_textbox_dimensions, screen, UI_SCREEN_FIRMWARE_LOG);
	expect_string(ui_get_log_textbox_dimensions, locale_code, "en");

	ASSERT_VB2_SUCCESS(ui_log_init(UI_SCREEN_FIRMWARE_LOG, "en", string, &log));

	ASSERT_VB2_SUCCESS(ui_log_set_anchors(&log, anchors, ARRAY_SIZE(anchors)));
	anchor_info = &log.anchor_info;
	assert_int_equal(anchor_info->total_count, 8);
	assert_int_equal(anchor_info->per_page_count[0], 3);
	assert_int_equal(anchor_info->per_page_count[1], 2);
	assert_int_equal(anchor_info->per_page_count[2], 3);
}

/* `anchor` should be a string literal;
   `expected_per_page_count` should be an array of int. */
#define ANCHOR_TEST(_anchor, _expected_total_count, _expected_per_page_count) { \
	("test_log_set_one_anchor-" _anchor), \
	test_log_set_one_anchor, NULL, NULL, \
	(&(struct anchor_test_state) { \
		.anchor = _anchor, \
		.expected_total_count = _expected_total_count, \
		.expected_per_page_count = _expected_per_page_count, \
	}), \
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_log_init_empty_string),
		cmocka_unit_test(test_log_init_one_page),
		cmocka_unit_test(test_log_init_three_pages),
		cmocka_unit_test(test_log_set_empty_anchor),
		ANCHOR_TEST("ab", 2, EMPTY_WRAP({1, 0, 1})),
		ANCHOR_TEST("ad", 0, EMPTY_WRAP({0, 0, 0})),
		ANCHOR_TEST("xPAGE2", 1, EMPTY_WRAP({0, 1, 0})),
		cmocka_unit_test(test_log_set_multi_anchors),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
