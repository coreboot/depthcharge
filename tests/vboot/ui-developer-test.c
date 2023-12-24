// SPDX-License-Identifier: GPL-2.0

#include <stdbool.h>
#include <mocks/callbacks.h>
#include <mocks/payload.h>
#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/context.h>
#include <tests/vboot/ui/common.h>
#include <vb2_api.h>
#include <vboot/stages.h>

#include "debug/firmware_shell/common.h"

/* Mock functions */

/*
 * Set countdown for ui_is_lid_open.
 * -1: Never close (always returns 1).
 * 0: Shouldn't be called (fails the test).
 * positive value x: Returns 1 for first (x - 1) calls, returns 0 at the x-th
 * call, and fails the test at the (x + 1)-th call.
 */
int mock_close_lid_countdown;

int ui_is_lid_open(void)
{
	if (mock_close_lid_countdown == 0)
		fail_msg("%s called when countdown is 0.", __func__);

	if (mock_close_lid_countdown > 0)
		--mock_close_lid_countdown;

	return mock_close_lid_countdown != 0;
}

int has_external_display(void)
{
	return 0;
}

bool dc_dev_firmware_shell_enabled(void)
{
	return true;
}

void dc_dev_enter_firmware_shell(void)
{
	function_called();
}

/* Tests */
struct ui_context test_ui_ctx;
struct vb2_kernel_params test_kparams;

static int setup_context(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
	reset_mock_workbuf = 1;

	test_ui_ctx.ctx = vboot_get_context();

	test_ui_ctx.ctx->flags |= VB2_CONTEXT_DEVELOPER_MODE;
	test_ui_ctx.ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALLOWED;
	set_boot_mode(test_ui_ctx.ctx, VB2_BOOT_MODE_DEVELOPER);

	memset(&test_kparams, 0, sizeof(test_kparams));
	test_ui_ctx.kparams = &test_kparams;
	*state = &test_ui_ctx;

	payload_altfw_head_initialized = 0;

	mock_time_ms = 31ULL * MSECS_PER_SEC;
	/* Larger than DEV_DELAY_NORMAL_MS / UI_KEY_DELAY_MS */
	mock_close_lid_countdown = 3000;

	return 0;
}

static void test_developer_ui_shutdown_menu(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_dev_disallowed_no_boot_altfw(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags &= ~VB2_CONTEXT_DEV_BOOT_ALLOWED;
	mock_close_lid_countdown = 5;
	EXPECT_BEEP(250, 400);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_TO_NORM, MOCK_IGNORE, MOCK_IGNORE,
			  MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE,
			  UI_ERROR_DEV_BOOT_NOT_ALLOWED);

	/* Cancel the error box */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_TO_NORM);

	/* Nothing happens */
	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_ALTFW, 0);

	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_dev_disallowed_no_boot_internal(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags &= ~VB2_CONTEXT_DEV_BOOT_ALLOWED;
	mock_close_lid_countdown = 5;
	EXPECT_BEEP(250, 400);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_TO_NORM, MOCK_IGNORE, MOCK_IGNORE,
			  MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE,
			  UI_ERROR_DEV_BOOT_NOT_ALLOWED);

	/* Cancel the error box */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_TO_NORM);

	/* Nothing happens */
	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_INTERNAL, 0);

	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_dev_disallowed_default_internal(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags &= ~VB2_CONTEXT_DEV_BOOT_ALLOWED;
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	/*
	 * Should not display UI_SCREEN_DEVELOPER_MODE, and should not boot
	 * from default boot target after timeout.
	 */
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_TO_NORM);
	EXPECT_BEEP(250, 400);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_dev_disallowed_default_external(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags &= ~VB2_CONTEXT_DEV_BOOT_ALLOWED;
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_TO_NORM);
	EXPECT_BEEP(250, 400);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_dev_disallowed_to_norm_confirm(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags &= ~VB2_CONTEXT_DEV_BOOT_ALLOWED;
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_TO_NORM, MOCK_IGNORE, MOCK_IGNORE,
			  MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE,
			  UI_ERROR_DEV_BOOT_NOT_ALLOWED);
	EXPECT_BEEP(250, 400);

	/* Cancel the error box */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_TO_NORM);

	/* Select "Confirm" */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	expect_function_call(vb2api_disable_developer_mode);

	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_get_locale_count, 10);
	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_REBOOT);
}

static void test_developer_ui_internal_timeout(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t start_time = mock_time_ms;

	WILL_LOAD_INTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_BEEP(250, 400, start_time + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, start_time + DEV_DELAY_BEEP2_MS);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_SUCCESS);

	ASSERT_TIME_RANGE(mock_time_ms, start_time + DEV_DELAY_NORMAL_MS);
}

static void test_developer_ui_internal_fail_no_disk(void **state)
{
	struct ui_context *ui = *state;

	WILL_LOAD_INTERNAL_ALWAYS(VB2_ERROR_LK_NO_DISK_FOUND);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_NORMAL_MS);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_select_internal_menu(void **state)
{
	struct ui_context *ui = *state;

	WILL_LOAD_INTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_SUCCESS);
}

static void test_developer_ui_select_internal_keyboard(void **state)
{
	struct ui_context *ui = *state;

	WILL_LOAD_INTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_INTERNAL, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_developer_ui_select_internal_button(void **state)
{
	if (!CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	WILL_LOAD_INTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_PRESS_KEY(UI_BUTTON_VOL_DOWN_LONG_PRESS, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_developer_ui_external_disallowed_default_boot(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_NORMAL_MS);
	will_return_always(vb2api_get_dev_default_boot_target,
			   VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_external_timeout(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t start_time = mock_time_ms;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	EXPECT_BEEP(250, 400, start_time + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, start_time + DEV_DELAY_BEEP2_MS);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_always(vb2api_get_dev_default_boot_target,
			   VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));

	ASSERT_TIME_RANGE(mock_time_ms, start_time + DEV_DELAY_NORMAL_MS);
}

static void test_developer_ui_external_fail_no_disk(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_NORMAL_MS);
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_ERROR_LK_NO_DISK_FOUND);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_select_external_keyboard(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_EXTERNAL, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_developer_ui_select_external_keyboard_fail(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_ERROR_LK_NO_DISK_FOUND);

	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE);

	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_EXTERNAL, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_BOOT_EXTERNAL, MOCK_IGNORE, 1);
	EXPECT_BEEP(250, 400);

	/* Make sure UP/DOWN is still functional */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_BOOT_EXTERNAL, MOCK_IGNORE, 2);

	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_select_external_menu(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_SUCCESS);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_developer_ui_select_external_button(void **state)
{
	if (!CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_SUCCESS);
	WILL_PRESS_KEY(UI_BUTTON_VOL_UP_LONG_PRESS, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_developer_ui_select_altfw_keyboard(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED;
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_ALTFW, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(payload_get_altfw_count, 2);
	expect_value(payload_run_altfw, altfw_id, 0);
	will_return(payload_run_altfw, 0);

	expect_assert_failure(vboot_select_and_load_kernel(ui->ctx,
							   ui->kparams));
}

static void test_developer_ui_select_altfw_keyboard_disallowed(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	EXPECT_BEEP(250, 400, mock_time_ms);
	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_ALTFW, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_select_altfw_menu(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED;
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_ALTFW);
	will_return_maybe(payload_get_altfw_count, 5);

	EXPECT_UI_DISPLAY_ANY();
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_SELECT_ALTFW, MOCK_IGNORE, 1);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_SELECT_ALTFW, MOCK_IGNORE, 2);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_SELECT_ALTFW, MOCK_IGNORE, 3);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(ui_keyboard_read, 0);

	expect_value(payload_run_altfw, altfw_id, 3);
	will_return(payload_run_altfw, 0);

	expect_assert_failure(vboot_select_and_load_kernel(ui->ctx,
							   ui->kparams));
}

static void test_developer_ui_select_to_norm(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	expect_function_call(vb2api_disable_developer_mode);
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_REBOOT);
}

static void test_developer_ui_select_to_norm_cancel(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_select_to_norm_keyboard(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	expect_function_call(vb2api_disable_developer_mode);
	WILL_PRESS_KEY(UI_KEY_DEV_TO_NORM, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_REBOOT);
}

static void test_developer_ui_select_to_norm_disallowed(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 2);

	WILL_PRESS_KEY(UI_KEY_UP, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 1);

	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 1,
			  MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE,
			  UI_ERROR_TO_NORM_NOT_ALLOWED);
	EXPECT_BEEP(250, 400, mock_time_ms);

	WILL_PRESS_KEY(UI_KEY_ENTER, 0);  /* Cancel error box */
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 1,
			  MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE,
			  UI_ERROR_NONE);

	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags,
			  VB2_GBB_FLAG_FORCE_DEV_SWITCH_ON);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_select_to_norm_keyboard_disallowed(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 2);

	WILL_PRESS_KEY(UI_KEY_DEV_TO_NORM, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 2,
			  MOCK_IGNORE, MOCK_IGNORE, MOCK_IGNORE,
			  UI_ERROR_TO_NORM_NOT_ALLOWED);
	EXPECT_BEEP(250, 400, mock_time_ms);

	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 2);

	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags,
			  VB2_GBB_FLAG_FORCE_DEV_SWITCH_ON);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_short_delay(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t start_time = mock_time_ms;

	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_LOAD_INTERNAL_ALWAYS(VB2_SUCCESS);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags,
			  VB2_GBB_FLAG_DEV_SCREEN_SHORT_DELAY);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));

	ASSERT_TIME_RANGE(mock_time_ms, start_time + DEV_DELAY_SHORT_MS);
}

static void test_developer_ui_stop_timer_on_input_normal_delay(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t start_time = mock_time_ms;

	WILL_PRESS_KEY('A', 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);

	assert_true(mock_time_ms > start_time + DEV_DELAY_NORMAL_MS + FUZZ_MS);
}

static void test_developer_ui_stop_timer_on_input_short_delay(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t start_time = mock_time_ms;

	WILL_PRESS_KEY('A', 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags,
			  VB2_GBB_FLAG_DEV_SCREEN_SHORT_DELAY);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);

	assert_true(mock_time_ms > start_time + DEV_DELAY_SHORT_MS + FUZZ_MS);
}

static void test_developer_ui_select_firmware_shell(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();

	/* Developer mode: Boot internal */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);

	/* Advanced options: Debug info */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0); /* Firmware Shell */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	expect_function_call(dc_dev_enter_firmware_shell);
	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_screen_default_boot_internal(void **state)
{
	struct ui_context *ui = *state;

	WILL_LOAD_INTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 2);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_developer_screen_default_boot_external(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 3);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_developer_screen_default_boot_altfw(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED;
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 4);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_ALTFW);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(payload_get_altfw_count, 2);
	expect_value(payload_run_altfw, altfw_id, 0);
	will_return(payload_run_altfw, 0);

	expect_assert_failure(vboot_select_and_load_kernel(ui->ctx,
							   ui->kparams));
}

static void test_developer_screen_disabled_and_hidden_altfw(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED;
	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	WILL_LOAD_INTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_developer_screen_disabled_and_hidden_force_dev(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED;
	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	WILL_LOAD_INTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags,
			  VB2_GBB_FLAG_FORCE_DEV_SWITCH_ON);
	will_return_maybe(ui_keyboard_read, 0);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_developer_screen_disabled_and_hidden_only_altfw(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags &= ~VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED;
	WILL_LOAD_INTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_developer_screen(void **state)
{
	struct ui_context *ui = *state;

	WILL_LOAD_INTERNAL_ALWAYS(VB2_SUCCESS);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_get_locale_count, 10);

	EXPECT_UI_DISPLAY_ANY();
	/* #0: Language menu */
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_LANGUAGE_SELECT);
	/* #1: Return to secure mode */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 1);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_TO_NORM);
	/* #2: Boot internal */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 2);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_developer_screen_external_default(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_SUCCESS);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_get_locale_count, 10);

	/* #3: Boot external */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 3);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_developer_screen_advanced_options(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	EXPECT_UI_DISPLAY_ANY();
	/* #5: Advanced options */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 5);
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x12);
	/* End of menu */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0); /* Blocked */
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 6);

	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_screen_advanced_options_screen(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_get_locale_count, 10);
	SET_LOG_DIMENSIONS(40, 20);

	EXPECT_UI_DISPLAY_ANY();
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY_ANY();
	/* #0: Language menu */
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_LANGUAGE_SELECT);
	/* #1: (Hidden) */
	/* #2: Debug info */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 2);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEBUG_INFO);
	/* #3: Firmware log */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 3);
	EXPECT_UI_DISPLAY(UI_SCREEN_FIRMWARE_LOG);
	/* #4: (Hidden) */
	/* #6: Back */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);  /* #5: Firmware Shell */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 5);
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 6);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE);
	/* End of menu */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 2);

	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_screen_debug_info(void **state)
{
	struct ui_context *ui = *state;

	WILL_PRESS_KEY('\t', 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_DEBUG_INFO);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	SET_LOG_DIMENSIONS(40, 20);
	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_screen_invalid_external_disk(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE);

	/* Try to boot from an invalid external disk */
	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_EXTERNAL, 0);
	/* The 1st for init(); the 2nd for action() */
	WILL_LOAD_EXTERNAL_COUNT(VB2_ERROR_LK_INVALID_KERNEL_FOUND, 2);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_INVALID_DISK);
	EXPECT_BEEP(250, 400);

	/* Unplug the invalid external disk */
	WILL_PRESS_KEY(0, 0);
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_ERROR_LK_NO_DISK_FOUND);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_BOOT_EXTERNAL);
	EXPECT_BEEP(250, 400);

	/* Press ESC to make sure we can get out of the polling screens */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEVELOPER_MODE);

	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

#define UI_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup_context)

int main(void)
{
	const struct CMUnitTest tests[] = {
		/* Developer actions */
		UI_TEST(test_developer_ui_shutdown_menu),
		UI_TEST(test_developer_ui_dev_disallowed_no_boot_altfw),
		UI_TEST(test_developer_ui_dev_disallowed_no_boot_internal),
		UI_TEST(test_developer_ui_dev_disallowed_default_internal),
		UI_TEST(test_developer_ui_dev_disallowed_default_external),
		UI_TEST(test_developer_ui_dev_disallowed_to_norm_confirm),
		UI_TEST(test_developer_ui_internal_timeout),
		UI_TEST(test_developer_ui_internal_fail_no_disk),
		UI_TEST(test_developer_ui_select_internal_menu),
		UI_TEST(test_developer_ui_select_internal_keyboard),
		UI_TEST(test_developer_ui_select_internal_button),
		UI_TEST(test_developer_ui_external_disallowed_default_boot),
		UI_TEST(test_developer_ui_external_timeout),
		UI_TEST(test_developer_ui_external_fail_no_disk),
		UI_TEST(test_developer_ui_select_external_keyboard),
		UI_TEST(test_developer_ui_select_external_keyboard_fail),
		UI_TEST(test_developer_ui_select_external_menu),
		UI_TEST(test_developer_ui_select_external_button),
		UI_TEST(test_developer_ui_select_altfw_keyboard),
		UI_TEST(test_developer_ui_select_altfw_keyboard_disallowed),
		UI_TEST(test_developer_ui_select_altfw_menu),
		UI_TEST(test_developer_ui_select_to_norm),
		UI_TEST(test_developer_ui_select_to_norm_cancel),
		UI_TEST(test_developer_ui_select_to_norm_keyboard),
		UI_TEST(test_developer_ui_select_to_norm_disallowed),
		UI_TEST(test_developer_ui_select_to_norm_keyboard_disallowed),
		UI_TEST(test_developer_ui_short_delay),
		UI_TEST(test_developer_ui_stop_timer_on_input_normal_delay),
		UI_TEST(test_developer_ui_stop_timer_on_input_short_delay),
		UI_TEST(test_developer_ui_select_firmware_shell),
		/* Developer screens */
		UI_TEST(test_developer_screen_default_boot_internal),
		UI_TEST(test_developer_screen_default_boot_external),
		UI_TEST(test_developer_screen_default_boot_altfw),
		UI_TEST(test_developer_screen_disabled_and_hidden_altfw),
		UI_TEST(test_developer_screen_disabled_and_hidden_force_dev),
		UI_TEST(test_developer_screen_disabled_and_hidden_only_altfw),
		UI_TEST(test_developer_screen),
		UI_TEST(test_developer_screen_external_default),
		UI_TEST(test_developer_screen_advanced_options),
		UI_TEST(test_developer_screen_advanced_options_screen),
		UI_TEST(test_developer_screen_debug_info),
		UI_TEST(test_developer_screen_invalid_external_disk),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
