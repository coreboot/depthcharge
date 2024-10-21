/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _TESTS_VBOOT_UI_COMMON_H
#define _TESTS_VBOOT_UI_COMMON_H

#include <tests/test.h>
#include <vb2_api.h>
#include <vboot/ui.h>

/* Fixed value for ignoring some checks. */
#define MOCK_IGNORE 0xffffu

/* Add return value to ui_is_physical_presence_pressed. */
#define WILL_PRESS_PHYSICAL_PRESENCE(pressed) \
	will_return(ui_is_physical_presence_pressed, (pressed))

#define _ASSERT_SCREEN_STATE(_state, _screen, _focused_item, \
			    _hidden_item_mask, ...) \
	do { \
		if ((_screen) != MOCK_IGNORE) { \
			assert_non_null((_state)->screen); \
			assert_int_equal_msg((_state)->screen->id, (_screen), \
					     "screen"); \
		} \
		if ((_focused_item) != MOCK_IGNORE) \
			assert_int_equal_msg((_state)->focused_item, \
					     (_focused_item), \
					     "focused_item"); \
		if ((_hidden_item_mask) != MOCK_IGNORE) \
			assert_int_equal_msg((_state)->hidden_item_mask, \
					     (_hidden_item_mask), \
					     "hidden_item_mask"); \
	} while (0)

/*
 * Check the values of screen, focused_item and hidden_item_mask of _state by
 * assert_int_equal. Pass MOCK_IGNORE to ignore the value checking to
 * corresponding field. This macro supports variable length of parameters, and
 * will fill the rest of missing parameters with MOCK_IGNORE.
 */
#define ASSERT_SCREEN_STATE(_state, ...) \
	_ASSERT_SCREEN_STATE((_state), __VA_ARGS__, MOCK_IGNORE, MOCK_IGNORE, \
			     MOCK_IGNORE)

void ui_display_side_effect(void);

vb2_error_t _ui_display(enum ui_screen screen, uint32_t locale_id,
			uint32_t focused_item, uint32_t disabled_item_mask,
			uint32_t hidden_item_mask, int timer_disabled,
			uint32_t current_page, enum ui_error error_code);

#define _EXPECT_UI_DISPLAY(_screen, _locale_id, _focused_item, \
			   _disabled_item_mask, _hidden_item_mask, \
			   _current_page, _error_code, ...) \
	do { \
		if ((_screen) == MOCK_IGNORE) \
			expect_any(_ui_display, screen); \
		else \
			expect_value(_ui_display, screen, (_screen)); \
		if ((_locale_id) == MOCK_IGNORE) \
			expect_any(_ui_display, locale_id); \
		else \
			expect_value(_ui_display, locale_id, \
				     (_locale_id)); \
		if ((_focused_item) == MOCK_IGNORE) \
			expect_any(_ui_display, focused_item); \
		else \
			expect_value(_ui_display, focused_item, \
				     (_focused_item)); \
		if ((_disabled_item_mask) == MOCK_IGNORE) \
			expect_any(_ui_display, disabled_item_mask); \
		else \
			expect_value(_ui_display, disabled_item_mask, \
				     (_disabled_item_mask)); \
		if ((_hidden_item_mask) == MOCK_IGNORE) \
			expect_any(_ui_display, hidden_item_mask); \
		else \
			expect_value(_ui_display, hidden_item_mask, \
				     (_hidden_item_mask)); \
		if ((_current_page) == MOCK_IGNORE) \
			expect_any(_ui_display, current_page); \
		else \
			expect_value(_ui_display, current_page, \
				     (_current_page)); \
		if ((_error_code) == MOCK_IGNORE) \
			expect_any(_ui_display, error_code); \
		else \
			expect_value(_ui_display, error_code, (_error_code)); \
	} while (0)

/*
 * Add events to check the parameters of _ui_display(). Pass MOCK_IGNORE to
 * ignore the value of the parameter. This macro supports variable length of
 * parameters, and will fill the rest of missing parameters with MOCK_IGNORE.
 */
#define EXPECT_UI_DISPLAY(...) \
	_EXPECT_UI_DISPLAY(__VA_ARGS__, MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE, \
			   MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE)

/*
 * Add events to check if _ui_display() has been called by using expect_any
 * to all checked parameters.
 */
#define EXPECT_UI_DISPLAY_ANY() EXPECT_UI_DISPLAY(MOCK_IGNORE)

/*
 * Add expect_any_count with count -1 (which means to expect any always in
 * CMocka) to every parameters of _ui_display.
 */
#define EXPECT_UI_DISPLAY_ANY_ALWAYS() \
	do { \
		expect_any_always(_ui_display, screen); \
		expect_any_always(_ui_display, locale_id); \
		expect_any_always(_ui_display, focused_item); \
		expect_any_always(_ui_display, disabled_item_mask); \
		expect_any_always(_ui_display, hidden_item_mask); \
		expect_any_always(_ui_display, current_page); \
		expect_any_always(_ui_display, error_code); \
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

static inline uint32_t ui_mock_lines_per_page(void)
{
	return mock_type(uint32_t);
}

static inline uint32_t ui_mock_chars_per_line(void)
{
	return mock_type(uint32_t);
}

/* Set the mocked dimensions of the log textbox. */
#define SET_LOG_DIMENSIONS(lines_per_page, chars_per_line) \
	do { \
		will_return_always(ui_mock_lines_per_page, lines_per_page); \
		will_return_always(ui_mock_chars_per_line, chars_per_line); \
		will_return_always(ui_get_log_textbox_dimensions, VB2_SUCCESS); \
	} while (0)

#define _EXPECT_BEEP(_msec, _frequency, _expected_time, ...) \
	do { \
		will_return(ui_beep, (_expected_time)); \
		if ((_msec) != MOCK_IGNORE) \
			expect_value(ui_beep, msec, (_msec)); \
		else \
			expect_any(ui_beep, msec); \
		if ((_frequency) != MOCK_IGNORE) \
			expect_value(ui_beep, frequency, (_frequency)); \
		else \
			expect_any(ui_beep, frequency); \
	} while (0)

/*
 * Expect a ui_beep call with specified duration and frequency at the
 * specified time. The expected time in ui_beep is checked using
 * ASSERT_TIME_RANGE. This macro supports variable length of parameters, for
 * example:
 *
 * EXPECT_BEEP(100, 200): Expect a 200Hz beep with 100ms duration
 * EXPECT_BEEP(100): Expect a beep with 100ms duration
 * EXPECT_BEEP(): Expect a beep with any duration and frequency
 */
#define EXPECT_BEEP(...) \
	_EXPECT_BEEP(__VA_ARGS__, MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE)

/*
 * Time window for ASSERT_TIME_RANGE macro.
 * This value is used to deal with some random internal delays. For example,
 * using either > or >= can affect the time when the function is being called.
 * FUZZ_MS and ASSERT_TIME_RANGE can be used in order to avoid some hard-coded
 * delay value in tests.
 */
#define FUZZ_MS (3 * UI_KEY_DELAY_MS)

/*
 * Check if the value is no less than the expected time, but smaller than
 * (expected + FUZZ_MS).
 */
#define ASSERT_TIME_RANGE(value, expected) \
	do { \
		intmax_t _local_expected = (expected); \
		assert_in_range((value), \
				_local_expected, \
				_local_expected + FUZZ_MS - 1); \
	} while (0)

#endif /* _TESTS_VBOOT_UI_COMMON_H */
