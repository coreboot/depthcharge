// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/common.h>
#include <tests/vboot/ui/common.h>
#include <tests/vboot/ui/mock_screens.h>
#include <vboot/ui.h>
#include <vboot_api.h>

struct ui_context test_ui_ctx;
struct ui_state test_ui_state;

static int setup_common(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
	*state = &test_ui_ctx;
	return 0;
}

static vb2_error_t try_back_helper(struct ui_context *ui)
{
	VB2_TRY(ui_screen_back(ui));
	return VB2_ERROR_MOCK;
}

static vb2_error_t try_change_helper(struct ui_context *ui,
				     enum ui_screen screen_id)
{
	VB2_TRY(ui_screen_change(ui, screen_id));
	return VB2_ERROR_MOCK;
}

static void test_screen_change_to_non_exist_screen(void **state)
{
	struct ui_context *ui = *state;

	assert_int_equal(ui_screen_change(ui, MOCK_SCREEN_INVALID),
			 VB2_REQUEST_UI_CONTINUE);
	assert_null(ui->state);
}

static void test_screen_back_with_empty_stack(void **state)
{
	struct ui_context *ui = *state;

	assert_int_equal(ui_screen_back(ui), VB2_REQUEST_UI_CONTINUE);
	assert_null(ui->state);
}

static void test_screen_back_to_previous_restore_state(void **state)
{
	struct ui_context *ui = *state;

	mock_screen_base.init = mock_action_base;
	expect_function_call(mock_action_base);

	ui_screen_change(ui, MOCK_SCREEN_ROOT);
	ui_screen_change(ui, MOCK_SCREEN_BASE);
	ui->state->selected_item = 2;
	ui->state->hidden_item_mask = 0x10;
	ui_screen_change(ui, MOCK_SCREEN_MENU);

	assert_int_equal(ui_screen_back(ui), VB2_REQUEST_UI_CONTINUE);
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_BASE, 2, 0x10);
}

/**
 * Screen change functions wrapped by VB2_TRY should return right away.
 * Check CL:2721377.
 */
static void test_screen_back_with_vb2_try(void **state)
{
	struct ui_context *ui = *state;

	assert_int_not_equal(try_back_helper(ui), VB2_ERROR_MOCK);
}

static void test_screen_change_with_vb2_try(void **state)
{
	struct ui_context *ui = *state;

	assert_int_not_equal(try_change_helper(ui, MOCK_SCREEN_ROOT),
			     VB2_ERROR_MOCK);
}

static void test_screen_change_to_state_in_stack(void **state)
{
	struct ui_context *ui = *state;

	mock_screen_base.init = mock_action_base;
	expect_function_call(mock_action_base);

	ui_screen_change(ui, MOCK_SCREEN_ROOT);
	ui_screen_change(ui, MOCK_SCREEN_BASE);
	ui->state->selected_item = 2;
	ui->state->hidden_item_mask = 0x10;
	ui_screen_change(ui, MOCK_SCREEN_MENU);

	assert_int_equal(ui_screen_change(ui, MOCK_SCREEN_BASE),
			 VB2_REQUEST_UI_CONTINUE);
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_BASE, 2, 0x10);
}

static void test_screen_change_screen_without_init(void **state)
{
	struct ui_context *ui = *state;

	assert_int_equal(ui_screen_change(ui, MOCK_SCREEN_MENU),
			 VB2_REQUEST_UI_CONTINUE);
	ASSERT_SCREEN_STATE(ui->state, MOCK_SCREEN_MENU,
			    1, /* Since index 0 is the language selection */
			    0);
}

#define UI_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup_common)

int main(void)
{
	const struct CMUnitTest tests[] = {
		UI_TEST(test_screen_change_to_non_exist_screen),
		UI_TEST(test_screen_back_with_empty_stack),
		UI_TEST(test_screen_back_to_previous_restore_state),
		UI_TEST(test_screen_back_with_vb2_try),
		UI_TEST(test_screen_change_with_vb2_try),
		UI_TEST(test_screen_change_to_state_in_stack),
		UI_TEST(test_screen_change_screen_without_init),
	};

	return cmocka_run_group_tests(tests, NULL, NULL);
}
