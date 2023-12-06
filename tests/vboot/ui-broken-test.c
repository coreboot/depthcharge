// SPDX-License-Identifier: GPL-2.0

#include <mocks/callbacks.h>
#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/context.h>
#include <tests/vboot/ui/common.h>
#include <vb2_api.h>
#include <vboot/stages.h>

/* Mock functions */
int ui_is_lid_open(void)
{
	return mock();
}

int ui_is_power_pressed(void)
{
	return 0;
}

int ui_is_physical_presence_pressed(void)
{
	return 0;
}

int has_external_display(void)
{
	return 0;
}

char *cbmem_console_snapshot(void)
{
	return mock_type(char *);
}

/* Tests */
struct ui_context test_ui_ctx;
struct vb2_kernel_params test_kparams;

static int setup_context(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
	reset_mock_workbuf = 1;
	test_ui_ctx.ctx = vboot_get_context();
	set_boot_mode(test_ui_ctx.ctx, VB2_BOOT_MODE_BROKEN_SCREEN);
	memset(&test_kparams, 0, sizeof(test_kparams));
	test_ui_ctx.kparams = &test_kparams;
	*state = &test_ui_ctx;
	return 0;
}

static void test_broken_ui_shortcuts_ignored(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_CLOSE_LID_IN(10);
	WILL_PRESS_KEY(UI_KEY_CTRL('D'), 1);
	WILL_PRESS_KEY(UI_KEY_CTRL('U'), 1);
	WILL_PRESS_KEY(UI_KEY_CTRL('L'), 1);
	WILL_PRESS_KEY(UI_BUTTON_VOL_UP_DOWN_COMBO_PRESS, 1);
	WILL_PRESS_KEY(UI_BUTTON_VOL_UP_LONG_PRESS, 1);
	WILL_PRESS_KEY(UI_BUTTON_VOL_DOWN_LONG_PRESS, 1);
	will_return_always(ui_keyboard_read, 0);
	EXPECT_UI_DISPLAY_ANY();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_broken_ui_disabled_and_hidden_item_mask(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_CLOSE_LID_IN(5);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_BROKEN, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x0);
	will_return_always(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_broken_ui_screen(void **state)
{
	struct ui_context *ui = *state;

	WILL_CLOSE_LID_IN(7);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_always(ui_get_locale_count, 10);
	will_return_maybe(vb2api_allow_recovery, 1);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_BROKEN, MOCK_IGNORE, 1);
	/* #0: Language menu */
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_BROKEN, MOCK_IGNORE, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_LANGUAGE_SELECT, MOCK_IGNORE, 0);
	/* #1: Advanced options */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_BROKEN, MOCK_IGNORE, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_BROKEN, MOCK_IGNORE, 1);
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS);
	/* End of menu */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_BROKEN, MOCK_IGNORE, 1);
	WILL_PRESS_KEY(0, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_broken_ui_power_button_shutdown(void **state)
{
	if (CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_is_lid_open, 1);
	WILL_PRESS_KEY(UI_BUTTON_POWER_SHORT_PRESS, 0);
	EXPECT_UI_DISPLAY_ANY();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_debug_info(void **state)
{
	struct ui_context *ui = *state;

	WILL_CLOSE_LID_IN(5);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	SET_LOG_DIMENSIONS(40, 20);

	WILL_PRESS_KEY(0, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_BROKEN);

	WILL_PRESS_KEY('\t', 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEBUG_INFO);

	/* Leave debug info */
	if (CONFIG(DETACHABLE))
		WILL_PRESS_KEY(UI_BUTTON_POWER_SHORT_PRESS, 0);
	else
		WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_BROKEN);

	will_return_always(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_debug_info_enter_failed(void **state)
{
	struct ui_context *ui = *state;

	WILL_CLOSE_LID_IN(5);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	SET_LOG_DIMENSIONS(0, 0);

	WILL_PRESS_KEY(0, 0);
	EXPECT_UI_DISPLAY_ANY();

	WILL_PRESS_KEY('\t', 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_BROKEN);
	EXPECT_BEEP(250, 400, mock_time_ms + 2 * UI_KEY_DELAY_MS);

	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

#define EXPECT_FIRMWARE_LOG_PAGE(page) \
	EXPECT_UI_DISPLAY(UI_SCREEN_FIRMWARE_LOG, MOCK_IGNORE, MOCK_IGNORE, \
			  MOCK_IGNORE, MOCK_IGNORE, page)

static void test_firmware_log(void **state)
{
	struct ui_context *ui = *state;
	const char *firmware_log =
		/* Page 0 */
		"Line 0\n"
		"Line 1\n"
		/* Page 1 */
		"Line 2\n"
		"Line 3\n"
		/* Page 2: containing anchor "coreboot-" */
		"coreboot-coreboot-unknown.9999.d565815 Fri Nov 03 "  // len=50
		"06:50:23 UTC 2023 aarch64 ramstage starting...\n"
		/* Page 3: containing anchor "Starting depthcharge on" */
		"Line 6\n"
		"Starting depthcharge on Geralt...\n"
		/* Page 4 */
		"Line 8";

	WILL_CLOSE_LID_IN(30);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return(cbmem_console_snapshot, firmware_log);
	/* Expect 5 pages */
	SET_LOG_DIMENSIONS(2, 50);

	WILL_PRESS_KEY(0, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_BROKEN);

	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS);

	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS);

	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_FIRMWARE_LOG);

	/* Firmware log screen */
	WILL_PRESS_KEY(UI_KEY_UP, 0);		/* still page 0 */
	WILL_PRESS_KEY(UI_KEY_UP, 0);		/* still page 0 */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_FIRMWARE_LOG_PAGE(1);

	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_FIRMWARE_LOG_PAGE(2);

	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_FIRMWARE_LOG_PAGE(3);

	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	EXPECT_FIRMWARE_LOG_PAGE(4);

	WILL_PRESS_KEY(UI_KEY_DOWN, 0);		/* still page 4 */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);		/* still page 4 */
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	EXPECT_FIRMWARE_LOG_PAGE(3);

	WILL_PRESS_KEY(UI_KEY_LEFT, 0);		/* first page */
	EXPECT_FIRMWARE_LOG_PAGE(0);

	WILL_PRESS_KEY(UI_KEY_RIGHT, 0);	/* last page */
	EXPECT_FIRMWARE_LOG_PAGE(4);

	WILL_PRESS_KEY(UI_KEY_NEXT_ANCHOR, 0);	/* still page 4 */

	WILL_PRESS_KEY(UI_KEY_PREV_ANCHOR, 0);
	EXPECT_FIRMWARE_LOG_PAGE(3);

	WILL_PRESS_KEY(UI_KEY_PREV_ANCHOR, 0);
	EXPECT_FIRMWARE_LOG_PAGE(2);

	WILL_PRESS_KEY(UI_KEY_PREV_ANCHOR, 0);	/* still page 2 */

	WILL_PRESS_KEY(UI_KEY_NEXT_ANCHOR, 0);
	EXPECT_FIRMWARE_LOG_PAGE(3);

	WILL_PRESS_KEY(UI_KEY_UP, 0);
	EXPECT_FIRMWARE_LOG_PAGE(2);

	/* page 1, back */
	if (CONFIG(DETACHABLE))
		WILL_PRESS_KEY(UI_BUTTON_POWER_SHORT_PRESS, 0);
	else
		WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_ADVANCED_OPTIONS);

	will_return_maybe(ui_keyboard_read, 0);

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_firmware_log_again_reacquire_new_one(void **state)
{
	struct ui_context *ui = *state;

	WILL_CLOSE_LID_IN(20);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_count(cbmem_console_snapshot, "mock log", 2);
	SET_LOG_DIMENSIONS(40, 20);

	/* Firmware log */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);

	/* Back */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);

	/* Firmware log again */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_firmware_log_again_not_reacquire_new_one(void **state)
{
	struct ui_context *ui = *state;

	WILL_CLOSE_LID_IN(20);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return(cbmem_console_snapshot, "mock log");
	SET_LOG_DIMENSIONS(40, 20);

	/* Firmware log */
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);

	/* Debug info */
	WILL_PRESS_KEY('\t', 0);

	/* Back to firmware log */
	WILL_PRESS_KEY(UI_KEY_ESC, 0);

	will_return_maybe(ui_keyboard_read, 0);
	EXPECT_UI_DISPLAY_ANY_ALWAYS();

	assert_int_equal(vboot_select_and_load_kernel(ui->ctx, ui->kparams),
			 VB2_REQUEST_SHUTDOWN);
}

#define UI_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup_context)

int main(void)
{
	const struct CMUnitTest tests[] = {
		/* Broken screen ui */
		UI_TEST(test_broken_ui_shortcuts_ignored),
		UI_TEST(test_broken_ui_disabled_and_hidden_item_mask),
		UI_TEST(test_broken_ui_screen),
		UI_TEST(test_broken_ui_power_button_shutdown),
		/* Debug info screen */
		UI_TEST(test_debug_info),
		UI_TEST(test_debug_info_enter_failed),
		/* Firmware log screen */
		UI_TEST(test_firmware_log),
		UI_TEST(test_firmware_log_again_reacquire_new_one),
		UI_TEST(test_firmware_log_again_not_reacquire_new_one),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
