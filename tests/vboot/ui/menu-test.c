// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/ui/common.h>
#include <tests/vboot/ui/mock_screens.h>
#include <vboot/ui.h>
#include <vboot_api.h>
#include <vb2_api.h>

struct ui_context test_ui_ctx;
struct ui_state test_ui_state;

vb2_error_t ui_screen_change(struct ui_context *ui, enum ui_screen id)
{
	check_expected(id);
	return VB2_ERROR_MOCK;
}

static int setup_ui_context(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
	memset(&test_ui_state, 0, sizeof(test_ui_state));
	test_ui_ctx.state = &test_ui_state;
	*state = &test_ui_ctx;
	return 0;
}

void test_prev_valid_action(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 2;
	ui->key = UI_KEY_UP;

	ASSERT_VB2_SUCCESS(ui_menu_prev(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 1, MOCK_IGNORE);
}

void test_prev_valid_action_with_hidden_mask(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 2;
	ui->state->hidden_item_mask = 0x0a; /* 0b01010 */
	ui->key = UI_KEY_UP;

	ASSERT_VB2_SUCCESS(ui_menu_prev(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 0, MOCK_IGNORE);
}

void test_prev_disable_mask_does_not_affect(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 2;
	ui->state->disabled_item_mask = 0x0a; /* 0b01010 */
	ui->key = UI_KEY_UP;

	ASSERT_VB2_SUCCESS(ui_menu_prev(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 1, MOCK_IGNORE);
}

void test_prev_invalid_action_blocked(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 0;
	ui->key = UI_KEY_UP;

	ASSERT_VB2_SUCCESS(ui_menu_prev(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 0, MOCK_IGNORE);
}

void test_prev_invalid_action_blocked_by_mask(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 2;
	ui->state->hidden_item_mask = 0x0b; /* 0b01011 */
	ui->key = UI_KEY_UP;

	ASSERT_VB2_SUCCESS(ui_menu_prev(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 2, MOCK_IGNORE);
}

void test_prev_ignore_up_for_non_detachable(void **state)
{
	if (CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 2;
	ui->key = UI_BUTTON_VOL_UP_SHORT_PRESS;

	ASSERT_VB2_SUCCESS(ui_menu_prev(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 2, MOCK_IGNORE);
}

void test_next_valid_action(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 2;
	ui->key = UI_KEY_DOWN;

	ASSERT_VB2_SUCCESS(ui_menu_next(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 3, MOCK_IGNORE);
}

void test_next_valid_action_with_hidden_mask(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 2;
	ui->state->hidden_item_mask = 0x0a; /* 0b01010 */
	ui->key = UI_KEY_DOWN;

	ASSERT_VB2_SUCCESS(ui_menu_next(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 4, MOCK_IGNORE);
}

void test_next_disable_mask_does_not_affect(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 2;
	ui->state->disabled_item_mask = 0x0a; /* 0b01010 */
	ui->key = UI_KEY_DOWN;

	ASSERT_VB2_SUCCESS(ui_menu_next(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 3, MOCK_IGNORE);
}

void test_next_invalid_action_blocked(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 4;
	ui->key = UI_KEY_DOWN;

	ASSERT_VB2_SUCCESS(ui_menu_next(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 4, MOCK_IGNORE);
}

void test_next_invalid_action_blocked_by_mask(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 2;
	ui->state->hidden_item_mask = 0x1a; /* 0b11010 */
	ui->key = UI_KEY_DOWN;

	ASSERT_VB2_SUCCESS(ui_menu_next(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 2, MOCK_IGNORE);
}

void test_next_ignore_up_for_non_detachable(void **state)
{
	if (CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 2;
	ui->key = UI_BUTTON_VOL_DOWN_SHORT_PRESS;

	ASSERT_VB2_SUCCESS(ui_menu_next(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 2, MOCK_IGNORE);
}

void test_select_action_with_no_item_screen(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_base;
	ui->key = UI_KEY_ENTER;

	ASSERT_VB2_SUCCESS(ui_menu_select(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_BASE, 0, MOCK_IGNORE);
}

void test_select_item_with_target(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 2;
	ui->key = UI_KEY_ENTER;
	expect_value(ui_screen_change, id, MOCK_SCREEN_TARGET2);

	assert_int_equal(ui_menu_select(ui), VB2_ERROR_MOCK);
}

void test_select_item_with_action(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 3;
	ui->key = UI_KEY_ENTER;
	expect_function_call(mock_action_base);

	ASSERT_VB2_SUCCESS(ui_menu_select(ui));
}

void test_select_item_with_no_target_and_action(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 4;
	ui->key = UI_KEY_ENTER;

	ASSERT_VB2_SUCCESS(ui_menu_select(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 4, MOCK_IGNORE);
}

void test_select_item_disabled(void **state)
{
	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 3;
	ui->state->disabled_item_mask = 0x08; /* 0b01000 */
	ui->key = UI_KEY_ENTER;

	ASSERT_VB2_SUCCESS(ui_menu_select(ui));
}

void test_select_ignore_power_button_for_non_detachable(void **state)
{
	if (CONFIG(DETACHABLE))
		skip();

	struct ui_context *ui = *state;

	ui->state->screen = &mock_screen_menu;
	ui->state->selected_item = 1;
	ui->key = UI_BUTTON_POWER_SHORT_PRESS;

	ASSERT_VB2_SUCCESS(ui_menu_select(ui));
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU, 1, MOCK_IGNORE);
}

#define UI_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup_ui_context)

int main(void)
{
	const struct CMUnitTest tests[] = {
		UI_TEST(test_prev_valid_action),
		UI_TEST(test_prev_valid_action_with_hidden_mask),
		UI_TEST(test_prev_disable_mask_does_not_affect),
		UI_TEST(test_prev_invalid_action_blocked),
		UI_TEST(test_prev_invalid_action_blocked_by_mask),
		UI_TEST(test_prev_ignore_up_for_non_detachable),

		UI_TEST(test_next_valid_action),
		UI_TEST(test_next_valid_action_with_hidden_mask),
		UI_TEST(test_next_disable_mask_does_not_affect),
		UI_TEST(test_next_invalid_action_blocked),
		UI_TEST(test_next_invalid_action_blocked_by_mask),
		UI_TEST(test_next_ignore_up_for_non_detachable),

		UI_TEST(test_select_action_with_no_item_screen),
		UI_TEST(test_select_item_with_target),
		UI_TEST(test_select_item_with_action),
		UI_TEST(test_select_item_with_no_target_and_action),
		UI_TEST(test_select_item_disabled),
		UI_TEST(test_select_ignore_power_button_for_non_detachable),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
