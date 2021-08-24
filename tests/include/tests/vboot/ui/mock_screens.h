/* SPDX-License-Identifier: GPL-2.0 */

#ifndef _TESTS_VBOOT_UI_MOCK_SCREENS_H
#define _TESTS_VBOOT_UI_MOCK_SCREENS_H

#include <tests/test.h>
#include <vb2_api.h>
#include <vboot/ui.h>

/* Mock screen index for testing screen utility functions. */
enum mock_screen {
	MOCK_SCREEN_INVALID = 0xef00,
	MOCK_SCREEN_BLANK = 0xef10,
	MOCK_SCREEN_BASE = 0xef11,
	MOCK_SCREEN_MENU = 0xef12,
	MOCK_SCREEN_TARGET0 = 0xef20,
	MOCK_SCREEN_TARGET1 = 0xef21,
	MOCK_SCREEN_TARGET2 = 0xef22,
	MOCK_SCREEN_ACTION = 0xef30,
	MOCK_SCREEN_ALL_ACTION = 0xef32,
	MOCK_SCREEN_ROOT = 0xefff,
};

extern const struct ui_screen_info mock_screen_blank;
extern struct ui_screen_info mock_screen_base;
extern const struct ui_screen_info mock_screen_menu;
extern const struct ui_screen_info mock_screen_target0;
extern const struct ui_screen_info mock_screen_target1;
extern const struct ui_screen_info mock_screen_target2;
extern const struct ui_screen_info mock_screen_action;
extern const struct ui_screen_info mock_screen_all_action;
extern const struct ui_screen_info mock_screen_root;

extern const struct ui_menu_item mock_screen_menu_items[];
extern const struct ui_menu_item mock_screen_all_action_items[];

vb2_error_t mock_action_countdown(struct ui_context *ui);
vb2_error_t mock_action_base(struct ui_context *ui);
vb2_error_t mock_action_flag0(struct ui_context *ui);
vb2_error_t mock_action_flag1(struct ui_context *ui);
vb2_error_t mock_action_flag2(struct ui_context *ui);

/*
 * Add will_return to mock_action_countdown. mock_action_countdown will
 * 1. Return VB2_SUCCESS for iterations - 1 times, and then
 * 2. Return VB2_REQUEST_UI_EXIT once.
 */
#define WILL_MOCK_ACTION_COUNTDOWN(iterations)                                 \
	do {                                                                   \
		if ((iterations) > 1)                                          \
			will_return_count(mock_action_countdown, VB2_SUCCESS,  \
					  (iterations) - 1);                   \
		will_return(mock_action_countdown, VB2_REQUEST_UI_EXIT);       \
	} while (0)

#endif /* _TESTS_VBOOT_UI_MOCK_SCREENS_H */
