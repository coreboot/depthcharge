// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/common.h>
#include <mocks/util/commonparams.h>
#include <vboot/ui.h>
#include <vboot/ui/screens.c>

/* Mock functions */
uint32_t VbExIsShutdownRequested(void) { return mock_type(uint32_t); }

/* Tests */
struct ui_context test_ui_ctx;
struct ui_state test_ui_state;

static int setup_ui_context(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
	memset(&test_ui_state, 0, sizeof(test_ui_state));
	reset_mock_workbuf = 1;

	test_ui_ctx.ctx = vboot_get_context();
	test_ui_ctx.state = &test_ui_state;

	test_ui_ctx.ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALLOWED;
	test_ui_ctx.ctx->flags |= VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED;

	*state = &test_ui_ctx;
	return 0;
}

static void test_language_menu_allocate_once(void **state)
{
	struct ui_context *ui = *state;
	const struct ui_menu *menu;
	const struct ui_menu_item *items;

	will_return(vb2ex_get_locale_count, 7);

	menu = get_language_menu(ui);
	assert_non_null(menu);
	assert_int_equal(menu->num_items, 7);
	assert_non_null(menu->items);
	items = menu->items;

	menu = get_language_menu(ui);
	assert_non_null(menu);
	assert_int_equal(menu->num_items, 7);
	assert_ptr_equal(menu->items, items);
}

static void test_language_menu_locale_count_0(void **state)
{
	struct ui_context *ui = *state;
	const struct ui_menu *menu;

	will_return(vb2ex_get_locale_count, 0);
	menu = get_language_menu(ui);
	assert_non_null(menu);
	assert_int_equal(menu->num_items, 1);
}

static void test_dev_altfw_action_default_bootloader(void **state)
{

	struct ui_context *ui = *state;

	ui->ctx->flags |= VB2_CONTEXT_DEVELOPER_MODE;
	ui->ctx->flags |= VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED;
	expect_value(vb2ex_run_altfw, altfw_id, 0);
	will_return(vb2ex_run_altfw, VB2_SUCCESS);
	will_return_maybe(vb2ex_get_altfw_count, 2);

	expect_assert_failure(ui_developer_mode_boot_altfw_action(ui));
}

#define UI_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup_ui_context)

int main(void)
{
	const struct CMUnitTest tests[] = {
		/* Get language menu */
		UI_TEST(test_language_menu_allocate_once),
		UI_TEST(test_language_menu_locale_count_0),
		/* Boot from alternate bootloader */
		UI_TEST(test_dev_altfw_action_default_bootloader),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
