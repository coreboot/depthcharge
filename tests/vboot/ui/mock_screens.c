// SPDX-License-Identifier: GPL-2.0

#include <vboot/ui.h>
#include <tests/test.h>
#include <tests/vboot/ui/mock_screens.h>

const struct ui_screen_info mock_screen_blank = {
	.id = MOCK_SCREEN_BLANK,
	.name = "mock_screen_blank",
};
struct ui_screen_info mock_screen_base = {
	.id = MOCK_SCREEN_BASE,
	.name = "mock_screen_base: menuless screen",
};
const struct ui_menu_item mock_screen_menu_items[] = {
	{
		.name = "item 0",
		.type = UI_MENU_ITEM_TYPE_LANGUAGE,
		.target = MOCK_SCREEN_TARGET0,
	},
	{
		.name = "item 1",
		.target = MOCK_SCREEN_TARGET1,
	},
	{
		.name = "item 2",
		.target = MOCK_SCREEN_TARGET2,
	},
	{
		.name = "item 3",
		.action = mock_action_base,
	},
	{
		.name = "item 4 (no target)",
	},
};
const struct ui_screen_info mock_screen_menu = {
	.id = MOCK_SCREEN_MENU,
	.name = "mock_screen_menu: screen with 5 items",
	.menu = {
		.num_items = ARRAY_SIZE(mock_screen_menu_items),
		.items = mock_screen_menu_items,
	},
};
const struct ui_screen_info mock_screen_target0 = {
	.id = MOCK_SCREEN_TARGET0,
	.name = "mock_screen_target0",
};
const struct ui_screen_info mock_screen_target1 = {
	.id = MOCK_SCREEN_TARGET1,
	.name = "mock_screen_target1",
};
const struct ui_screen_info mock_screen_target2 = {
	.id = MOCK_SCREEN_TARGET2,
	.name = "mock_screen_target2",
};
const struct ui_screen_info mock_screen_action = {
	.id = MOCK_SCREEN_ACTION,
	.name = "mock_screen_action",
	.action = mock_action_countdown,
};
const struct ui_menu_item mock_screen_all_action_items[] = {
	{
		.name = "all_action_screen_item",
		.action = mock_action_flag1,
	},
};
const struct ui_screen_info mock_screen_all_action = {
	.id = MOCK_SCREEN_ALL_ACTION,
	.name = "mock_screen_all_action",
	.action = mock_action_flag0,
	.menu = {
		.num_items = ARRAY_SIZE(mock_screen_all_action_items),
		.items = mock_screen_all_action_items,
	},
};
const struct ui_menu_item mock_screen_hook0_items[] = {
	{
		.name = "goto hook1",
		.target = MOCK_SCREEN_HOOK1,
	},
};
const struct ui_screen_info mock_screen_hook0 = {
	.id = MOCK_SCREEN_HOOK0,
	.name = "mock_screen_hook0",
	.init = mock_action_init,
	.reinit = mock_action_reinit,
	.exit = mock_action_exit,
	.menu = {
		.num_items = 1,
		.items = mock_screen_hook0_items,
	},
};
const struct ui_menu_item mock_screen_hook1_items[] = {
	{
		.name = "goto hook2",
		.target = MOCK_SCREEN_HOOK2,
	},
};
const struct ui_screen_info mock_screen_hook1 = {
	.id = MOCK_SCREEN_HOOK1,
	.name = "mock_screen_hook1",
	.init = mock_action_init,
	.reinit = mock_action_reinit,
	.exit = mock_action_exit,
	.menu = {
		.num_items = 1,
		.items = mock_screen_hook1_items,
	},
};
const struct ui_screen_info mock_screen_hook2 = {
	.id = MOCK_SCREEN_HOOK2,
	.name = "mock_screen_hook2",
	.init = mock_action_init,
	.reinit = mock_action_reinit,
	.exit = mock_action_exit,
};
const struct ui_screen_info mock_screen_root = {
	.id = MOCK_SCREEN_ROOT,
};

vb2_error_t mock_action_countdown(struct ui_context *ui)
{
	return mock_type(vb2_error_t);
}

vb2_error_t mock_action_base(struct ui_context *ui)
{
	function_called();
	return VB2_SUCCESS;
}

vb2_error_t mock_action_flag0(struct ui_context *ui)
{
	return mock_type(vb2_error_t);
}

vb2_error_t mock_action_flag1(struct ui_context *ui)
{
	return mock_type(vb2_error_t);
}

vb2_error_t mock_action_flag2(struct ui_context *ui)
{
	return mock_type(vb2_error_t);
}

void check_mock_hook(enum mock_screen screen_id, const char *hook_name)
{
	check_expected(screen_id);
	check_expected(hook_name);
}

vb2_error_t mock_action_init(struct ui_context *ui)
{
	check_mock_hook(ui->state->screen->id, _MOCK_HOOK_INIT);
	return mock_type(vb2_error_t);
}

vb2_error_t mock_action_reinit(struct ui_context *ui)
{
	check_mock_hook(ui->state->screen->id, _MOCK_HOOK_REINIT);
	return mock_type(vb2_error_t);
}

vb2_error_t mock_action_exit(struct ui_context *ui)
{
	check_mock_hook(ui->state->screen->id, _MOCK_HOOK_EXIT);
	return mock_type(vb2_error_t);
}

static const struct ui_screen_info *screens[] = {
	&mock_screen_blank,
	&mock_screen_base,
	&mock_screen_menu,
	&mock_screen_target0,
	&mock_screen_target1,
	&mock_screen_target2,
	&mock_screen_action,
	&mock_screen_all_action,
	&mock_screen_hook0,
	&mock_screen_hook1,
	&mock_screen_hook2,
	&mock_screen_root,
};

const struct ui_screen_info *ui_get_screen_info(enum ui_screen screen)
{
	for (int i = 0; i < ARRAY_SIZE(screens); ++i) {
		if (screens[i]->id == screen)
			return screens[i];
	}

	if (screen == (enum ui_screen)MOCK_SCREEN_INVALID)
		return NULL;

	fail_msg("%s: screen %#x not found", __func__, screen);

	/* Never reach here */
	return NULL;
}
