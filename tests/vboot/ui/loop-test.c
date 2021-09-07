// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/ui/common.h>
#include <tests/vboot/ui/mock_screens.h>
#include <mocks/callbacks.h>
#include <vboot/ui.h>
#include <vboot_api.h>
#include <vboot/ui/loop.c>

/* Mock functions */
int ui_is_power_pressed(void)
{
	return mock();
}

int ui_is_lid_open(void)
{
	return mock();
}

/* Tests */
struct ui_context test_ui_ctx;

static vb2_error_t mock_action_msleep(struct ui_context *ui)
{
	vb2ex_msleep(mock());
	return VB2_SUCCESS;
}

static vb2_error_t mock_action_screen_change(struct ui_context *ui)
{
	return ui_screen_change(ui, MOCK_SCREEN_BASE);
}

static int setup_common(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
	mock_time_ms = 31ULL * MSECS_PER_SEC;
	*state = &test_ui_ctx;
	return 0;
}

static void test_shutdown_detachable_ignore_power_button(void **state)
{
	if (!CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	will_return_maybe(ui_is_power_pressed, 1);
	will_return_maybe(ui_is_lid_open, 1);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	ASSERT_VB2_SUCCESS(check_shutdown_request(ui));
	ASSERT_VB2_SUCCESS(check_shutdown_request(ui));
}

static void test_shutdown_detachable_ignore_power_button_press(void **state)
{
	if (!CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	will_return_maybe(ui_is_power_pressed, 0);
	will_return_maybe(ui_is_lid_open, 1);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	ui->key = UI_BUTTON_POWER_SHORT_PRESS;

	ASSERT_VB2_SUCCESS(check_shutdown_request(ui));
}

static void test_shutdown_release_press_hold_release(void **state)
{
	if (CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	will_return_maybe(ui_is_lid_open, 1);
	will_return(ui_is_power_pressed, 0);
	will_return(ui_is_power_pressed, 1);
	will_return(ui_is_power_pressed, 1);
	will_return_always(ui_is_power_pressed, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	ASSERT_VB2_SUCCESS(check_shutdown_request(ui));
	ASSERT_VB2_SUCCESS(check_shutdown_request(ui));
	ASSERT_VB2_SUCCESS(check_shutdown_request(ui));
	assert_int_equal(check_shutdown_request(ui), VB2_REQUEST_SHUTDOWN);
}

static void test_shutdown_press_ignored_if_held_since_boot(void **state)
{
	if (CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	will_return_always(ui_is_power_pressed, 1);
	will_return_maybe(ui_is_lid_open, 1);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	ASSERT_VB2_SUCCESS(check_shutdown_request(ui));
	ASSERT_VB2_SUCCESS(check_shutdown_request(ui));
}

static void test_shutdown_power_button_short_press_from_key(void **state)
{
	if (CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	will_return_maybe(ui_is_power_pressed, 0);
	will_return_maybe(ui_is_lid_open, 1);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	ui->key = UI_BUTTON_POWER_SHORT_PRESS;

	assert_int_equal(check_shutdown_request(ui), VB2_REQUEST_SHUTDOWN);
}

static void test_shutdown_button_short_pressed_when_lid_ignored(void **state)
{
	if (CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	will_return_maybe(ui_is_power_pressed, 0);
	will_return_maybe(ui_is_lid_open, 0);
	will_return_always(vb2api_gbb_get_flags,
			   VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN);
	ui->key = UI_BUTTON_POWER_SHORT_PRESS;

	assert_int_equal(check_shutdown_request(ui), VB2_REQUEST_SHUTDOWN);
}

static void test_shutdown_button_while_lid_ignored_by_gbb(void **state)
{
	if (CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	will_return_maybe(ui_is_lid_open, 0);
	will_return(ui_is_power_pressed, 0);
	will_return(ui_is_power_pressed, 1);
	will_return_maybe(ui_is_power_pressed, 0);
	will_return_always(vb2api_gbb_get_flags,
			   VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN);

	ASSERT_VB2_SUCCESS(check_shutdown_request(ui));
	ASSERT_VB2_SUCCESS(check_shutdown_request(ui));
	assert_int_equal(check_shutdown_request(ui), VB2_REQUEST_SHUTDOWN);
}

static void test_shutdown_if_lid_closure(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(ui_is_power_pressed, 0);
	will_return_always(ui_is_lid_open, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);

	assert_int_equal(check_shutdown_request(ui), VB2_REQUEST_SHUTDOWN);

	ui->key = 'A';

	assert_int_equal(check_shutdown_request(ui), VB2_REQUEST_SHUTDOWN);
}

static void test_shutdown_lid_ignored_by_gbb_flags(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(ui_is_power_pressed, 0);
	will_return_maybe(ui_is_lid_open, 0);
	will_return_always(vb2api_gbb_get_flags,
			   VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN);

	ASSERT_VB2_SUCCESS(check_shutdown_request(ui));
}

static void test_loop_die_if_no_screen(void **state)
{
	struct ui_context *ui = *state;

	expect_die(ui_loop(ui->ctx, MOCK_SCREEN_INVALID, NULL));
}

static void test_loop_shutdown_if_requested(void **state)
{
	struct ui_context *ui = *state;

	will_return_always(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_CLOSE_LID_IN(10);
	will_return_maybe(ui_is_power_pressed, 0);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_BASE);

	assert_int_equal(ui_loop(ui->ctx, MOCK_SCREEN_BASE, NULL),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_loop_screen_action_request_ui_exit(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(ui_is_power_pressed, 0);
	will_return_maybe(ui_is_lid_open, 1);
	will_return_always(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_MOCK_ACTION_COUNTDOWN(10);
	EXPECT_UI_DISPLAY_ANY();

	ASSERT_VB2_SUCCESS(ui_loop(ui->ctx, MOCK_SCREEN_ACTION, NULL));
}

static void test_loop_global_action_request_ui_exit(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(ui_is_power_pressed, 0);
	will_return_maybe(ui_is_lid_open, 1);
	will_return_always(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_MOCK_ACTION_COUNTDOWN(10);
	EXPECT_UI_DISPLAY_ANY();

	ASSERT_VB2_SUCCESS(
		ui_loop(ui->ctx, MOCK_SCREEN_BLANK, mock_action_countdown));
}

static void test_loop_global_action_can_change_screen(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_always(ui_keyboard_read, 0);
	WILL_CLOSE_LID_IN(10);
	will_return_maybe(ui_is_power_pressed, 0);
	EXPECT_UI_DISPLAY_ANY();
	EXPECT_UI_DISPLAY(MOCK_SCREEN_BASE);

	assert_int_equal(
		ui_loop(ui->ctx, MOCK_SCREEN_BLANK, mock_action_screen_change),
		VB2_REQUEST_SHUTDOWN);
}

static void test_loop_screen_action_success(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(ui_is_power_pressed, 0);
	will_return_maybe(ui_is_lid_open, 1);
	will_return_always(mock_action_flag0, VB2_REQUEST_UI_EXIT);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_always(ui_keyboard_read, 0);
	EXPECT_UI_DISPLAY_ANY();

	ASSERT_VB2_SUCCESS(
		ui_loop(ui->ctx, MOCK_SCREEN_ALL_ACTION, mock_action_flag2));
}

static void test_loop_item_target_action_success(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(ui_is_power_pressed, 0);
	will_return_maybe(ui_is_lid_open, 1);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return_maybe(mock_action_flag0, VB2_SUCCESS);
	will_return(mock_action_flag1, VB2_REQUEST_UI_EXIT);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();

	ASSERT_VB2_SUCCESS(
		ui_loop(ui->ctx, MOCK_SCREEN_ALL_ACTION, mock_action_flag2));
}

static void test_loop_global_action_success(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(ui_is_power_pressed, 0);
	will_return_maybe(ui_is_lid_open, 1);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return(mock_action_flag0, VB2_SUCCESS);
	will_return(mock_action_flag1, VB2_SUCCESS);
	will_return(mock_action_flag2, VB2_REQUEST_UI_EXIT);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	EXPECT_UI_DISPLAY_ANY();

	ASSERT_VB2_SUCCESS(
		ui_loop(ui->ctx, MOCK_SCREEN_ALL_ACTION, mock_action_flag2));
}

static void test_loop_navigation(void **state)
{
	struct ui_context *ui = *state;

	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_CLOSE_LID_IN(11);
	will_return_maybe(ui_is_power_pressed, 0);
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_UP, 0); /* (blocked) */
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0);
	WILL_PRESS_KEY(UI_KEY_DOWN, 0); /* (blocked) */
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_UP, 0);
	WILL_PRESS_KEY(UI_KEY_ENTER, 0);
	will_return_always(ui_keyboard_read, 0);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 1);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 0);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 1);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 2);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 3);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 4);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 3);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 2);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_TARGET2);

	assert_int_equal(ui_loop(ui->ctx, MOCK_SCREEN_MENU, NULL),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_loop_detachable_navigation(void **state)
{
	if (!CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_CLOSE_LID_IN(11);
	will_return_maybe(ui_is_power_pressed, 0);
	WILL_PRESS_KEY(UI_BUTTON_VOL_UP_SHORT_PRESS, 0);
	WILL_PRESS_KEY(UI_BUTTON_VOL_UP_SHORT_PRESS, 0);
	WILL_PRESS_KEY(UI_BUTTON_VOL_DOWN_SHORT_PRESS, 0);
	WILL_PRESS_KEY(UI_BUTTON_VOL_DOWN_SHORT_PRESS, 0);
	WILL_PRESS_KEY(UI_BUTTON_VOL_DOWN_SHORT_PRESS, 0);
	WILL_PRESS_KEY(UI_BUTTON_VOL_DOWN_SHORT_PRESS, 0);
	WILL_PRESS_KEY(UI_BUTTON_VOL_DOWN_SHORT_PRESS, 0);
	WILL_PRESS_KEY(UI_BUTTON_VOL_UP_SHORT_PRESS, 0);
	WILL_PRESS_KEY(UI_BUTTON_VOL_UP_SHORT_PRESS, 0);
	WILL_PRESS_KEY(UI_BUTTON_POWER_SHORT_PRESS, 0);
	will_return_always(ui_keyboard_read, 0);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 1);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 0);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 1);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 2);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 3);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 4);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 3);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_MENU, MOCK_IGNORE, 2);
	EXPECT_UI_DISPLAY(MOCK_SCREEN_TARGET2);

	assert_int_equal(ui_loop(ui->ctx, MOCK_SCREEN_MENU, NULL),
			 VB2_REQUEST_SHUTDOWN);
}

static void test_loop_delay_sleep_20_ms(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t mock_time_start_ms = mock_time_ms;

	will_return(mock_action_msleep, 0);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_CLOSE_LID_IN(2);
	will_return_maybe(ui_is_power_pressed, 0);
	EXPECT_UI_DISPLAY_ANY();

	assert_int_equal(ui_loop(ui->ctx, MOCK_SCREEN_BASE, mock_action_msleep),
			 VB2_REQUEST_SHUTDOWN);
	assert_int_equal(mock_time_ms - mock_time_start_ms, UI_KEY_DELAY_MS);
}

static void test_loop_delay_complement_to_20_ms(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t mock_time_start_ms = mock_time_ms;

	will_return(mock_action_msleep, UI_KEY_DELAY_MS / 2);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_CLOSE_LID_IN(2);
	will_return_maybe(ui_is_power_pressed, 0);
	EXPECT_UI_DISPLAY_ANY();

	assert_int_equal(ui_loop(ui->ctx, MOCK_SCREEN_BASE, mock_action_msleep),
			 VB2_REQUEST_SHUTDOWN);
	assert_int_equal(mock_time_ms - mock_time_start_ms, UI_KEY_DELAY_MS);
}

static void test_loop_delay_no_sleep_if_time_too_long(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t mock_time_start_ms = mock_time_ms;

	will_return_always(mock_action_msleep, 1234);
	will_return_always(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_CLOSE_LID_IN(2);
	will_return_maybe(ui_is_power_pressed, 0);
	EXPECT_UI_DISPLAY_ANY();

	assert_int_equal(ui_loop(ui->ctx, MOCK_SCREEN_BASE, mock_action_msleep),
			 VB2_REQUEST_SHUTDOWN);
	assert_int_equal(mock_time_ms - mock_time_start_ms, 1234);
}

static void test_loop_delay_overflow_sleep_20_ms(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t mock_time_start_ms = mock_time_ms = UINT32_MAX;

	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	will_return(mock_action_msleep, 0);
	WILL_CLOSE_LID_IN(2);
	will_return_maybe(ui_is_power_pressed, 0);
	EXPECT_UI_DISPLAY_ANY();

	assert_int_equal(ui_loop(ui->ctx, MOCK_SCREEN_BASE, mock_action_msleep),
			 VB2_REQUEST_SHUTDOWN);
	assert_int_equal(mock_time_ms - mock_time_start_ms, UI_KEY_DELAY_MS);
}

static void test_loop_delay_overflow_complement_to_20_ms(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t mock_time_start_ms = mock_time_ms = UINT32_MAX;

	will_return(mock_action_msleep, UI_KEY_DELAY_MS / 2);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_CLOSE_LID_IN(2);
	will_return_maybe(ui_is_power_pressed, 0);
	EXPECT_UI_DISPLAY_ANY();

	assert_int_equal(ui_loop(ui->ctx, MOCK_SCREEN_BASE, mock_action_msleep),
			 VB2_REQUEST_SHUTDOWN);
	assert_int_equal(mock_time_ms - mock_time_start_ms, UI_KEY_DELAY_MS);
}

static void test_loop_delay_overflow_no_sleep_if_time_too_long(void **state)
{
	struct ui_context *ui = *state;
	const uint32_t mock_time_start_ms = mock_time_ms = UINT32_MAX;

	will_return(mock_action_msleep, 1234);
	will_return_maybe(ui_keyboard_read, 0);
	will_return_maybe(vb2api_gbb_get_flags, 0);
	WILL_CLOSE_LID_IN(2);
	will_return_maybe(ui_is_power_pressed, 0);
	EXPECT_UI_DISPLAY_ANY();

	assert_int_equal(ui_loop(ui->ctx, MOCK_SCREEN_BASE, mock_action_msleep),
			 VB2_REQUEST_SHUTDOWN);
	assert_int_equal(mock_time_ms - mock_time_start_ms, 1234);
}

#define UI_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup_common)

int main(void)
{
	const struct CMUnitTest tests[] = {
		UI_TEST(test_shutdown_detachable_ignore_power_button),
		UI_TEST(test_shutdown_detachable_ignore_power_button_press),
		UI_TEST(test_shutdown_release_press_hold_release),
		UI_TEST(test_shutdown_press_ignored_if_held_since_boot),
		UI_TEST(test_shutdown_power_button_short_press_from_key),
		UI_TEST(test_shutdown_button_short_pressed_when_lid_ignored),
		UI_TEST(test_shutdown_button_while_lid_ignored_by_gbb),
		UI_TEST(test_shutdown_if_lid_closure),
		UI_TEST(test_shutdown_lid_ignored_by_gbb_flags),

		UI_TEST(test_loop_die_if_no_screen),
		UI_TEST(test_loop_shutdown_if_requested),
		UI_TEST(test_loop_screen_action_request_ui_exit),
		UI_TEST(test_loop_global_action_request_ui_exit),
		UI_TEST(test_loop_global_action_can_change_screen),
		UI_TEST(test_loop_screen_action_success),
		UI_TEST(test_loop_item_target_action_success),
		UI_TEST(test_loop_global_action_success),
		UI_TEST(test_loop_navigation),
		UI_TEST(test_loop_detachable_navigation),

		UI_TEST(test_loop_delay_sleep_20_ms),
		UI_TEST(test_loop_delay_complement_to_20_ms),
		UI_TEST(test_loop_delay_no_sleep_if_time_too_long),
		UI_TEST(test_loop_delay_overflow_sleep_20_ms),
		UI_TEST(test_loop_delay_overflow_complement_to_20_ms),
		UI_TEST(test_loop_delay_overflow_no_sleep_if_time_too_long),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
