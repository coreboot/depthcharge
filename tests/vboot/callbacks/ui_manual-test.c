// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/ui/common.h>
#include <mocks/callbacks.h>
#include <mocks/util/commonparams.h>
#include <mocks/vb2api.h>
#include <vboot_api.h>
#include <vb2_api.h>
#include <vboot/util/commonparams.h>
#include <vboot/callbacks/ui.c>

#define IGNORE_VB_TRY_LOAD_KERNEL() \
	do { \
		will_return_always(VbTryLoadKernel, \
				   VB2_ERROR_LK_NO_DISK_FOUND); \
		expect_value_count(VbTryLoadKernel, disk_flags, \
				   VB_DISK_FLAG_REMOVABLE, \
				   WILL_RETURN_ALWAYS); \
	} while (0)

/* Mock functions */
uint32_t VbExIsShutdownRequested(void) { return mock_type(uint32_t); }

/* Tests */
struct ui_context test_ui_ctx;

static int setup_context(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
	reset_mock_workbuf = 1;
	test_ui_ctx.ctx = vboot_get_context();
	*state = &test_ui_ctx;

	mock_time_ms = 31ULL * MSECS_PER_SEC;

	return 0;
}

static void setup_will_return_common(void)
{
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(vb2api_phone_recovery_ui_enabled, 1);
	will_return_maybe(vb2api_diagnostic_ui_enabled, 1);
	will_return_maybe(vb2api_allow_recovery, 1);
}

static void test_manual_action_success(void **state)
{
	struct ui_context *ui = *state;

	will_return(VbTryLoadKernel, VB2_SUCCESS);
	expect_value(VbTryLoadKernel, disk_flags, VB_DISK_FLAG_REMOVABLE);

	assert_int_equal(ui_manual_recovery_action(ui), VB2_REQUEST_UI_EXIT);
}

static void test_manual_action_no_disk_found(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	will_return(VbTryLoadKernel, VB2_ERROR_LK_NO_DISK_FOUND);
	expect_value(VbTryLoadKernel, disk_flags, VB_DISK_FLAG_REMOVABLE);

	assert_int_equal(ui_manual_recovery_action(ui),
			 VB2_REQUEST_UI_CONTINUE);

	ASSERT_SCREEN_STATE(ui->state, VB2_SCREEN_RECOVERY_SELECT);
}

static void test_manual_action_invalid_kernel(void **state)
{
	struct ui_context *ui = *state;

	will_return(VbTryLoadKernel, VB2_ERROR_LK_INVALID_KERNEL_FOUND);
	expect_value(VbTryLoadKernel, disk_flags, VB_DISK_FLAG_REMOVABLE);
	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(ui_manual_recovery_action(ui),
			 VB2_REQUEST_UI_CONTINUE);

	ASSERT_SCREEN_STATE(ui->state, VB2_SCREEN_RECOVERY_INVALID);
}

static void test_manual_ui_no_disk_found_invalid_kernel(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(VbExIsShutdownRequested, 0);
	will_return(VbTryLoadKernel, VB2_ERROR_LK_NO_DISK_FOUND);
	will_return(VbTryLoadKernel, VB2_ERROR_LK_INVALID_KERNEL_FOUND);
	will_return(VbTryLoadKernel, VB2_SUCCESS);
	expect_value_count(VbTryLoadKernel, disk_flags, VB_DISK_FLAG_REMOVABLE,
			   WILL_RETURN_ALWAYS);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();

	ASSERT_VB2_SUCCESS(vb2ex_manual_recovery_ui(ui->ctx));
}


static void test_manual_ui_invalid_kernel_no_disk_found(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(VbExIsShutdownRequested, 0);
	will_return(VbTryLoadKernel, VB2_ERROR_LK_INVALID_KERNEL_FOUND);
	will_return(VbTryLoadKernel, VB2_ERROR_LK_NO_DISK_FOUND);
	will_return(VbTryLoadKernel, VB2_SUCCESS);
	expect_value_count(VbTryLoadKernel, disk_flags, VB_DISK_FLAG_REMOVABLE,
			   WILL_RETURN_ALWAYS);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();

	ASSERT_VB2_SUCCESS(vb2ex_manual_recovery_ui(ui->ctx));
}

static void test_manual_ui_network_recovery_shortcut(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_NETWORK_RECOVERY, 0);
	will_return_maybe(VbExIsShutdownRequested, 0);
	will_return_maybe(VbTryLoadKernel, VB2_ERROR_LK_NO_DISK_FOUND);
	expect_value_count(VbTryLoadKernel, disk_flags, VB_DISK_FLAG_REMOVABLE,
			   WILL_RETURN_ALWAYS);
	will_return(VbTryLoadMiniOsKernel, VB2_SUCCESS);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();

	ASSERT_VB2_SUCCESS(vb2ex_manual_recovery_ui(ui->ctx));
}

static void test_manual_ui_timeout(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(ui_keyboard_read, 0);
	WILL_SHUTDOWN_IN(3);
	EXPECT_DISPLAY_UI_ANY();
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_power_button_shutdown(void **state)
{
	if (CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(ui_keyboard_read, 0);
	WILL_SHUTDOWN_IN(3);
	EXPECT_DISPLAY_UI_ANY();
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_boot_with_valid_image(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(VbExIsShutdownRequested, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return(VbTryLoadKernel, VB2_SUCCESS);
	expect_value(VbTryLoadKernel, disk_flags, VB_DISK_FLAG_REMOVABLE);
	EXPECT_DISPLAY_UI_ANY();

	ASSERT_VB2_SUCCESS(vb2ex_manual_recovery_ui(ui->ctx));
}

static void test_manual_ui_boot_with_valid_image_later(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(VbExIsShutdownRequested, 0);
	will_return(VbTryLoadKernel, VB2_ERROR_LK_NO_DISK_FOUND);
	will_return(VbTryLoadKernel, VB2_ERROR_LK_NO_DISK_FOUND);
	will_return(VbTryLoadKernel, VB2_SUCCESS);
	expect_value_count(VbTryLoadKernel, disk_flags, VB_DISK_FLAG_REMOVABLE,
			   WILL_RETURN_ALWAYS);
	EXPECT_DISPLAY_UI_ANY();

	ASSERT_VB2_SUCCESS(vb2ex_manual_recovery_ui(ui->ctx));
}

static void test_manual_ui_boot_invalid_remove_valid(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(VbExIsShutdownRequested, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return(VbTryLoadKernel, VB2_ERROR_MOCK);
	will_return_count(VbTryLoadKernel, VB2_ERROR_LK_NO_DISK_FOUND, 2);
	will_return(VbTryLoadKernel, VB2_SUCCESS);
	expect_value_count(VbTryLoadKernel, disk_flags, VB_DISK_FLAG_REMOVABLE,
			   WILL_RETURN_ALWAYS);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_INVALID);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);

	ASSERT_VB2_SUCCESS(vb2ex_manual_recovery_ui(ui->ctx));
}

static void test_manual_ui_keyboard_to_dev_and_cancel(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_SHUTDOWN_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(' ', 0);
	will_return_maybe(ui_keyboard_read, 0);
	WILL_PRESS_PHYSICAL_PRESENCE(0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_TO_DEV);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_keyboard_to_dev_and_cancel_ppkeyboard(void **state)
{
	if (!CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_SHUTDOWN_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(' ', 0);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_TO_DEV);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_cancel_to_dev(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_SHUTDOWN_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(UI_KEY_ENTER, 1);
	will_return_maybe(vb2ex_physical_presence_pressed, 0);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_cancel_to_dev_ppkeyboard(void **state)
{
	if (!CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_SHUTDOWN_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(UI_KEY_DOWN, 1);
	WILL_PRESS_KEY(UI_KEY_ENTER, 1);
	will_return_maybe(vb2ex_physical_presence_pressed, 0);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_confirm_to_dev(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();

	will_return_maybe(VbExIsShutdownRequested, 0);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_PHYSICAL_PRESENCE(0);
	WILL_PRESS_PHYSICAL_PRESENCE(1);
	WILL_PRESS_PHYSICAL_PRESENCE(1);
	WILL_PRESS_PHYSICAL_PRESENCE(0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2ex_physical_presence_pressed, 0);
	expect_function_call(vb2api_enable_developer_mode);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_REBOOT_EC_TO_RO);
}

static void test_manual_ui_confirm_to_dev_ppkeyboard(void **state)
{
	if (!CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();

	will_return_maybe(VbExIsShutdownRequested, 0);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(UI_KEY_ENTER, 1);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2ex_physical_presence_pressed, 0);
	expect_function_call(vb2api_enable_developer_mode);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_REBOOT_EC_TO_RO);
}

static void test_manual_ui_confirm_by_untrusted_fails_ppkeyboard(void **state)
{
	if (!CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_SHUTDOWN_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2ex_physical_presence_pressed, 0);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	EXPECT_BEEP(250, 400, mock_time_ms + 3 * UI_KEY_DELAY_MS);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_cannot_enable_dev_enabled(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEVELOPER_MODE;
	setup_will_return_common();
	WILL_SHUTDOWN_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	EXPECT_BEEP(250, 400, mock_time_ms + 2 * UI_KEY_DELAY_MS);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_cannot_enable_dev_enabled_ppkeyboard(void **state)
{
	if (!CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEVELOPER_MODE;
	setup_will_return_common();
	WILL_SHUTDOWN_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(UI_KEY_ENTER, 1);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	EXPECT_BEEP(250, 400, mock_time_ms + 2 * UI_KEY_DELAY_MS);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_pp_button_stuck(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_SHUTDOWN_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_PHYSICAL_PRESENCE(1); /* Hold since boot */
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2ex_physical_presence_pressed, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_pp_button_stuck_press(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(VbExIsShutdownRequested, 0);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_PHYSICAL_PRESENCE(1); /* Hold since boot */
	WILL_PRESS_PHYSICAL_PRESENCE(0);
	WILL_PRESS_PHYSICAL_PRESENCE(1); /* Press again */
	WILL_PRESS_PHYSICAL_PRESENCE(0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2ex_physical_presence_pressed, 0);
	expect_function_call(vb2api_enable_developer_mode);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_TO_DEV);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_REBOOT_EC_TO_RO);
}

static void test_manual_ui_pp_button_cancel_enter_again(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(VbExIsShutdownRequested, 0);
	WILL_PRESS_KEY(0, 0);
	/* Enter to_dev */
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_PHYSICAL_PRESENCE(0);
	/* Press pp button */
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_PHYSICAL_PRESENCE(1);
	/* Space = back */
	WILL_PRESS_KEY(' ', 0);
	/* Wait */
	WILL_PRESS_KEY(0, 0);
	/* Enter to_dev */
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_PHYSICAL_PRESENCE(0);
	WILL_PRESS_PHYSICAL_PRESENCE(1);
	WILL_PRESS_PHYSICAL_PRESENCE(0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2ex_physical_presence_pressed, 0);
	expect_function_call(vb2api_enable_developer_mode);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_TO_DEV);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_TO_DEV);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_REBOOT_EC_TO_RO);
}

static void test_manual_ui_enter_diagnostics(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(VbExIsShutdownRequested, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	expect_function_call(vb2api_request_diagnostics);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx), VB2_REQUEST_REBOOT);
}

static void test_recovery_select_screen_disabled_and_hidden_mask(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_SHUTDOWN_IN(2);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x0);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_recovery_select_screen(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_SHUTDOWN_IN(20);
	will_return_maybe(vb2ex_get_locale_count, 10);

	EXPECT_DISPLAY_UI_ANY();
	/* #0: Language menu */
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_LANGUAGE_SELECT);
	/* #1: Phone recovery */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, 1);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_PHONE_STEP1);
	/* #2: External disk recovery */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, 2);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_DISK_STEP1);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, 2);
	/* #3: Launch diagnostics */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, 3);
	/* #4: Advanced options */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, 4);
	EXPECT_DISPLAY_UI(VB2_SCREEN_ADVANCED_OPTIONS);
	/* End of menu */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);  /* Blocked */
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, 5);

	will_return_maybe(ui_keyboard_read, 0);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_advanced_options_screen_disabled_and_hidden_mask(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_SHUTDOWN_IN(5);
	will_return_maybe(vb2ex_get_locale_count, 10);

	EXPECT_DISPLAY_UI_ANY();
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x0);

	will_return_maybe(ui_keyboard_read, 0);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_advanced_options_screen(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_SHUTDOWN_IN(30);
	will_return_maybe(vb2ex_get_locale_count, 10);
	will_return_maybe(vb2ex_prepare_log_screen, 1);

	EXPECT_DISPLAY_UI_ANY();
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI_ANY();
	/* #0: Language menu */
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_LANGUAGE_SELECT);
	/* #1: Enable dev mode */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 1);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_TO_DEV);
	/* #2: Debug info */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 2);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEBUG_INFO);
	/* #3: Firmware log */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 3);
	EXPECT_DISPLAY_UI(VB2_SCREEN_FIRMWARE_LOG);
	/* #4: Back */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 4);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	/* End of menu */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 2);

	expect_any_always(vb2ex_prepare_log_screen, str);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2ex_physical_presence_pressed, 0);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_language_ui_change_language(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_SHUTDOWN_IN(8);
	will_return_maybe(vb2ex_get_locale_count, 100);
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* select language */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* select locale 24 */
	will_return_maybe(ui_keyboard_read, 0);
	IGNORE_VB_TRY_LOAD_KERNEL();
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, 23);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, 23, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_LANGUAGE_SELECT, 23, 23);
	EXPECT_DISPLAY_UI(VB2_SCREEN_LANGUAGE_SELECT, 23, 24);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, 24);
	mock_locale_id = 23;

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);

	assert_int_equal(mock_locale_id, 24);
}

static void test_language_ui_locale_count_0(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_SHUTDOWN_IN(8);
	will_return_maybe(vb2ex_get_locale_count, 0);
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* select language */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* select locale 0 */
	will_return_maybe(ui_keyboard_read, 0);
	IGNORE_VB_TRY_LOAD_KERNEL();
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, 23);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, 23, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_LANGUAGE_SELECT, 23, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT, 0);
	mock_locale_id = 23;

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);

	assert_int_equal(mock_locale_id, 0);
}

static void test_debug_info(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	WILL_SHUTDOWN_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY('\t', 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_always(vb2ex_prepare_log_screen, 1);
	expect_any_always(vb2ex_prepare_log_screen, str);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEBUG_INFO);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_debug_info_enter_failed(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	WILL_SHUTDOWN_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY('\t', 0);
	will_return_always(vb2ex_prepare_log_screen, 0);
	will_return_maybe(ui_keyboard_read, 0);
	expect_any_always(vb2ex_prepare_log_screen, str);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	EXPECT_BEEP(250, 400, mock_time_ms + 2 * UI_KEY_DELAY_MS);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_debug_info_one_page(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	WILL_SHUTDOWN_IN(8);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY('\t', 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_always(vb2ex_prepare_log_screen, 1);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_DISPLAY_UI_ANY();
	/* 0x6 = 0b110 */
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEBUG_INFO, MOCK_IGNORE, 3, 0x6, 0x0, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	IGNORE_VB_TRY_LOAD_KERNEL();
	expect_any_always(vb2ex_prepare_log_screen, str);

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_debug_info_three_pages(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	WILL_SHUTDOWN_IN(15);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY('\t', 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* page 0, select page down */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* page 1, select page down */
	WILL_PRESS_KEY(UI_KEY_UP, 0);		/* page 2, select page down */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* page 2, select page up */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* page 1, select page up */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);		/* page 0, select page up */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* page 0, select page down */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);		/* page 1, select page down */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* page 1, select back */
	will_return_always(vb2ex_prepare_log_screen, 3);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEBUG_INFO, MOCK_IGNORE, 2, 0x2, 0x0, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEBUG_INFO, MOCK_IGNORE, 2, 0x0, 0x0, 1);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEBUG_INFO, MOCK_IGNORE, 2, 0x4, 0x0, 2);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEBUG_INFO, MOCK_IGNORE, 1, 0x4, 0x0, 2);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEBUG_INFO, MOCK_IGNORE, 1, 0x0, 0x0, 1);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEBUG_INFO, MOCK_IGNORE, 1, 0x2, 0x0, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEBUG_INFO, MOCK_IGNORE, 2, 0x2, 0x0, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEBUG_INFO, MOCK_IGNORE, 2, 0x0, 0x0, 1);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEBUG_INFO, MOCK_IGNORE, 3, 0x0, 0x0, 1);
	EXPECT_DISPLAY_UI(VB2_SCREEN_RECOVERY_SELECT);
	expect_any_always(vb2ex_prepare_log_screen, str);
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_firmware_log(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	firmware_log_snapshots_count = 0;
	WILL_SHUTDOWN_IN(10);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(vb2ex_prepare_log_screen, 1);
	will_return_maybe(ui_keyboard_read, 0);
	expect_string(vb2ex_prepare_log_screen, str, "1");
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_firmware_log_again_reacquire_new_one(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	firmware_log_snapshots_count = 0;
	WILL_SHUTDOWN_IN(10);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(vb2ex_prepare_log_screen, 1);
	will_return_maybe(ui_keyboard_read, 0);
	expect_string(vb2ex_prepare_log_screen, str, "1");
	expect_string(vb2ex_prepare_log_screen, str, "2");
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_firmware_log_back_not_reacquire_new_one(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	firmware_log_snapshots_count = 0;
	WILL_SHUTDOWN_IN(10);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY('\t', 0); /* enter debug info screen */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	will_return_maybe(vb2ex_prepare_log_screen, 1);
	will_return_maybe(ui_keyboard_read, 0);
	expect_string(vb2ex_prepare_log_screen, str, "1");
	/* Skip the one entry, which is for preparing debug info */
	expect_any(vb2ex_prepare_log_screen, str);
	expect_string(vb2ex_prepare_log_screen, str, "1");
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	IGNORE_VB_TRY_LOAD_KERNEL();

	assert_int_equal(vb2ex_manual_recovery_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

#define UI_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup_context)

int main(void)
{
	const struct CMUnitTest tests[] = {
		/* Manual recovery action */
		UI_TEST(test_manual_action_success),
		UI_TEST(test_manual_action_no_disk_found),
		UI_TEST(test_manual_action_invalid_kernel),
		/* Manual recovery ui */
		UI_TEST(test_manual_ui_no_disk_found_invalid_kernel),
		UI_TEST(test_manual_ui_invalid_kernel_no_disk_found),
		UI_TEST(test_manual_ui_network_recovery_shortcut),
		UI_TEST(test_manual_ui_timeout),
		UI_TEST(test_manual_ui_power_button_shutdown),
		UI_TEST(test_manual_ui_boot_with_valid_image),
		UI_TEST(test_manual_ui_boot_with_valid_image_later),
		UI_TEST(test_manual_ui_boot_invalid_remove_valid),
		UI_TEST(test_manual_ui_keyboard_to_dev_and_cancel),
		UI_TEST(test_manual_ui_keyboard_to_dev_and_cancel_ppkeyboard),
		UI_TEST(test_manual_ui_cancel_to_dev),
		UI_TEST(test_manual_ui_cancel_to_dev_ppkeyboard),
		UI_TEST(test_manual_ui_confirm_to_dev),
		UI_TEST(test_manual_ui_confirm_to_dev_ppkeyboard),
		UI_TEST(test_manual_ui_confirm_by_untrusted_fails_ppkeyboard),
		UI_TEST(test_manual_ui_cannot_enable_dev_enabled),
		UI_TEST(test_manual_ui_cannot_enable_dev_enabled_ppkeyboard),
		UI_TEST(test_manual_ui_pp_button_stuck),
		UI_TEST(test_manual_ui_pp_button_stuck_press),
		UI_TEST(test_manual_ui_pp_button_cancel_enter_again),
		UI_TEST(test_manual_ui_enter_diagnostics),
		/* Recovery select screen */
		UI_TEST(test_recovery_select_screen_disabled_and_hidden_mask),
		UI_TEST(test_recovery_select_screen),
		/* Advanced options screen */
		UI_TEST(test_advanced_options_screen_disabled_and_hidden_mask),
		UI_TEST(test_advanced_options_screen),
		/* Language select screen */
		UI_TEST(test_language_ui_change_language),
		UI_TEST(test_language_ui_locale_count_0),
		/* Debug info screen */
		UI_TEST(test_debug_info),
		UI_TEST(test_debug_info_enter_failed),
		UI_TEST(test_debug_info_one_page),
		UI_TEST(test_debug_info_three_pages),
		/* Firmware log */
		UI_TEST(test_firmware_log),
		UI_TEST(test_firmware_log_again_reacquire_new_one),
		UI_TEST(test_firmware_log_back_not_reacquire_new_one),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
