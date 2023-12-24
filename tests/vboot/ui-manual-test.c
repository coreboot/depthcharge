// SPDX-License-Identifier: GPL-2.0

#include <mocks/callbacks.h>
#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/context.h>
#include <tests/vboot/ui/common.h>
#include <mocks/vb2api.h>
#include <vb2_api.h>
#include <vboot/stages.h>
#include <vboot/ui.c>
#include <vboot/ui.h>

/* Mock functions */
vb2_error_t ui_get_locale_info(uint32_t locale_id,
			       struct ui_locale const **locale)
{
	static struct ui_locale stub_locale;

	stub_locale.id = locale_id;
	*locale = &stub_locale;
	return mock_type(vb2_error_t);
}

uint32_t ui_get_locale_count(void)
{
	return mock_type(uint32_t);
}

int ui_is_lid_open(void)
{
	return mock();
}

int has_external_display(void)
{
	return 0;
}

/* Tests */
struct ui_context test_ui_ctx;
struct vb2_kernel_params test_kparams;

static int setup_context(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
	reset_mock_workbuf = 1;
	test_ui_ctx.ctx = vboot_get_context();
	set_boot_mode(test_ui_ctx.ctx, VB2_BOOT_MODE_MANUAL_RECOVERY);
	memset(&test_kparams, 0, sizeof(test_kparams));
	test_ui_ctx.kparams = &test_kparams;
	*state = &test_ui_ctx;

	mock_time_ms = 31ULL * MSECS_PER_SEC;

	return 0;
}

static void setup_will_return_common(void)
{
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(vb2api_diagnostic_ui_enabled, 1);
	will_return_maybe(ui_get_locale_info, VB2_SUCCESS);
}

static void test_manual_action_success(void **state)
{
	struct ui_context *ui = *state;

	WILL_LOAD_EXTERNAL_ALWAYS(VB2_SUCCESS);

	assert_int_equal(ui_manual_recovery_action(ui), VB2_REQUEST_UI_EXIT);
}

static void test_manual_action_no_disk_found(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	WILL_LOAD_EXTERNAL_ALWAYS(VB2_ERROR_LK_NO_DISK_FOUND);

	assert_int_equal(ui_manual_recovery_action(ui),
			 VB2_REQUEST_UI_CONTINUE);

	ASSERT_SCREEN_STATE(ui->state, UI_SCREEN_RECOVERY_SELECT);
}

static void test_manual_action_invalid_kernel(void **state)
{
	struct ui_context *ui = *state;

	WILL_LOAD_EXTERNAL_ALWAYS(VB2_ERROR_LK_INVALID_KERNEL_FOUND);
	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(ui_manual_recovery_action(ui),
			 VB2_REQUEST_UI_CONTINUE);

	ASSERT_SCREEN_STATE(ui->state, UI_SCREEN_RECOVERY_INVALID);
}

static void test_manual_ui_no_disk_found_invalid_kernel(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(ui_is_lid_open, 1);
	WILL_LOAD_EXTERNAL(VB2_ERROR_LK_NO_DISK_FOUND);
	WILL_LOAD_EXTERNAL(VB2_ERROR_LK_INVALID_KERNEL_FOUND);
	WILL_LOAD_EXTERNAL(VB2_SUCCESS);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}


static void test_manual_ui_invalid_kernel_no_disk_found(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(ui_is_lid_open, 1);
	WILL_LOAD_EXTERNAL(VB2_ERROR_LK_INVALID_KERNEL_FOUND);
	WILL_LOAD_EXTERNAL(VB2_ERROR_LK_NO_DISK_FOUND);
	WILL_LOAD_EXTERNAL(VB2_SUCCESS);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_manual_ui_internet_recovery_shortcut(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_INTERNET_RECOVERY, 0);
	will_return_maybe(ui_is_lid_open, 1);
	WILL_LOAD_EXTERNAL_MAYBE(VB2_ERROR_LK_NO_DISK_FOUND);
	expect_value(vboot_load_minios_kernel, minios_flags, 0);
	will_return(vboot_load_minios_kernel, VB2_SUCCESS);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_manual_ui_internet_recovery_menu(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_CLOSE_LID_IN(10);

	/* Fail to boot from MiniOS */
	WILL_PRESS_KEY(0, 0);			/* #1: External disk recovery */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);		/* #2: Internet recovery */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(ui_keyboard_read, 0);
	WILL_LOAD_EXTERNAL_MAYBE(VB2_ERROR_LK_NO_DISK_FOUND);
	expect_value(vboot_load_minios_kernel, minios_flags, 0);
	will_return(vboot_load_minios_kernel, VB2_ERROR_MOCK);
	EXPECT_BEEP(250, 400);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_internet_recovery_menu_old(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	/* Try older version from advanced options screen */
	WILL_PRESS_KEY(0, 0);			/* #1: External disk recovery */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);		/* #2: Internet recovery */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);		/* #3: Diagnostics */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);		/* #4: Advanced options */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* #1: Enable developer mode */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);		/* #2: Debug info */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);		/* #3: Firmware log*/
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);		/* #4: Internet recovery */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(ui_is_lid_open, 1);
	WILL_LOAD_EXTERNAL_MAYBE(VB2_ERROR_LK_NO_DISK_FOUND);
	expect_value(vboot_load_minios_kernel, minios_flags,
		     VB2_MINIOS_FLAG_NON_ACTIVE);
	will_return(vboot_load_minios_kernel, VB2_SUCCESS);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_manual_ui_timeout(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(ui_keyboard_read, 0);
	WILL_CLOSE_LID_IN(3);
	EXPECT_UI_DISPLAY_ANY();
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_power_button_shutdown(void **state)
{
	if (CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(ui_keyboard_read, 0);
	WILL_CLOSE_LID_IN(3);
	EXPECT_UI_DISPLAY_ANY();
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_boot_with_valid_image(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(ui_is_lid_open, 1);
	will_return_maybe(ui_keyboard_read, 0);
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_UI_DISPLAY_ANY();

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_manual_ui_boot_with_valid_image_later(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(ui_is_lid_open, 1);
	WILL_LOAD_EXTERNAL_COUNT(VB2_ERROR_LK_NO_DISK_FOUND, 2);
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_UI_DISPLAY_ANY();

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_manual_ui_boot_invalid_remove_valid(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(ui_is_lid_open, 1);
	will_return_maybe(ui_keyboard_read, 0);
	WILL_LOAD_EXTERNAL(VB2_ERROR_MOCK);
	WILL_LOAD_EXTERNAL_COUNT(VB2_ERROR_LK_NO_DISK_FOUND, 2);
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_SUCCESS);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_INVALID);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_manual_ui_keyboard_to_dev_and_cancel(void **state)
{
	if (!CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_CLOSE_LID_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(' ', 0);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_TO_DEV);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_keyboard_to_dev_and_cancel_chromebox(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_CLOSE_LID_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(' ', 0);
	will_return_maybe(ui_keyboard_read, 0);
	WILL_PRESS_PHYSICAL_PRESENCE(0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_TO_DEV);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_cancel_to_dev(void **state)
{
	if (!CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_CLOSE_LID_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(UI_KEY_DOWN, 1);
	WILL_PRESS_KEY(UI_KEY_ENTER, 1);
	will_return_maybe(ui_is_physical_presence_pressed, 0);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_cancel_to_dev_chromebox(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_CLOSE_LID_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(UI_KEY_ENTER, 1);
	will_return_maybe(ui_is_physical_presence_pressed, 0);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_confirm_to_dev(void **state)
{
	if (!CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();

	will_return_maybe(ui_is_lid_open, 1);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	if (CONFIG(DETACHABLE))
		WILL_PRESS_KEY(UI_BUTTON_POWER_SHORT_PRESS, 1);
	else
		WILL_PRESS_KEY(UI_KEY_ENTER, 1);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(ui_is_physical_presence_pressed, 0);
	expect_function_call(vb2api_enable_developer_mode);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_REBOOT_EC_TO_RO);
}

static void test_manual_ui_confirm_to_dev_chromebox(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();

	will_return_maybe(ui_is_lid_open, 1);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_PHYSICAL_PRESENCE(0);
	WILL_PRESS_PHYSICAL_PRESENCE(1);
	WILL_PRESS_PHYSICAL_PRESENCE(1);
	WILL_PRESS_PHYSICAL_PRESENCE(0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(ui_is_physical_presence_pressed, 0);
	expect_function_call(vb2api_enable_developer_mode);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_REBOOT_EC_TO_RO);
}

static void test_manual_ui_confirm_by_untrusted_fails(void **state)
{
	if (!CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_CLOSE_LID_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(ui_is_physical_presence_pressed, 0);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	EXPECT_BEEP(250, 400, mock_time_ms + 3 * UI_KEY_DELAY_MS);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_cannot_enable_dev_enabled_chromebox(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEVELOPER_MODE;
	setup_will_return_common();
	WILL_CLOSE_LID_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	EXPECT_BEEP(250, 400, mock_time_ms + 2 * UI_KEY_DELAY_MS);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_cannot_enable_dev_enabled(void **state)
{
	if (!CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEVELOPER_MODE;
	setup_will_return_common();
	WILL_CLOSE_LID_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(UI_KEY_ENTER, 1);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	EXPECT_BEEP(250, 400, mock_time_ms + 2 * UI_KEY_DELAY_MS);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_pp_button_stuck(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_CLOSE_LID_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_PHYSICAL_PRESENCE(1); /* Hold since boot */
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(ui_is_physical_presence_pressed, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_manual_ui_pp_button_stuck_press(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(ui_is_lid_open, 1);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_KEY(UI_KEY_REC_TO_DEV, 1);
	WILL_PRESS_PHYSICAL_PRESENCE(1); /* Hold since boot */
	WILL_PRESS_PHYSICAL_PRESENCE(0);
	WILL_PRESS_PHYSICAL_PRESENCE(1); /* Press again */
	WILL_PRESS_PHYSICAL_PRESENCE(0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(ui_is_physical_presence_pressed, 0);
	expect_function_call(vb2api_enable_developer_mode);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_TO_DEV);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_REBOOT_EC_TO_RO);
}

static void test_manual_ui_pp_button_cancel_enter_again(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(ui_is_lid_open, 1);
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
	will_return_maybe(ui_is_physical_presence_pressed, 0);
	expect_function_call(vb2api_enable_developer_mode);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_TO_DEV);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_TO_DEV);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_REBOOT_EC_TO_RO);
}

static void test_manual_ui_enter_diagnostics(void **state)
{
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		skip();

	struct ui_context *ui = *state;

	setup_will_return_common();
	will_return_maybe(ui_is_lid_open, 1);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	expect_function_call(vb2api_request_diagnostics);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_REBOOT);
}

static void test_manual_ui_locale_not_found(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(vb2api_diagnostic_ui_enabled, 1);
	will_return_maybe(ui_get_locale_info, VB2_ERROR_MOCK);

	will_return_maybe(ui_is_lid_open, 1);
	will_return_maybe(ui_keyboard_read, 0);
	WILL_LOAD_EXTERNAL_ALWAYS(VB2_SUCCESS);

	EXPECT_UI_DISPLAY_ANY();

	ASSERT_VB2_SUCCESS(vboot_select_and_load_kernel(ui->ctx, ui->kparams));
}

static void test_recovery_select_screen_disabled_and_hidden_mask(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_CLOSE_LID_IN(2);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x0);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_recovery_select_screen(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_CLOSE_LID_IN(20);
	will_return_maybe(ui_get_locale_count, 10);

	EXPECT_UI_DISPLAY_ANY();
	/* #0: Language menu */
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_LANGUAGE_SELECT);
	/* #1: External disk recovery */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, 1);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_DISK_STEP1);
	/* #3: Launch diagnostics */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);  /* #3: Internet recovery */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, 2);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, 3);
	/* #4: Advanced options */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, 4);
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS);
	/* End of menu */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);  /* Blocked */
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT, MOCK_IGNORE, 5);

	will_return_maybe(ui_keyboard_read, 0);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_advanced_options_screen_disabled_and_hidden_mask(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_CLOSE_LID_IN(10);
	will_return_maybe(ui_get_locale_count, 10);

	EXPECT_UI_DISPLAY_ANY();
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x20);

	will_return_maybe(ui_keyboard_read, 0);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_advanced_options_screen(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_CLOSE_LID_IN(30);
	will_return_maybe(ui_get_locale_count, 10);
	SET_LOG_DIMENSIONS(40, 20);

	EXPECT_UI_DISPLAY_ANY();
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY_ANY();
	/* #0: Language menu */
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_LANGUAGE_SELECT);
	/* #1: Enable dev mode */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 1);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_TO_DEV);
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
	/* #5: (Hidden) */
	/* #6: Back */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);  /* #4: Internet recovery */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 4);
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 6);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT);
	/* End of menu */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS, MOCK_IGNORE, 2);

	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(ui_is_physical_presence_pressed, 0);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_language_ui_change_language(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_CLOSE_LID_IN(8);
	will_return_maybe(ui_get_locale_count, 100);
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* select language */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* select locale 24 */
	will_return_maybe(ui_keyboard_read, 0);
	WILL_HAVE_NO_EXTERNAL();
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT, 23);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT, 23, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_LANGUAGE_SELECT, 23, 23);
	EXPECT_UI_DISPLAY(UI_SCREEN_LANGUAGE_SELECT, 23, 24);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT, 24);
	mock_locale_id = 23;

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);

	assert_int_equal(mock_locale_id, 24);
}

static void test_language_ui_locale_count_0(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();
	WILL_CLOSE_LID_IN(8);
	will_return_maybe(ui_get_locale_count, 0);
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* select language */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);	/* select locale 0 */
	will_return_maybe(ui_keyboard_read, 0);
	WILL_HAVE_NO_EXTERNAL();
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT, 23);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT, 23, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_LANGUAGE_SELECT, 23, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_SELECT, 0);
	mock_locale_id = 23;

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);

	assert_int_equal(mock_locale_id, 0);
}

static void test_debug_info(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	WILL_CLOSE_LID_IN(5);
	WILL_PRESS_KEY(0, 0);
	WILL_PRESS_KEY('\t', 0);
	will_return_maybe(ui_keyboard_read, 0);
	SET_LOG_DIMENSIONS(40, 20);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(UI_SCREEN_DEBUG_INFO);
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_firmware_log(void **state)
{
	struct ui_context *ui = *state;

	setup_will_return_common();

	WILL_CLOSE_LID_IN(10);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(ui_keyboard_read, 0);
	SET_LOG_DIMENSIONS(40, 20);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();
	WILL_HAVE_NO_EXTERNAL();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
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
		UI_TEST(test_manual_ui_internet_recovery_shortcut),
		UI_TEST(test_manual_ui_internet_recovery_menu),
		UI_TEST(test_manual_ui_internet_recovery_menu_old),
		UI_TEST(test_manual_ui_timeout),
		UI_TEST(test_manual_ui_power_button_shutdown),
		UI_TEST(test_manual_ui_boot_with_valid_image),
		UI_TEST(test_manual_ui_boot_with_valid_image_later),
		UI_TEST(test_manual_ui_boot_invalid_remove_valid),
		UI_TEST(test_manual_ui_keyboard_to_dev_and_cancel),
		UI_TEST(test_manual_ui_keyboard_to_dev_and_cancel_chromebox),
		UI_TEST(test_manual_ui_cancel_to_dev),
		UI_TEST(test_manual_ui_cancel_to_dev_chromebox),
		UI_TEST(test_manual_ui_confirm_to_dev),
		UI_TEST(test_manual_ui_confirm_to_dev_chromebox),
		UI_TEST(test_manual_ui_confirm_by_untrusted_fails),
		UI_TEST(test_manual_ui_cannot_enable_dev_enabled),
		UI_TEST(test_manual_ui_cannot_enable_dev_enabled_chromebox),
		UI_TEST(test_manual_ui_pp_button_stuck),
		UI_TEST(test_manual_ui_pp_button_stuck_press),
		UI_TEST(test_manual_ui_pp_button_cancel_enter_again),
		UI_TEST(test_manual_ui_enter_diagnostics),
		UI_TEST(test_manual_ui_locale_not_found),
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
		/* Firmware log screen */
		UI_TEST(test_firmware_log),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
