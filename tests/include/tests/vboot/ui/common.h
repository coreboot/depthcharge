/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _TESTS_VBOOT_UI_COMMON_H
#define _TESTS_VBOOT_UI_COMMON_H

#include <tests/test.h>
#include <vb2_api.h>
#include <vboot/ui.h>

/* Fixed value for ignoring some checks. */
#define MOCK_IGNORE 0xffffu

#define _ASSERT_SCREEN_STATE(_state, _screen, _selected_item, \
			    _hidden_item_mask, ...) \
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

/*
 * Check the values of screen, selected_item and hidden_item_mask of _state by
 * assert_int_equal. Pass MOCK_IGNORE to ignore the value checking to
 * corresponding field. This macro supports variable length of parameters, and
 * will fill the rest of missing parameters with MOCK_IGNORE.
 */
#define ASSERT_SCREEN_STATE(_state, ...) \
	_ASSERT_SCREEN_STATE((_state), __VA_ARGS__, MOCK_IGNORE, MOCK_IGNORE, \
			     MOCK_IGNORE)

#define _EXPECT_UI_DISPLAY(_screen, _locale_id, _selected_item, \
			   _disabled_item_mask, _hidden_item_mask, \
			   _current_page, ...) \
	do { \
		if ((_screen) == MOCK_IGNORE) \
			expect_any(ui_display, screen); \
		else \
			expect_value(ui_display, screen, (_screen)); \
		if ((_locale_id) == MOCK_IGNORE) \
			expect_any(ui_display, locale_id); \
		else \
			expect_value(ui_display, locale_id, \
				     (_locale_id)); \
		if ((_selected_item) == MOCK_IGNORE) \
			expect_any(ui_display, selected_item); \
		else \
			expect_value(ui_display, selected_item, \
				     (_selected_item)); \
		if ((_disabled_item_mask) == MOCK_IGNORE) \
			expect_any(ui_display, disabled_item_mask); \
		else \
			expect_value(ui_display, disabled_item_mask, \
				     (_disabled_item_mask)); \
		if ((_hidden_item_mask) == MOCK_IGNORE) \
			expect_any(ui_display, hidden_item_mask); \
		else \
			expect_value(ui_display, hidden_item_mask, \
				     (_hidden_item_mask)); \
		if ((_current_page) == MOCK_IGNORE) \
			expect_any(ui_display, current_page); \
		else \
			expect_value(ui_display, current_page, \
				     (_current_page)); \
	} while (0)

/*
 * Add events to check the parameters of ui_display(). Pass MOCK_IGNORE to
 * ignore the value of the parameter. This macro supports variable length of
 * parameters, and will fill the rest of missing parameters with MOCK_IGNORE.
 */
#define EXPECT_UI_DISPLAY(...) \
	_EXPECT_UI_DISPLAY(__VA_ARGS__, MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE, \
			   MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE)

/*
 * Add events to check if ui_display() has been called by using expect_any
 * to all checked parameters.
 */
#define EXPECT_UI_DISPLAY_ANY() EXPECT_UI_DISPLAY(MOCK_IGNORE)

/*
 * Add expect_any_count with count -1 (which means to expect any always in
 * CMocka) to every parameters of ui_display.
 */
#define EXPECT_UI_DISPLAY_ANY_ALWAYS() \
	do { \
		expect_any_always(ui_display, screen); \
		expect_any_always(ui_display, locale_id); \
		expect_any_always(ui_display, selected_item); \
		expect_any_always(ui_display, disabled_item_mask); \
		expect_any_always(ui_display, hidden_item_mask); \
		expect_any_always(ui_display, current_page); \
	} while (0)

/*
 * Add return value to ui_keyboard_read.
 * SetUI_KEY_FLAG_TRUSTED_KEYBOARD flag if the passed trusted is non-zero.
 */
#define WILL_PRESS_KEY(key, trusted) \
	do { \
		will_return(ui_keyboard_read, \
			    (trusted) ? UI_KEY_FLAG_TRUSTED_KEYBOARD : 0); \
		will_return(ui_keyboard_read, (key)); \
	} while (0)

/*
 * Add will_return to ui_is_lid_open.
 * ui_is_lid_open will
 * 1. Return 1 for iterations - 1 times, and then
 * 2. Return 0 once.
 */
#define WILL_CLOSE_LID_IN(iterations) \
	do { \
		if ((iterations) > 1) { \
			will_return_count(ui_is_lid_open, 1, \
					  (iterations) - 1); \
		} \
		will_return(ui_is_lid_open, 0); \
	} while (0)

#define EXPECT_UI_LOG_INIT_ANY_ALWAYS() \
	do { \
		expect_any_always(ui_log_init, screen); \
		expect_any_always(ui_log_init, locale_code); \
		expect_any_always(ui_log_init, str); \
	} while (0)

#define _WILL_CALL_UI_LOG_INIT_ALWAYS(log_page_count, rv, ...) \
	do { \
		intmax_t _log_page_count = (log_page_count), \
			 _rv = (rv); \
		if (_log_page_count == MOCK_IGNORE) \
			will_return_always(ui_log_init_log_page_count, 1); \
		else \
			will_return_always(ui_log_init_log_page_count, \
					   (_log_page_count)); \
		if (_rv == MOCK_IGNORE) \
			will_return_always(ui_log_init, VB2_SUCCESS); \
		else \
			will_return_always(ui_log_init, _rv); \
	} while (0)

/*
 * Call will_return_always for ui_log_init_log_page_count and ui_log_init.
 * The first argument passed to this macro will be treated as the value that
 * will be set into log->page_count, and the second argument passed to this
 * macro is the return value of ui_log_init. If the first argument is
 * MOCK_IGNORE, the page_count will be set to 1. If the second argument is
 * omitted or is MOCK_IGNORE, the return value will be VB2_SUCCESS.
 */
#define WILL_CALL_UI_LOG_INIT_ALWAYS(...) \
	_WILL_CALL_UI_LOG_INIT_ALWAYS(__VA_ARGS__, MOCK_IGNORE)

#endif /* _TESTS_VBOOT_UI_COMMON_H */
