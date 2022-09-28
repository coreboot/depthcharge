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
	/* Mock screens for testing the hooks of init/reinit/exit... */
	MOCK_SCREEN_HOOK0 = 0xef40,
	MOCK_SCREEN_HOOK1 = 0xef41,
	MOCK_SCREEN_HOOK2 = 0xef42,
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
extern const struct ui_screen_info mock_screen_hook0;
extern const struct ui_screen_info mock_screen_hook1;
extern const struct ui_screen_info mock_screen_hook2;
extern const struct ui_screen_info mock_screen_root;

extern const struct ui_menu_item mock_screen_menu_items[];
extern const struct ui_menu_item mock_screen_all_action_items[];

vb2_error_t mock_action_countdown(struct ui_context *ui);
vb2_error_t mock_action_base(struct ui_context *ui);
vb2_error_t mock_action_flag0(struct ui_context *ui);
vb2_error_t mock_action_flag1(struct ui_context *ui);
vb2_error_t mock_action_flag2(struct ui_context *ui);

/* For testing the hooks */
#define _MOCK_HOOK_INIT "init"
#define _MOCK_HOOK_REINIT "reinit"
#define _MOCK_HOOK_EXIT "exit"

#define _EXPECT_HOOK(_screen_id, _hook_name) do {			       \
	expect_value(check_mock_hook, screen_id, (_screen_id));		       \
	expect_string(check_mock_hook, hook_name, (_hook_name));	       \
} while (0)

#define EXPECT_HOOK_INIT(_screen_id)					       \
	_EXPECT_HOOK((_screen_id), _MOCK_HOOK_INIT)
#define EXPECT_HOOK_REINIT(_screen_id)					       \
	_EXPECT_HOOK((_screen_id), _MOCK_HOOK_REINIT)
#define EXPECT_HOOK_EXIT(_screen_id)					       \
	_EXPECT_HOOK((_screen_id), _MOCK_HOOK_EXIT)

void check_mock_hook(enum mock_screen screen_id, const char *hook_name);
vb2_error_t mock_action_init(struct ui_context *ui);
vb2_error_t mock_action_reinit(struct ui_context *ui);
vb2_error_t mock_action_exit(struct ui_context *ui);

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
