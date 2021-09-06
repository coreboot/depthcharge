// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/ui/common.h>
#include <mocks/callbacks.h>
#include <mocks/util/commonparams.h>
#include <vboot/util/commonparams.h>

#define SETUP_VB_TRY_LOAD_KERNEL(_rv, _disk_flags) \
	do { \
		will_return_always(VbTryLoadKernel, (_rv)); \
		expect_value_count(VbTryLoadKernel, disk_flags, \
				   (_disk_flags), WILL_RETURN_ALWAYS); \
	} while (0)

/* Mock functions */

/*
 * Set countdown for VbExIsShutdownRequested.
 * -1: Never shutdown (always returns 0).
 * 0: Shouldn't be called (fails the test).
 * positive value x: Returns 1 at the x-th call. Returns 0 for first (x - 1)
 * calls, fails the test at the (x + 1)-th call.
 */
int mock_shutdown_countdown;

uint32_t VbExIsShutdownRequested(void)
{
	if (mock_shutdown_countdown == 0)
		fail_msg("%s called when countdown is 0.", __func__);

	if (mock_shutdown_countdown > 0)
		--mock_shutdown_countdown;

	return mock_shutdown_countdown == 0;
}

/* Tests */
struct ui_context test_ui_ctx;

static int setup_context(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
	reset_mock_workbuf = 1;

	test_ui_ctx.ctx = vboot_get_context();

	test_ui_ctx.ctx->flags |= VB2_CONTEXT_DEVELOPER_MODE;
	test_ui_ctx.ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALLOWED;

	*state = &test_ui_ctx;

	mock_time_ms = 31ULL * MSECS_PER_SEC;
	/* Larger than DEV_DELAY_NORMAL_MS / UI_KEY_DELAY_MS */
	mock_shutdown_countdown = 3000;

	return 0;
}

static void test_developer_ui_shutdown_timeout(void **state)
{
	if (CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	WILL_PRESS_KEY(UI_BUTTON_POWER_SHORT_PRESS, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_shutdown_menu(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_dev_disallowed_no_boot_altfw(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags &= ~VB2_CONTEXT_DEV_BOOT_ALLOWED;
	mock_shutdown_countdown = 5;
	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_ALTFW, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_TO_NORM);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_dev_disallowed_no_boot_internal(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags &= ~VB2_CONTEXT_DEV_BOOT_ALLOWED;
	mock_shutdown_countdown = 5;
	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_INTERNAL, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_TO_NORM);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
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
	 * Should not display VB2_SCREEN_DEVELOPER_MODE, and should not boot
	 * from default boot target after timeout.
	 */
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_TO_NORM);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_dev_disallowed_default_external(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags &= ~VB2_CONTEXT_DEV_BOOT_ALLOWED;
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_TO_NORM);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_dev_disallowed_to_norm_confirm(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags &= ~VB2_CONTEXT_DEV_BOOT_ALLOWED;
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(vb2ex_get_locale_count, 10);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_TO_NORM);
	expect_function_call(vb2api_disable_developer_mode);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_REBOOT);
}

static void test_developer_ui_internal_timeout(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t start_time = mock_time_ms;

	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	EXPECT_BEEP(250, 400, start_time + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, start_time + DEV_DELAY_BEEP2_MS);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_SUCCESS);

	ASSERT_TIME_RANGE(mock_time_ms, start_time + DEV_DELAY_NORMAL_MS);
}

static void test_developer_ui_internal_fail_no_disk(void **state)
{
	struct ui_context *ui = *state;

	SETUP_VB_TRY_LOAD_KERNEL(VB2_ERROR_LK_NO_DISK_FOUND,
				 VB_DISK_FLAG_FIXED);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_NORMAL_MS);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vb2ex_developer_ui(ui->ctx),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_select_internal_menu(void **state)
{
	struct ui_context *ui = *state;

	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vb2ex_developer_ui(ui->ctx),
			 VB2_SUCCESS);
}

static void test_developer_ui_select_internal_keyboard(void **state)
{
	struct ui_context *ui = *state;

	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_INTERNAL, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));
}

static void test_developer_ui_select_internal_button(void **state)
{
	if (!CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	WILL_PRESS_KEY(UI_BUTTON_VOL_DOWN_LONG_PRESS, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));
}

static void test_developer_ui_external_disallowed_default_boot(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_NORMAL_MS);
	will_return_always(vb2api_get_dev_default_boot_target,
			   VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_external_timeout(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t start_time = mock_time_ms;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	EXPECT_BEEP(250, 400, start_time + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, start_time + DEV_DELAY_BEEP2_MS);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_always(vb2api_get_dev_default_boot_target,
			   VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));

	ASSERT_TIME_RANGE(mock_time_ms, start_time + DEV_DELAY_NORMAL_MS);
}

static void test_developer_ui_external_fail_no_disk(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_NORMAL_MS);
	SETUP_VB_TRY_LOAD_KERNEL(VB2_ERROR_LK_NO_DISK_FOUND,
				 VB_DISK_FLAG_REMOVABLE);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_select_external_keyboard(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_EXTERNAL, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));
}

static void test_developer_ui_select_external_menu(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS,
				 VB_DISK_FLAG_REMOVABLE);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));
}

static void test_developer_ui_select_external_button(void **state)
{
	if (!CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	WILL_PRESS_KEY(UI_BUTTON_VOL_UP_LONG_PRESS, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));
}

static void test_developer_ui_select_altfw_keyboard(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED;
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	EXPECT_BEEP(250, 400, mock_time_ms);
	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_ALTFW, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2ex_get_altfw_count, 2);
	expect_any(vb2ex_run_altfw, altfw_id);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_select_altfw_keyboard_disallowed(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	EXPECT_BEEP(250, 400, mock_time_ms);
	WILL_PRESS_KEY(UI_KEY_DEV_BOOT_ALTFW, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_select_to_norm(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	expect_function_call(vb2api_disable_developer_mode);
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_REBOOT);
}

static void test_developer_ui_select_to_norm_cancel(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_select_to_norm_keyboard(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	expect_function_call(vb2api_disable_developer_mode);
	WILL_PRESS_KEY(UI_KEY_DEV_TO_NORM, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_REBOOT);
}

static void test_developer_ui_to_norm_dev_forced_by_gbb(void **state)
{
	struct ui_context *ui = *state;

	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	EXPECT_BEEP(250, 400, mock_time_ms);
	WILL_PRESS_KEY(UI_KEY_DEV_TO_NORM, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags,
			  VB2_GBB_FLAG_FORCE_DEV_SWITCH_ON);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_ui_short_delay(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t start_time = mock_time_ms;

	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags,
			  VB2_GBB_FLAG_DEV_SCREEN_SHORT_DELAY);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));

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
	EXPECT_DISPLAY_UI_ANY_ALWAYS();

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);

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
	EXPECT_DISPLAY_UI_ANY_ALWAYS();

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);

	assert_true(mock_time_ms > start_time + DEV_DELAY_SHORT_MS + FUZZ_MS);
}

static void test_developer_screen_default_select_item_internal(void **state)
{
	struct ui_context *ui = *state;

	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 2);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));
}

static void test_developer_screen_default_select_item_external(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 3);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));
}

static void test_developer_screen_disabled_and_hidden_altfw(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED;
	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x0);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));
}

static void test_developer_screen_disabled_and_hidden_force_dev(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED;
	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x2);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags,
			  VB2_GBB_FLAG_FORCE_DEV_SWITCH_ON);
	will_return_maybe(ui_keyboard_read, 0);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));
}

static void test_developer_screen_disabled_and_hidden_only_altfw(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags &= ~VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED;
	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP1_MS);
	EXPECT_BEEP(250, 400, mock_time_ms + DEV_DELAY_BEEP2_MS);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x8);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_keyboard_read, 0);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));
}

static void test_developer_screen(void **state)
{
	struct ui_context *ui = *state;

	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_FIXED);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(vb2ex_get_locale_count, 10);

	EXPECT_DISPLAY_UI_ANY();
	/* #0: Language menu */
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_LANGUAGE_SELECT);
	/* #1: Return to secure mode */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 1);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_TO_NORM);
	/* #2: Boot internal */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 2);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));
}

static void test_developer_screen_external_default(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	SETUP_VB_TRY_LOAD_KERNEL(VB2_SUCCESS, VB_DISK_FLAG_REMOVABLE);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(vb2ex_get_locale_count, 10);

	/* #3: Boot external */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 3);

	ASSERT_VB2_SUCCESS(vb2ex_developer_ui(ui->ctx));
}

static void test_developer_screen_altfw(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED;
	will_return_maybe(vb2ex_prepare_log_screen, 1);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(vb2ex_get_locale_count, 10);

	/* #4: Alternate boot */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI_ANY_ALWAYS();
	will_return_maybe(ui_keyboard_read, 0);

	expect_any(vb2ex_prepare_log_screen, str);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_screen_advanced_options(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	EXPECT_DISPLAY_UI_ANY();
	/* #5: Advanced options */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 5);
	EXPECT_DISPLAY_UI(VB2_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x2);
	/* End of menu */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0); /* Blocked */
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_MODE, MOCK_IGNORE, 6);

	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_screen_advanced_options_screen(void **state)
{
	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(vb2ex_get_locale_count, 10);
	expect_any_always(vb2ex_prepare_log_screen, str);
	will_return_maybe(vb2ex_prepare_log_screen, 1);

	EXPECT_DISPLAY_UI_ANY();
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI_ANY();
	/* #0: Language menu */
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_LANGUAGE_SELECT);
	/* #1: (Hidden) */
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
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEVELOPER_MODE);
	/* End of menu */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_DISPLAY_UI(VB2_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 2);

	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_developer_screen_debug_info(void **state)
{
	struct ui_context *ui = *state;

	WILL_PRESS_KEY('\t', 0);
	EXPECT_DISPLAY_UI_ANY();
	EXPECT_DISPLAY_UI(VB2_SCREEN_DEBUG_INFO);
	will_return_maybe(vb2api_get_dev_default_boot_target,
			  VB2_DEV_DEFAULT_BOOT_TARGET_INTERNAL);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	expect_any_always(vb2ex_prepare_log_screen, str);
	will_return_maybe(vb2ex_prepare_log_screen, 1);
	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vb2ex_developer_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

#define UI_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup_context)

int main(void)
{
	const struct CMUnitTest tests[] = {
		/* Developer actions */
		UI_TEST(test_developer_ui_shutdown_timeout),
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
		UI_TEST(test_developer_ui_select_external_menu),
		UI_TEST(test_developer_ui_select_external_button),
		UI_TEST(test_developer_ui_select_altfw_keyboard),
		UI_TEST(test_developer_ui_select_altfw_keyboard_disallowed),
		UI_TEST(test_developer_ui_select_to_norm),
		UI_TEST(test_developer_ui_select_to_norm_cancel),
		UI_TEST(test_developer_ui_select_to_norm_keyboard),
		UI_TEST(test_developer_ui_to_norm_dev_forced_by_gbb),
		UI_TEST(test_developer_ui_short_delay),
		UI_TEST(test_developer_ui_stop_timer_on_input_normal_delay),
		UI_TEST(test_developer_ui_stop_timer_on_input_short_delay),
		/* Developer screens */
		UI_TEST(test_developer_screen_default_select_item_internal),
		UI_TEST(test_developer_screen_default_select_item_external),
		UI_TEST(test_developer_screen_disabled_and_hidden_altfw),
		UI_TEST(test_developer_screen_disabled_and_hidden_force_dev),
		UI_TEST(test_developer_screen_disabled_and_hidden_only_altfw),
		UI_TEST(test_developer_screen),
		UI_TEST(test_developer_screen_external_default),
		UI_TEST(test_developer_screen_altfw),
		UI_TEST(test_developer_screen_advanced_options),
		UI_TEST(test_developer_screen_advanced_options_screen),
		UI_TEST(test_developer_screen_debug_info),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
