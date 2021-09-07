// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/ui/common.h>
#include <mocks/callbacks.h>
#include <mocks/util/commonparams.h>
#include <vboot_api.h>
#include <vb2_api.h>
#include <vboot/util/commonparams.h>

/* Mock functions */
int ui_is_power_pressed(void)
{
	return 0;
}

int ui_is_lid_open(void)
{
	return mock();
}

/* Tests */
struct ui_context test_ui_ctx;

static int setup_context(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
	reset_mock_workbuf = 1;
	test_ui_ctx.ctx = vboot_get_context();
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

	assert_int_equal(vb2ex_broken_screen_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_broken_ui_debug_info(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_CALL_UI_LOG_INIT_ALWAYS(1);
	WILL_CLOSE_LID_IN(5);
	WILL_PRESS_KEY('\t', 0);
	will_return_always(ui_keyboard_read, 0);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_BROKEN);
	EXPECT_UI_DISPLAY(UI_SCREEN_DEBUG_INFO);
	EXPECT_UI_LOG_INIT_ANY_ALWAYS();

	assert_int_equal(vb2ex_broken_screen_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_broken_ui_disabled_and_hidden_item_mask(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_CLOSE_LID_IN(5);
	EXPECT_UI_DISPLAY(UI_SCREEN_RECOVERY_BROKEN, MOCK_IGNORE, MOCK_IGNORE,
			  0x0, 0x0);
	will_return_always(ui_keyboard_read, 0);

	assert_int_equal(vb2ex_broken_screen_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_broken_ui_screen(void **state)
{
	struct ui_context *ui = *state;

	WILL_CLOSE_LID_IN(7);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_always(vb2ex_get_locale_count, 10);
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

	assert_int_equal(vb2ex_broken_screen_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

static void test_broken_ui_power_button_shutdown(void **state)
{
	if (CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(ui_is_power_pressed, 0);
	will_return_maybe(ui_is_lid_open, 1);
	WILL_PRESS_KEY(UI_BUTTON_POWER_SHORT_PRESS, 0);
	EXPECT_UI_DISPLAY_ANY();

	assert_int_equal(vb2ex_broken_screen_ui(ui->ctx), VB2_REQUEST_SHUTDOWN);
}

#define UI_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup_context)

int main(void)
{
	const struct CMUnitTest tests[] = {
		/* Broken screen ui */
		UI_TEST(test_broken_ui_shortcuts_ignored),
		UI_TEST(test_broken_ui_debug_info),
		UI_TEST(test_broken_ui_disabled_and_hidden_item_mask),
		UI_TEST(test_broken_ui_screen),
		UI_TEST(test_broken_ui_power_button_shutdown),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
