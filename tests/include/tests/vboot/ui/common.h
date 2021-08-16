/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _TESTS_VBOOT_UI_COMMON_H
#define _TESTS_VBOOT_UI_COMMON_H

#include <tests/test.h>
#include <vb2_api.h>
#include <vboot/ui.h>

/* Fixed value for ignoring some checks. */
#define MOCK_IGNORE 0xffffu

#define ASSERT_SCREEN_STATE(_state, _screen, _selected_item, \
			    _hidden_item_mask) \
	do { \
		if ((_screen) != MOCK_IGNORE) { \
			assert_non_null((_state)->screen); \
			assert_int_equal_msg((_state)->screen->id, (_screen), \
					     "screen"); \
		} \
		if ((_selected_item) != MOCK_IGNORE) \
			assert_int_equal_msg((_state)->selected_item, \
					     (_selected_item), \
					     "selected_item"); \
		if ((_hidden_item_mask) != MOCK_IGNORE) \
			assert_int_equal_msg((_state)->hidden_item_mask, \
					     (_hidden_item_mask), \
					     "hidden_item_mask"); \
	} while (0)

#define _EXPECT_DISPLAY_UI(_screen, _locale_id, _selected_item, \
			   _disabled_item_mask, _hidden_item_mask, \
			   _current_page, ...) \
	do { \
		if ((_screen) == MOCK_IGNORE) \
			expect_any(vb2ex_display_ui, screen); \
		else \
			expect_value(vb2ex_display_ui, screen, (_screen)); \
		if ((_locale_id) == MOCK_IGNORE) \
			expect_any(vb2ex_display_ui, locale_id); \
		else \
			expect_value(vb2ex_display_ui, locale_id, \
				     (_locale_id)); \
		if ((_selected_item) == MOCK_IGNORE) \
			expect_any(vb2ex_display_ui, selected_item); \
		else \
			expect_value(vb2ex_display_ui, selected_item, \
				     (_selected_item)); \
		if ((_disabled_item_mask) == MOCK_IGNORE) \
			expect_any(vb2ex_display_ui, disabled_item_mask); \
		else \
			expect_value(vb2ex_display_ui, disabled_item_mask, \
				     (_disabled_item_mask)); \
		if ((_hidden_item_mask) == MOCK_IGNORE) \
			expect_any(vb2ex_display_ui, hidden_item_mask); \
		else \
			expect_value(vb2ex_display_ui, hidden_item_mask, \
				     (_hidden_item_mask)); \
		if ((_current_page) == MOCK_IGNORE) \
			expect_any(vb2ex_display_ui, current_page); \
		else \
			expect_value(vb2ex_display_ui, current_page, \
				     (_current_page)); \
	} while (0)

/*
 * Add events to check the parameters of vb2ex_display_ui(). Pass MOCK_IGNORE to
 * ignore the value of the parameter. This macro supports variable length of
 * parameters, and will fill the rest of missing parameters with MOCK_IGNORE.
 */
#define EXPECT_DISPLAY_UI(...) \
	_EXPECT_DISPLAY_UI(__VA_ARGS__, MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE, \
			   MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE)

/*
 * Add events to check if vb2ex_display_ui() has been called by using expect_any
 * to all checked parameters.
 */
#define EXPECT_DISPLAY_UI_ANY() EXPECT_DISPLAY_UI(MOCK_IGNORE)

#endif /* _TESTS_VBOOT_UI_COMMON_H */
