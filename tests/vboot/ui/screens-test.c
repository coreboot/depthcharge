// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/common.h>
#include <vboot/ui/screens.c>

struct ui_context test_ui_ctx;

static int setup_ui_context(void **state)
{
	memset(&test_ui_ctx, 0, sizeof(test_ui_ctx));
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

#define UI_TEST(test_function_name) \
	cmocka_unit_test_setup(test_function_name, setup_ui_context)

int main(void)
{
	const struct CMUnitTest tests[] = {
		UI_TEST(test_language_menu_allocate_once),
		UI_TEST(test_language_menu_locale_count_0),
	};
	return cmocka_run_group_tests(tests, NULL, NULL);
}
