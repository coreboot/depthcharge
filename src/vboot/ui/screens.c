// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>
#include <vb2_api.h>

#include "base/list.h"
#include "boot/payload.h"
#include "diag/storage_test.h"
#include "drivers/ec/cros/ec.h"
#include "vboot/ui.h"
#include "vboot/util/commonparams.h"

#define UI_DESC(a) ((struct ui_desc){	\
	.count = ARRAY_SIZE(a),		\
	.files = a,			\
})

#define UI_MENU(a) ((struct ui_menu){	\
	.num_items = ARRAY_SIZE(a),	\
	.items = a,			\
})

#define LANGUAGE_SELECT_ITEM ((struct ui_menu_item){	\
	.name = "Language selection",			\
	.file = NULL,					\
	.type = UI_MENU_ITEM_TYPE_LANGUAGE,		\
	.target = UI_SCREEN_LANGUAGE_SELECT,		\
})

#define PAGE_UP_ITEM ((struct ui_menu_item){			\
	.name = "Page up",					\
	.file = "btn_page_up.bmp",				\
	.disabled_help_text_file = "page_up_disabled_help.bmp",	\
	.action = log_page_prev_action,				\
})

#define PAGE_DOWN_ITEM ((struct ui_menu_item){				\
	.name = "Page down",						\
	.file = "btn_page_down.bmp",					\
	.disabled_help_text_file = "page_down_disabled_help.bmp",	\
	.action = log_page_next_action,					\
})

#define BACK_ITEM ((struct ui_menu_item){	\
	.name = "Back",				\
	.file = "btn_back.bmp",			\
	.action = ui_screen_back,		\
})

#define NEXT_ITEM(target_screen) ((struct ui_menu_item){	\
	.name = "Next",						\
	.file = "btn_next.bmp",					\
	.target = (target_screen),				\
})

#define ADVANCED_OPTIONS_ITEM ((struct ui_menu_item){	\
	.name = "Advanced options",			\
	.file = "btn_adv_options.bmp",			\
	.type = UI_MENU_ITEM_TYPE_SECONDARY,		\
	.icon_file = "ic_settings.bmp",			\
	.target = UI_SCREEN_ADVANCED_OPTIONS,		\
})

/* Action that will power off the device. */
static vb2_error_t power_off_action(struct ui_context *ui)
{
	return VB2_REQUEST_SHUTDOWN;
}

#define POWER_OFF_ITEM ((struct ui_menu_item){	\
	.name = "Power off",			\
	.file = "btn_power_off.bmp",		\
	.type = UI_MENU_ITEM_TYPE_SECONDARY,	\
	.icon_file = "ic_power.bmp",		\
	.flags = UI_MENU_ITEM_FLAG_NO_ARROW,	\
	.action = power_off_action,		\
})

/******************************************************************************/
/* Helper functions */

static int is_battery_low(void)
{
	static uint32_t batt_pct;
	static int batt_pct_initialized = 0;

	if (!batt_pct_initialized) {
		if (!CONFIG(DRIVER_EC_CROS)) {
			UI_WARN("No EC support to get battery level; "
				"assuming low battery\n");
		} else if (cros_ec_read_batt_state_of_charge(&batt_pct)) {
			UI_WARN("Failed to get battery level; "
				"assuming low battery\n");
			batt_pct = 0;
		}
		batt_pct_initialized = 1;
	}
	return batt_pct < 10;
}

/*
 * Draw the scrollbar for log screen.
 *
 * @param state		UI state.
 * @param begin_y	Starting y-coordinate of the box of the log screen.
 */
static vb2_error_t draw_log_scrollbar(const struct ui_state *state,
				      int32_t begin_y)
{
	const uint32_t page_count = state->log->page_count;
	const int32_t log_box_width = UI_SCALE - UI_MARGIN_H * 2;
	const int32_t x = UI_MARGIN_H + log_box_width - UI_BOX_PADDING_H -
		UI_SCROLLBAR_WIDTH;
	int32_t log_box_inside_height;
	uint32_t lines_per_page, chars_per_line;

	/* No scrollbar if there is only one page. */
	if (page_count <= 1)
		return VB2_SUCCESS;

	VB2_TRY(ui_get_log_textbox_dimensions(
		state->screen->id, state->locale->code, &lines_per_page,
		&chars_per_line));

	log_box_inside_height =
		UI_BOX_TEXT_HEIGHT * lines_per_page +
		UI_BOX_TEXT_LINE_SPACING * (lines_per_page - 1);

	VB2_TRY(ui_draw_scrollbar(x, begin_y + UI_BOX_PADDING_V,
				  log_box_inside_height, state->current_page,
				  page_count, 1));

	return VB2_SUCCESS;
}

static vb2_error_t draw_log_desc(const struct ui_state *state,
				 const struct ui_state *prev_state,
				 int32_t *y)
{
	static char *prev_buf;
	static size_t prev_buf_len;
	static int32_t prev_y;
	char *buf;
	size_t buf_len;
	vb2_error_t rv = VB2_SUCCESS;
	int32_t begin_y = *y;

	buf = ui_log_get_page_content(state->log, state->current_page);
	if (!buf)
		return VB2_ERROR_UI_LOG_INIT;
	buf_len = strlen(buf);
	/* Redraw only if screen or text changed. */
	if (!prev_state || state->screen->id != prev_state->screen->id ||
	    state->error_code != prev_state->error_code ||
	    !prev_buf || buf_len != prev_buf_len ||
	    strncmp(buf, prev_buf, buf_len) ||
	    state->current_page != prev_state->current_page) {
		rv = ui_draw_log_textbox(buf, state, y);

		if (rv == VB2_SUCCESS)
			rv = draw_log_scrollbar(state, begin_y);
	} else {
		*y = prev_y;
	}

	if (prev_buf)
		free(prev_buf);
	prev_buf = buf;
	prev_buf_len = buf_len;
	prev_y = *y;

	return rv;
}

/******************************************************************************/
/* Functions for ui error handling */

static vb2_error_t set_ui_error(struct ui_context *ui,
				enum ui_error error_code)
{
	/* Keep the first occurring error. */
	if (ui->error_code)
		UI_INFO("When handling ui error %#x, another ui error "
			"occurred: %#x",
			ui->error_code, error_code);
	else
		ui->error_code = error_code;
	/* Return to the ui loop to show the error code. */
	return VB2_REQUEST_UI_CONTINUE;
}

static vb2_error_t set_ui_error_and_go_back(struct ui_context *ui,
					    enum ui_error error_code)
{
	set_ui_error(ui, error_code);
	return ui_screen_back(ui);
}

/******************************************************************************/
/*
 * Functions used for log screens
 *
 * Expects that the page_count is valid and page_up_item and page_down_item are
 * assigned to correct menu item indices in all three functions, the
 * current_page is valid in prev and next actions, and the back_item is assigned
 * to a correct menu item index.
 */

/* TODO(b/167514343): Make log screen API more consistent */
static vb2_error_t log_page_update(struct ui_context *ui,
				   const char *new_log_string)
{
	const struct ui_locale *locale;
	const struct ui_screen_info *screen = ui->state->screen;

	if (CONFIG(HEADLESS)) {
		/* TODO(b/151200757): Support headless devices */
		global_ui_log_info.page_count = 0;
		return VB2_SUCCESS;
	}

	if (new_log_string) {
		VB2_TRY(ui_get_locale_info(ui->locale_id, &locale));
		VB2_TRY(ui_log_init(screen->id, locale->code, new_log_string,
				    &global_ui_log_info));
		if (global_ui_log_info.page_count == 0) {
			UI_ERROR("page_count is 0\n");
			return VB2_ERROR_UI_LOG_INIT;
		}

		ui->state->page_count = global_ui_log_info.page_count;

		if (ui->state->current_page >= ui->state->page_count)
			ui->state->current_page = ui->state->page_count - 1;
		ui->force_display = 1;
	}
	VB2_CLR_BIT(ui->state->disabled_item_mask, screen->page_up_item);
	VB2_CLR_BIT(ui->state->disabled_item_mask, screen->page_down_item);
	if (ui->state->current_page == 0)
		VB2_SET_BIT(ui->state->disabled_item_mask,
			    screen->page_up_item);
	if (ui->state->current_page == ui->state->page_count - 1)
		VB2_SET_BIT(ui->state->disabled_item_mask,
			    screen->page_down_item);

	return VB2_SUCCESS;
}

static vb2_error_t log_page_reset_to_top(struct ui_context *ui)
{
	const struct ui_screen_info *screen = ui->state->screen;

	ui->state->current_page = 0;
	ui->state->selected_item = ui->state->page_count > 1
					   ? screen->page_down_item
					   : screen->back_item;
	return log_page_update(ui, NULL);
}

static vb2_error_t log_page_show_back_or_cancel(struct ui_context *ui,
						int is_show_cancel)
{
	int back_item = ui->state->screen->back_item;
	int cancel_item = ui->state->screen->cancel_item;
	VB2_CLR_BIT(ui->state->hidden_item_mask, back_item);
	VB2_CLR_BIT(ui->state->hidden_item_mask, cancel_item);
	if (is_show_cancel) {
		VB2_SET_BIT(ui->state->hidden_item_mask, back_item);
		if (ui->state->selected_item == back_item)
			ui->state->selected_item = cancel_item;
	} else {
		VB2_SET_BIT(ui->state->hidden_item_mask, cancel_item);
		if (ui->state->selected_item == cancel_item)
			ui->state->selected_item = back_item;
	}
	return VB2_SUCCESS;
}

static vb2_error_t log_page_prev_action(struct ui_context *ui)
{
	/* Validity check. */
	if (ui->state->current_page == 0)
		return VB2_SUCCESS;

	ui->state->current_page--;
	return log_page_update(ui, NULL);
}

static vb2_error_t log_page_next_action(struct ui_context *ui)
{
	/* Validity check. */
	if (ui->state->current_page == ui->state->page_count - 1)
		return VB2_SUCCESS;

	ui->state->current_page++;
	return log_page_update(ui, NULL);
}

/******************************************************************************/
/* UI_SCREEN_FIRMWARE_SYNC */

static const char *const firmware_sync_desc[] = {
	"firmware_sync_desc.bmp",
};

static const struct ui_screen_info firmware_sync_screen = {
	.id = UI_SCREEN_FIRMWARE_SYNC,
	.icon = UI_ICON_TYPE_NONE,
	.title = "firmware_sync_title.bmp",
	.desc = UI_DESC(firmware_sync_desc),
	.no_footer = 1,
	.mesg = "Please do not power off your device.\n"
		"Your system is applying a critical update.",
};

/******************************************************************************/
/* UI_SCREEN_LANGUAGE_SELECT */

static vb2_error_t language_select_init(struct ui_context *ui)
{
	const struct ui_menu *menu = ui_get_menu(ui);
	if (menu->num_items == 0) {
		UI_ERROR("ERROR: No menu items found; "
			 "rejecting entering language selection screen\n");
		return ui_screen_back(ui);
	}
	if (ui->locale_id < menu->num_items) {
		ui->state->selected_item = ui->locale_id;
	} else {
		UI_WARN("WARNING: Current locale not found in menu items; "
			"initializing selected_item to 0\n");
		ui->state->selected_item = 0;
	}
	return VB2_SUCCESS;
}

static vb2_error_t draw_language_select(const struct ui_state *state,
					const struct ui_state *prev_state)
{
	int id;
	const int reverse = state->locale->rtl;
	uint32_t num_lang;
	uint32_t locale_id;
	int32_t x, x_begin, x_end, y, y_begin, y_end, y_center, menu_height;
	int num_lang_per_page, target_pos, id_begin, id_end;
	int32_t box_width, box_height;
	const int32_t border_thickness = UI_LANG_MENU_BORDER_THICKNESS;
	const uint32_t flags = PIVOT_H_LEFT | PIVOT_V_CENTER;
	int focused;
	const struct ui_locale *locale;
	const struct rgb_color *bg_color, *fg_color;
	struct ui_bitmap bitmap;

	/*
	 * Call default drawing function to clear the screen if necessary, and
	 * draw the footer.
	 */
	VB2_TRY(ui_draw_default(state, prev_state));

	num_lang = ui_get_locale_count();
	if (num_lang == 0) {
		UI_ERROR("Locale count is 0\n");
		return VB2_ERROR_UI_INVALID_ARCHIVE;
	}

	x_begin = UI_MARGIN_H;
	x_end = UI_SCALE - UI_MARGIN_H;
	box_width = x_end - x_begin;
	box_height = UI_LANG_MENU_BOX_HEIGHT;

	y_begin = UI_MARGIN_TOP + UI_LANG_BOX_HEIGHT + UI_LANG_MENU_MARGIN_TOP;
	y_end = UI_SCALE - UI_MARGIN_BOTTOM - UI_FOOTER_HEIGHT -
		UI_FOOTER_MARGIN_TOP;
	num_lang_per_page = (y_end - y_begin) / box_height;
	menu_height = box_height * MIN(num_lang_per_page, num_lang);
	y_end = y_begin + menu_height;  /* Correct for integer division error */

	/* Get current locale_id */
	locale_id = state->selected_item;
	if (locale_id >= num_lang) {
		UI_WARN("selected_item (%u) exceeds number of locales (%u); "
			"falling back to locale 0\n",
			locale_id, num_lang);
		locale_id = 0;
	}

	/* Draw language dropdown */
	VB2_TRY(ui_get_locale_info(locale_id, &locale));
	VB2_TRY(ui_draw_language_header(locale, state, 1));

	/*
	 * Calculate the list of languages to display, from id_begin
	 * (inclusive) to id_end (exclusive). The focused one is placed at the
	 * center of the list if possible.
	 */
	target_pos = (num_lang_per_page - 1) / 2;
	if (locale_id < target_pos || num_lang < num_lang_per_page) {
		/* locale_id is too small to put at the center, or
		   all languages fit in the screen */
		id_begin = 0;
		id_end = MIN(num_lang_per_page, num_lang);
	} else if (locale_id > num_lang - num_lang_per_page + target_pos) {
		/* locale_id is too large to put at the center */
		id_begin = num_lang - num_lang_per_page;
		id_end = num_lang;
	} else {
		/* Place locale_id at the center. It's guaranteed that
		   (id_begin >= 0) and (id_end <= num_lang). */
		id_begin = locale_id - target_pos;
		id_end = locale_id + num_lang_per_page - target_pos;
	}

	/* Draw dropdown menu */
	x = x_begin + UI_LANG_ICON_GLOBE_SIZE + UI_LANG_ICON_MARGIN_H * 2;
	y = y_begin;
	for (id = id_begin; id < id_end; id++) {
		focused = id == locale_id;
		bg_color = focused ? &ui_color_button : &ui_color_lang_menu_bg;
		fg_color = focused ? &ui_color_lang_menu_bg : &ui_color_fg;
		/* Solid box */
		VB2_TRY(ui_draw_rounded_box(x_begin, y, box_width, box_height,
					    bg_color, 0, 0, reverse));
		/* Separator between languages */
		if (id > id_begin)
			VB2_TRY(ui_draw_h_line(x_begin, y, box_width,
					       border_thickness,
					       &ui_color_lang_menu_border));
		/* Text */
		y_center = y + box_height / 2;
		VB2_TRY(ui_get_locale_info(id, &locale));
		VB2_TRY(ui_get_language_name_bitmap(locale->code, &bitmap));
		VB2_TRY(ui_draw_mapped_bitmap(&bitmap, x, y_center,
					      UI_SIZE_AUTO,
					      UI_LANG_MENU_TEXT_HEIGHT,
					      bg_color, fg_color,
					      flags, reverse));
		y += box_height;
	}

	/* Draw outer borders */
	VB2_TRY(ui_draw_rounded_box(x_begin, y_begin, box_width, menu_height,
				    &ui_color_lang_menu_border,
				    border_thickness, 0, reverse));

	if (num_lang <= num_lang_per_page)
		return VB2_SUCCESS;

	/* Draw scrollbar */
	x = x_end - UI_LANG_MENU_SCROLLBAR_MARGIN_RIGHT - UI_SCROLLBAR_WIDTH;
	if (reverse)
		x = UI_SCALE - x - UI_SCROLLBAR_WIDTH +
		    UI_LANG_MENU_SCROLLBAR_MARGIN_RIGHT;
	VB2_TRY(ui_draw_scrollbar(x, y_begin, menu_height, id_begin, num_lang,
				  num_lang_per_page));

	return VB2_SUCCESS;
}

static vb2_error_t language_select_action(struct ui_context *ui)
{
	vb2_error_t rv;
	ui->locale_id = ui->state->selected_item;
	UI_INFO("Locale changed to %u\n", ui->locale_id);

	/* Write locale id back to nvdata. */
	vb2api_set_locale_id(ui->ctx, ui->locale_id);

	/* Commit nvdata changes immediately, in case of three-finger salute
	   reboot. Ignore commit errors in recovery mode. */
	rv = vb2ex_commit_data(ui->ctx);
	if (rv && !(ui->ctx->flags & VB2_CONTEXT_RECOVERY_MODE))
		return rv;

	return ui_screen_back(ui);
}

const struct ui_menu *get_language_menu(struct ui_context *ui)
{
	int i;
	uint32_t num_locales;
	struct ui_menu_item *items;

	if (ui->language_menu.num_items > 0)
		return &ui->language_menu;

	num_locales = vb2ex_get_locale_count();
	if (num_locales == 0) {
		UI_WARN("WARNING: No locales available; assuming 1 locale\n");
		num_locales = 1;
	}

	items = malloc(num_locales * sizeof(struct ui_menu_item));
	if (!items) {
		UI_ERROR("ERROR: malloc failed for language items\n");
		return NULL;
	}

	for (i = 0; i < num_locales; i++) {
		items[i].name = "Some language";
		items[i].action = language_select_action;
	}

	ui->language_menu.num_items = num_locales;
	ui->language_menu.items = items;
	return &ui->language_menu;
}

static const struct ui_screen_info language_select_screen = {
	.id = UI_SCREEN_LANGUAGE_SELECT,
	.name = "Language selection screen",
	.init = language_select_init,
	.draw = draw_language_select,
	.mesg = "Language selection",
	.get_menu = get_language_menu,
};

/******************************************************************************/
/* UI_SCREEN_RECOVERY_BROKEN */

static const char *const broken_desc[] = {
	"broken_desc.bmp",
};

static const struct ui_menu_item broken_items[] = {
	LANGUAGE_SELECT_ITEM,
	ADVANCED_OPTIONS_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info broken_screen = {
	.id = UI_SCREEN_RECOVERY_BROKEN,
	.name = "Recover broken device",
	.icon = UI_ICON_TYPE_INFO,
	.title = "broken_title.bmp",
	.desc = UI_DESC(broken_desc),
	.menu = UI_MENU(broken_items),
	.mesg = "Something is wrong. Please remove all connected devices.\n"
		"To initiate recovery on a:\n"
		"* Chromebook: Hold down Escape, Refresh, and Power buttons\n"
		"* Chromebox/Chromebit: Hold down the Recovery button, press "
		"Power, release\n"
		"  the Recovery button\n"
		"* Tablet: Hold down Power, Volume Up, Volume Down buttons for "
		"10 secs",
};

/******************************************************************************/
/* UI_SCREEN_ADVANCED_OPTIONS */

#define ADVANCED_OPTIONS_ITEM_DEVELOPER_MODE 1
#define ADVANCED_OPTIONS_ITEM_DEBUG_INFO 2
#define ADVANCED_OPTIONS_ITEM_INTERNET_RECOVERY 4

static vb2_error_t boot_minios_impl(struct ui_context *ui, int non_active_only)
{
	/* Validity check, should never happen. */
	if (ui->ctx->boot_mode != VB2_BOOT_MODE_MANUAL_RECOVERY) {
		UI_ERROR("ERROR: Not in manual recovery mode; ignoring\n");
		return VB2_REQUEST_UI_CONTINUE;
	}

	vb2_error_t rv = VbTryLoadMiniOsKernel(ui->ctx, !!non_active_only);
	if (rv) {
		UI_ERROR("ERROR: Failed to boot from MiniOS: %#x\n", rv);
		return set_ui_error(ui, UI_ERROR_MINIOS_BOOT_FAILED);
	}
	return VB2_REQUEST_UI_EXIT;
}

static vb2_error_t boot_old_minios_action(struct ui_context *ui)
{
	return boot_minios_impl(ui, 1);
}

vb2_error_t advanced_options_init(struct ui_context *ui)
{
	ui->state->selected_item = ADVANCED_OPTIONS_ITEM_DEVELOPER_MODE;
	if ((ui->ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) ||
	    ui->ctx->boot_mode != VB2_BOOT_MODE_MANUAL_RECOVERY) {
		VB2_SET_BIT(ui->state->hidden_item_mask,
			    ADVANCED_OPTIONS_ITEM_DEVELOPER_MODE);
		ui->state->selected_item = ADVANCED_OPTIONS_ITEM_DEBUG_INFO;
	}

	if (ui->ctx->boot_mode != VB2_BOOT_MODE_MANUAL_RECOVERY)
		VB2_SET_BIT(ui->state->hidden_item_mask,
			    ADVANCED_OPTIONS_ITEM_INTERNET_RECOVERY);

	return VB2_SUCCESS;
}

static const struct ui_menu_item advanced_options_items[] = {
	LANGUAGE_SELECT_ITEM,
	[ADVANCED_OPTIONS_ITEM_DEVELOPER_MODE] = {
		.name = "Enable developer mode",
		.file = "btn_dev_mode.bmp",
		.target = UI_SCREEN_RECOVERY_TO_DEV,
	},
	[ADVANCED_OPTIONS_ITEM_DEBUG_INFO] = {
		.name = "Debug info",
		.file = "btn_debug_info.bmp",
		.target = UI_SCREEN_DEBUG_INFO,
	},
	{
		.name = "Firmware log",
		.file = "btn_firmware_log.bmp",
		.target = UI_SCREEN_FIRMWARE_LOG,
	},
	[ADVANCED_OPTIONS_ITEM_INTERNET_RECOVERY] = {
		.name = "Internet recovery (older version)",
		.file = "btn_rec_by_internet_old.bmp",
		.action = boot_old_minios_action,
	},
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info advanced_options_screen = {
	.id = UI_SCREEN_ADVANCED_OPTIONS,
	.name = "Advanced options",
	.icon = UI_ICON_TYPE_NONE,
	.title = "adv_options_title.bmp",
	.init = advanced_options_init,
	.mesg = "Advanced options",
	.menu = UI_MENU(advanced_options_items),
};

/******************************************************************************/
/* UI_SCREEN_DEBUG_INFO */

#define DEBUG_INFO_ITEM_PAGE_UP 1
#define DEBUG_INFO_ITEM_PAGE_DOWN 2
#define DEBUG_INFO_ITEM_BACK 3

static vb2_error_t debug_info_set_content(struct ui_context *ui)
{
	const char *log_string = vb2ex_get_debug_info(ui->ctx);
	if (!log_string)
		return set_ui_error_and_go_back(ui, UI_ERROR_DEBUG_LOG);
	if (vb2_is_error(log_page_update(ui, log_string)))
		return set_ui_error_and_go_back(ui, UI_ERROR_DEBUG_LOG);
	return VB2_SUCCESS;
}

static vb2_error_t debug_info_init(struct ui_context *ui)
{
	VB2_TRY(debug_info_set_content(ui));
	if (vb2_is_error(log_page_reset_to_top(ui)))
		return set_ui_error_and_go_back(ui, UI_ERROR_DEBUG_LOG);
	return VB2_SUCCESS;
}

static vb2_error_t debug_info_reinit(struct ui_context *ui)
{
	return debug_info_set_content(ui);
}

static const struct ui_menu_item debug_info_items[] = {
	LANGUAGE_SELECT_ITEM,
	[DEBUG_INFO_ITEM_PAGE_UP] = PAGE_UP_ITEM,
	[DEBUG_INFO_ITEM_PAGE_DOWN] = PAGE_DOWN_ITEM,
	[DEBUG_INFO_ITEM_BACK] = BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info debug_info_screen = {
	.id = UI_SCREEN_DEBUG_INFO,
	.name = "Debug info",
	.icon = UI_ICON_TYPE_NONE,
	.title = "debug_info_title.bmp",
	.menu = UI_MENU(debug_info_items),
	.init = debug_info_init,
	.reinit = debug_info_reinit,
	.draw_desc = draw_log_desc,
	.mesg = "Debug info",
	.page_up_item = DEBUG_INFO_ITEM_PAGE_UP,
	.page_down_item = DEBUG_INFO_ITEM_PAGE_DOWN,
	.back_item = DEBUG_INFO_ITEM_BACK,
};

/******************************************************************************/
/* UI_SCREEN_FIRMWARE_LOG */

#define FIRMWARE_LOG_ITEM_PAGE_UP 1
#define FIRMWARE_LOG_ITEM_PAGE_DOWN 2
#define FIRMWARE_LOG_ITEM_BACK 3

static vb2_error_t firmware_log_set_content(struct ui_context *ui,
					    int reset)
{
	const char *log_string = vb2ex_get_firmware_log(reset);
	if (!log_string)
		return set_ui_error_and_go_back(ui, UI_ERROR_FIRMWARE_LOG);
	if (vb2_is_error(log_page_update(ui, log_string)))
		return set_ui_error_and_go_back(ui, UI_ERROR_FIRMWARE_LOG);
	return VB2_SUCCESS;
}

static vb2_error_t firmware_log_init(struct ui_context *ui)
{
	VB2_TRY(firmware_log_set_content(ui, 1));
	if (vb2_is_error(log_page_reset_to_top(ui)))
		return set_ui_error_and_go_back(ui, UI_ERROR_FIRMWARE_LOG);
	return VB2_SUCCESS;
}

static vb2_error_t firmware_log_reinit(struct ui_context *ui)
{
	return firmware_log_set_content(ui, 0);
}

static const struct ui_menu_item firmware_log_items[] = {
	LANGUAGE_SELECT_ITEM,
	[FIRMWARE_LOG_ITEM_PAGE_UP] = PAGE_UP_ITEM,
	[FIRMWARE_LOG_ITEM_PAGE_DOWN] = PAGE_DOWN_ITEM,
	[FIRMWARE_LOG_ITEM_BACK] = BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info firmware_log_screen = {
	.id = UI_SCREEN_FIRMWARE_LOG,
	.name = "Firmware log",
	.icon = UI_ICON_TYPE_NONE,
	.title = "firmware_log_title.bmp",
	.menu = UI_MENU(firmware_log_items),
	.init = firmware_log_init,
	.reinit = firmware_log_reinit,
	.draw_desc = draw_log_desc,
	.mesg = "Firmware log",
	.page_up_item = FIRMWARE_LOG_ITEM_PAGE_UP,
	.page_down_item = FIRMWARE_LOG_ITEM_PAGE_DOWN,
	.back_item = FIRMWARE_LOG_ITEM_BACK,
};

/******************************************************************************/
/* UI_SCREEN_RECOVERY_TO_DEV */

#define RECOVERY_TO_DEV_ITEM_CONFIRM 1
#define RECOVERY_TO_DEV_ITEM_CANCEL 2

static const char *const recovery_to_dev_desc[] = {
	"rec_to_dev_desc0.bmp",
	"rec_to_dev_desc1.bmp",
};

static vb2_error_t recovery_to_dev_init(struct ui_context *ui)
{
	if (ui->ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) {
		/* Notify the user that they are already in dev mode */
		UI_WARN("Already in dev mode\n");
		return set_ui_error_and_go_back(
			ui, UI_ERROR_DEV_MODE_ALREADY_ENABLED);
	}

	if (!CONFIG(PHYSICAL_PRESENCE_KEYBOARD) &&
	    vb2ex_physical_presence_pressed()) {
		UI_INFO("Physical presence button stuck?\n");
		return ui_screen_back(ui);
	}


	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD)) {
		ui->state->selected_item = RECOVERY_TO_DEV_ITEM_CONFIRM;
	} else {
		/*
		 * Disable "Confirm" button for other physical presence types.
		 */
		VB2_SET_BIT(ui->state->hidden_item_mask,
			    RECOVERY_TO_DEV_ITEM_CONFIRM);
		ui->state->selected_item = RECOVERY_TO_DEV_ITEM_CANCEL;
	}

	ui->physical_presence_button_pressed = 0;

	return VB2_SUCCESS;
}

static vb2_error_t recovery_to_dev_finalize(struct ui_context *ui)
{
	UI_INFO("Physical presence confirmed!\n");

	/* Validity check, should never happen. */
	if (ui->state->screen->id != UI_SCREEN_RECOVERY_TO_DEV ||
	    (ui->ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) ||
	    ui->ctx->boot_mode != VB2_BOOT_MODE_MANUAL_RECOVERY) {
		UI_ERROR("ERROR: Dev transition validity check failed\n");
		return VB2_SUCCESS;
	}

	UI_INFO("Enabling dev mode and rebooting...\n");

	if (vb2api_enable_developer_mode(ui->ctx) != VB2_SUCCESS) {
		UI_WARN("Failed to enable developer mode\n");
		return VB2_SUCCESS;
	}

	return VB2_REQUEST_REBOOT_EC_TO_RO;
}

static vb2_error_t recovery_to_dev_confirm_action(struct ui_context *ui)
{
	if (!ui->key_trusted) {
		UI_INFO("Reject untrusted %s confirmation\n",
			ui->key == UI_KEY_ENTER ? "ENTER" : "POWER");
		/*
		 * If physical presence is confirmed using the keyboard,
		 * beep and notify the user when the ENTER key comes
		 * from an untrusted keyboard.
		 */
		if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD) &&
		    ui->key == UI_KEY_ENTER)
			return set_ui_error(
				ui, UI_ERROR_UNTRUSTED_CONFIRMATION);
		return VB2_SUCCESS;
	}
	return recovery_to_dev_finalize(ui);
}

static vb2_error_t recovery_to_dev_action(struct ui_context *ui)
{
	int pressed;

	if (ui->key == ' ') {
		UI_INFO("SPACE means cancel dev mode transition\n");
		return ui_screen_back(ui);
	}

	/* Keyboard physical presence case covered by "Confirm" action. */
	if (CONFIG(PHYSICAL_PRESENCE_KEYBOARD))
		return VB2_SUCCESS;

	pressed = vb2ex_physical_presence_pressed();
	if (pressed) {
		UI_INFO("Physical presence button pressed, "
			"awaiting release\n");
		ui->physical_presence_button_pressed = 1;
		return VB2_SUCCESS;
	}
	if (!ui->physical_presence_button_pressed)
		return VB2_SUCCESS;
	UI_INFO("Physical presence button released\n");

	return recovery_to_dev_finalize(ui);
}

static const struct ui_menu_item recovery_to_dev_items[] = {
	LANGUAGE_SELECT_ITEM,
	[RECOVERY_TO_DEV_ITEM_CONFIRM] = {
		.name = "Confirm",
		.file = "btn_confirm.bmp",
		.action = recovery_to_dev_confirm_action,
	},
	[RECOVERY_TO_DEV_ITEM_CANCEL] = {
		.name = "Cancel",
		.file = "btn_cancel.bmp",
		.action = ui_screen_back,
	},
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_to_dev_screen = {
	.id = UI_SCREEN_RECOVERY_TO_DEV,
	.name = "Transition to developer mode",
	.icon = UI_ICON_TYPE_INFO,
	.title = "rec_to_dev_title.bmp",
	.desc = UI_DESC(recovery_to_dev_desc),
	.menu = UI_MENU(recovery_to_dev_items),
	.init = recovery_to_dev_init,
	.action = recovery_to_dev_action,
	.mesg = "You are attempting to enable developer mode\n"
		"This involves erasing all data from your device,\n"
		"and will make your device insecure.\n"
		"Select \"Confirm\" or press the RECOVERY/POWER button to\n"
		"enable developer mode,\n"
		"or select \"Cancel\" to remain protected",
};

/******************************************************************************/
/* UI_SCREEN_RECOVERY_SELECT */

#define RECOVERY_SELECT_ITEM_PHONE 1
#define RECOVERY_SELECT_ITEM_EXTERNAL_DISK 2
#define RECOVERY_SELECT_ITEM_INTERNET 3
#define RECOVERY_SELECT_ITEM_DIAGNOSTICS 4

static vb2_error_t draw_recovery_select_desc(
	const struct ui_state *state,
	const struct ui_state *prev_state,
	int32_t *y)
{
	static const char *const desc_files[] = {
		"rec_sel_desc0.bmp",
		"rec_sel_desc1.bmp",
	};
	static const char *const desc_no_phone_files[] = {
		"rec_sel_desc0.bmp",
		"rec_sel_desc1_no_phone.bmp",
	};
	struct vb2_context *ctx = vboot_get_context();
	const struct ui_desc desc = vb2api_phone_recovery_ui_enabled(ctx) ?
		UI_DESC(desc_files) : UI_DESC(desc_no_phone_files);

	return ui_draw_desc(&desc, state, y);
}

/* Set VB2_NV_DIAG_REQUEST and reboot. */
static vb2_error_t launch_diagnostics_action(struct ui_context *ui)
{
	vb2api_request_diagnostics(ui->ctx);
	return VB2_REQUEST_REBOOT;
}

vb2_error_t ui_recovery_mode_boot_minios_action(struct ui_context *ui)
{
	return boot_minios_impl(ui, 0);
}

vb2_error_t recovery_select_init(struct ui_context *ui)
{
	ui->state->selected_item = RECOVERY_SELECT_ITEM_PHONE;
	if (!vb2api_phone_recovery_ui_enabled(ui->ctx)) {
		UI_WARN("WARNING: Phone recovery not available\n");
		VB2_SET_BIT(ui->state->hidden_item_mask,
			    RECOVERY_SELECT_ITEM_PHONE);
		ui->state->selected_item = RECOVERY_SELECT_ITEM_EXTERNAL_DISK;
	}

	if (!vb2api_diagnostic_ui_enabled(ui->ctx))
		VB2_SET_BIT(ui->state->hidden_item_mask,
			    RECOVERY_SELECT_ITEM_DIAGNOSTICS);

	return VB2_SUCCESS;
}

static const struct ui_menu_item recovery_select_items[] = {
	LANGUAGE_SELECT_ITEM,
	[RECOVERY_SELECT_ITEM_PHONE] = {
		.name = "Recovery using phone",
		.file = "btn_rec_by_phone.bmp",
		.target = UI_SCREEN_RECOVERY_PHONE_STEP1,
	},
	[RECOVERY_SELECT_ITEM_EXTERNAL_DISK] = {
		.name = "Recovery using external disk",
		.file = "btn_rec_by_disk.bmp",
		.target = UI_SCREEN_RECOVERY_DISK_STEP1,
	},
	[RECOVERY_SELECT_ITEM_INTERNET] = {
		.name = "Recovery using internet connection",
		.file = "btn_rec_by_internet.bmp",
		.action = ui_recovery_mode_boot_minios_action,
	},
	[RECOVERY_SELECT_ITEM_DIAGNOSTICS] = {
		.name = "Launch diagnostics",
		.file = "btn_launch_diag.bmp",
		.type = UI_MENU_ITEM_TYPE_SECONDARY,
		.icon_file = "ic_search.bmp",
		.flags = UI_MENU_ITEM_FLAG_NO_ARROW,
		.action = launch_diagnostics_action,
	},
	ADVANCED_OPTIONS_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_select_screen = {
	.id = UI_SCREEN_RECOVERY_SELECT,
	.name = "Recovery method selection",
	.icon = UI_ICON_TYPE_INFO,
	.title = "rec_sel_title.bmp",
	.draw_desc = draw_recovery_select_desc,
	.menu = UI_MENU(recovery_select_items),
	.init = recovery_select_init,
	.mesg = "Select how you'd like to recover.\n"
		"You can recover using a USB drive or an SD card.",
};

/******************************************************************************/
/* UI_SCREEN_RECOVERY_PHONE_STEP1 */

static vb2_error_t draw_recovery_phone_step1_desc(
	const struct ui_state *state,
	const struct ui_state *prev_state,
	int32_t *y)
{
	static const char *const desc_files[] = {
		"rec_phone_step1_desc0.bmp",
		"rec_phone_step1_desc1.bmp",
		"rec_step1_desc2.bmp",
	};
	static const char *const desc_low_battery_files[] = {
		"rec_phone_step1_desc0.bmp",
		"rec_phone_step1_desc1.bmp",
		"rec_step1_desc2_low_bat.bmp",
	};
	const struct ui_desc desc = is_battery_low() ?
		UI_DESC(desc_low_battery_files) : UI_DESC(desc_files);

	return ui_draw_desc(&desc, state, y);
}

static const struct ui_menu_item recovery_phone_step1_items[] = {
	LANGUAGE_SELECT_ITEM,
	NEXT_ITEM(UI_SCREEN_RECOVERY_PHONE_STEP2),
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_phone_step1_screen = {
	.id = UI_SCREEN_RECOVERY_PHONE_STEP1,
	.name = "Phone recovery step 1",
	.icon = UI_ICON_TYPE_STEP,
	.step = 1,
	.num_steps = 3,
	.title = "rec_step1_title.bmp",
	.menu = UI_MENU(recovery_phone_step1_items),
	.draw_desc = draw_recovery_phone_step1_desc,
	.mesg = "To proceed with the recovery process, you’ll need\n"
		"1. An Android phone with internet access\n"
		"2. A USB cable which connects your phone and this device\n"
		"We also recommend that you connect to a power source during\n"
		"the recovery process.",
};

/******************************************************************************/
/* UI_SCREEN_RECOVERY_PHONE_STEP2 */

static vb2_error_t draw_recovery_phone_step2_desc(
	const struct ui_state *state,
	const struct ui_state *prev_state,
	int32_t *y)
{
	static const char *const desc_files[] = {
		"rec_phone_step2_desc.bmp",
	};
	static const struct ui_desc desc = UI_DESC(desc_files);
	const int32_t x = UI_SCALE - UI_MARGIN_H - UI_REC_QR_MARGIN_H;
	const int32_t y_begin = *y;
	const int32_t size = UI_REC_QR_SIZE;
	const uint32_t flags = PIVOT_H_RIGHT | PIVOT_V_TOP;
	const int reverse = state->locale->rtl;
	struct ui_bitmap bitmap;

	VB2_TRY(ui_draw_desc(&desc, state, y));
	VB2_TRY(ui_get_bitmap("qr_rec_phone.bmp", NULL, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y_begin, size, size,
			       flags, reverse));
	return VB2_SUCCESS;
}

static const struct ui_menu_item recovery_phone_step2_items[] = {
	LANGUAGE_SELECT_ITEM,
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_phone_step2_screen = {
	.id = UI_SCREEN_RECOVERY_PHONE_STEP2,
	.name = "Phone recovery step 2",
	.icon = UI_ICON_TYPE_STEP,
	.step = 2,
	.num_steps = 3,
	.title = "rec_phone_step2_title.bmp",
	.menu = UI_MENU(recovery_phone_step2_items),
	.draw_desc = draw_recovery_phone_step2_desc,
	.mesg = "Download the Chrome OS recovery app on your Android phone\n"
		"by plugging in your phone or by scanning the QR code on the\n"
		"right. Once you launch the app, connect your phone to your\n"
		"device and recovery will start automatically.",
};

/******************************************************************************/
/* UI_SCREEN_RECOVERY_DISK_STEP1 */

static vb2_error_t draw_recovery_disk_step1_desc(
	const struct ui_state *state,
	const struct ui_state *prev_state,
	int32_t *y)
{
	static const char *const desc_files[] = {
		"rec_disk_step1_desc0.bmp",
		"rec_disk_step1_desc1.bmp",
		"rec_step1_desc2.bmp",
	};
	static const char *const desc_low_battery_files[] = {
		"rec_disk_step1_desc0.bmp",
		"rec_disk_step1_desc1.bmp",
		"rec_step1_desc2_low_bat.bmp",
	};
	const struct ui_desc desc = is_battery_low() ?
		UI_DESC(desc_low_battery_files) : UI_DESC(desc_files);

	return ui_draw_desc(&desc, state, y);
}

static const struct ui_menu_item recovery_disk_step1_items[] = {
	LANGUAGE_SELECT_ITEM,
	NEXT_ITEM(UI_SCREEN_RECOVERY_DISK_STEP2),
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_disk_step1_screen = {
	.id = UI_SCREEN_RECOVERY_DISK_STEP1,
	.name = "Disk recovery step 1",
	.icon = UI_ICON_TYPE_STEP,
	.step = 1,
	.num_steps = 3,
	.title = "rec_step1_title.bmp",
	.menu = UI_MENU(recovery_disk_step1_items),
	.draw_desc = draw_recovery_disk_step1_desc,
	.mesg = "To proceed with the recovery process, you'll need\n"
		"1. An external storage disk such as a USB drive or an SD card"
		"\n2. An additional device with internet access\n"
		"We also recommend that you connect to a power source during\n"
		"the recovery process.",
};

/******************************************************************************/
/* UI_SCREEN_RECOVERY_DISK_STEP2 */

static const char *const recovery_disk_step2_desc[] = {
	"rec_disk_step2_desc0.bmp",
	"rec_disk_step2_desc1.bmp",
	"rec_disk_step2_desc2.bmp",
};

static const struct ui_menu_item recovery_disk_step2_items[] = {
	LANGUAGE_SELECT_ITEM,
	NEXT_ITEM(UI_SCREEN_RECOVERY_DISK_STEP3),
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_disk_step2_screen = {
	.id = UI_SCREEN_RECOVERY_DISK_STEP2,
	.name = "Disk recovery step 2",
	.icon = UI_ICON_TYPE_STEP,
	.step = 2,
	.num_steps = 3,
	.title = "rec_disk_step2_title.bmp",
	.desc = UI_DESC(recovery_disk_step2_desc),
	.menu = UI_MENU(recovery_disk_step2_items),
	.mesg = "External disk setup.\n"
		"Go to google.com/chromeos/recovery on another computer and\n"
		"install the Chrome extension. Follow instructions on the\n"
		"Chrome extension, and download the recovery image onto an\n"
		"external disk.",
};

/******************************************************************************/
/* UI_SCREEN_RECOVERY_DISK_STEP3 */

static const char *const recovery_disk_step3_desc[] = {
	"rec_disk_step3_desc0.bmp",
};

static const struct ui_menu_item recovery_disk_step3_items[] = {
	LANGUAGE_SELECT_ITEM,
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_disk_step3_screen = {
	.id = UI_SCREEN_RECOVERY_DISK_STEP3,
	.name = "Disk recovery step 3",
	.icon = UI_ICON_TYPE_STEP,
	.step = 3,
	.num_steps = 3,
	.title = "rec_disk_step3_title.bmp",
	.desc = UI_DESC(recovery_disk_step3_desc),
	.menu = UI_MENU(recovery_disk_step3_items),
	.mesg = "Do you have your external disk ready?\n"
		"If your external disk is ready with a recovery image, plug\n"
		"it into the device to start the recovery process.",
};

/******************************************************************************/
/* UI_SCREEN_RECOVERY_INVALID */

static vb2_error_t draw_recovery_invalid_desc(
	const struct ui_state *state,
	const struct ui_state *prev_state,
	int32_t *y)
{
	struct vb2_context *ctx = vboot_get_context();
	static const char *const desc_files[] = {
		"rec_invalid_desc.bmp",
	};
	static const char *const desc_no_phone_files[] = {
		"rec_invalid_disk_desc.bmp",
	};
	const struct ui_desc desc = vb2api_phone_recovery_ui_enabled(ctx) ?
		UI_DESC(desc_files) : UI_DESC(desc_no_phone_files);

	return ui_draw_desc(&desc, state, y);
}

static const struct ui_menu_item recovery_invalid_items[] = {
	LANGUAGE_SELECT_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_invalid_screen = {
	.id = UI_SCREEN_RECOVERY_INVALID,
	.icon = UI_ICON_TYPE_STEP,
	.step = -3,
	.num_steps = 3,
	.title = "rec_invalid_title.bmp",
	.menu = UI_MENU(recovery_invalid_items),
	.draw_desc = draw_recovery_invalid_desc,
	.mesg = "No valid image detected.\n"
		"Make sure your external disk has a valid recovery image,\n"
		"and re-insert the disk when ready.",
};

/******************************************************************************/
/* UI_SCREEN_DEVELOPER_MODE */

#define DEVELOPER_MODE_ITEM_RETURN_TO_SECURE 1
#define DEVELOPER_MODE_ITEM_BOOT_INTERNAL 2
#define DEVELOPER_MODE_ITEM_BOOT_EXTERNAL 3
#define DEVELOPER_MODE_ITEM_SELECT_ALTFW 4

static vb2_error_t developer_mode_init(struct ui_context *ui)
{
	enum vb2_dev_default_boot_target default_boot =
		vb2api_get_dev_default_boot_target(ui->ctx);

	/* Hide "Return to secure mode" button if GBB forces dev mode. */
	if (vb2api_gbb_get_flags(ui->ctx) & VB2_GBB_FLAG_FORCE_DEV_SWITCH_ON)
		VB2_SET_BIT(ui->state->hidden_item_mask,
			    DEVELOPER_MODE_ITEM_RETURN_TO_SECURE);

	/* Hide "Boot from external disk" button if not allowed. */
	if (!(ui->ctx->flags & VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED))
		VB2_SET_BIT(ui->state->hidden_item_mask,
			    DEVELOPER_MODE_ITEM_BOOT_EXTERNAL);

	/* Hide "Select alternate bootloader" button if not allowed. */
	if (!(ui->ctx->flags & VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED))
		VB2_SET_BIT(ui->state->hidden_item_mask,
			    DEVELOPER_MODE_ITEM_SELECT_ALTFW);

	/* Choose the default selection. */
	switch (default_boot) {
	case VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL:
		ui->state->selected_item = DEVELOPER_MODE_ITEM_BOOT_EXTERNAL;
		break;
	case VB2_DEV_DEFAULT_BOOT_TARGET_ALTFW:
		ui->state->selected_item =
			DEVELOPER_MODE_ITEM_SELECT_ALTFW;
		break;
	default:
		ui->state->selected_item = DEVELOPER_MODE_ITEM_BOOT_INTERNAL;
		break;
	}

	ui->start_time_ms = vb2ex_mtime();

	return VB2_SUCCESS;
}

vb2_error_t ui_developer_mode_boot_internal_action(struct ui_context *ui)
{
	vb2_error_t rv;

	/* Validity check, should never happen. */
	if (!(ui->ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) ||
	    !(ui->ctx->flags & VB2_CONTEXT_DEV_BOOT_ALLOWED)) {
		UI_ERROR("ERROR: Dev mode internal boot not allowed\n");
		return VB2_SUCCESS;
	}

	rv = VbTryLoadKernel(ui->ctx, VB_DISK_FLAG_FIXED);
	if (rv == VB2_SUCCESS)
		return VB2_REQUEST_UI_EXIT;

	UI_ERROR("ERROR: Failed to boot from internal disk: %#x\n", rv);
	ui->error_beep = 1;
	return set_ui_error(ui, UI_ERROR_INTERNAL_BOOT_FAILED);
}

vb2_error_t ui_developer_mode_boot_external_action(struct ui_context *ui)
{
	vb2_error_t rv;

	if (!(ui->ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) ||
	    !(ui->ctx->flags & VB2_CONTEXT_DEV_BOOT_ALLOWED) ||
	    !(ui->ctx->flags & VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED)) {
		UI_ERROR("ERROR: Dev mode external boot not allowed\n");
		ui->error_beep = 1;
		return set_ui_error(ui, UI_ERROR_EXTERNAL_BOOT_DISABLED);
	}

	rv = VbTryLoadKernel(ui->ctx, VB_DISK_FLAG_REMOVABLE);
	if (rv == VB2_SUCCESS) {
		return VB2_REQUEST_UI_EXIT;
	} else if (rv == VB2_ERROR_LK_NO_DISK_FOUND) {
		if (ui->state->screen->id !=
		    UI_SCREEN_DEVELOPER_BOOT_EXTERNAL) {
			UI_WARN("No external disk found\n");
			ui->error_beep = 1;
		}
		return ui_screen_change(
			ui, UI_SCREEN_DEVELOPER_BOOT_EXTERNAL);
	} else {
		if (ui->state->screen->id !=
		    UI_SCREEN_DEVELOPER_INVALID_DISK) {
			UI_WARN("Invalid external disk: %#x\n", rv);
			ui->error_beep = 1;
		}
		return ui_screen_change(
			ui, UI_SCREEN_DEVELOPER_INVALID_DISK);
	}
}

static vb2_error_t developer_mode_action(struct ui_context *ui)
{
	const int dev_delay_ms = (vb2api_gbb_get_flags(ui->ctx) &
				  VB2_GBB_FLAG_DEV_SCREEN_SHORT_DELAY) ?
		DEV_DELAY_SHORT_MS : DEV_DELAY_NORMAL_MS;
	uint64_t elapsed_ms;
	enum vb2_dev_default_boot_target default_boot;

	/* Once any user interaction occurs, stop the timer. */
	if (ui->key)
		ui->disable_timer = 1;
	if (ui->disable_timer)
		return VB2_SUCCESS;

	elapsed_ms = vb2ex_mtime() - ui->start_time_ms;

	/* Boot from default target after timeout. */
	if (elapsed_ms > dev_delay_ms) {
		UI_INFO("Booting default target after %ds\n",
			dev_delay_ms / MSECS_PER_SEC);
		ui->disable_timer = 1;
		default_boot = vb2api_get_dev_default_boot_target(ui->ctx);
		switch (default_boot) {
		case VB2_DEV_DEFAULT_BOOT_TARGET_EXTERNAL:
			return ui_developer_mode_boot_external_action(ui);
		case VB2_DEV_DEFAULT_BOOT_TARGET_ALTFW:
			return ui_developer_mode_boot_altfw_action(ui);
		default:
			return ui_developer_mode_boot_internal_action(ui);
		}
	}

	/* Beep at 20 and 20.5 seconds. */
	if ((ui->beep_count == 0 && elapsed_ms > DEV_DELAY_BEEP1_MS) ||
	    (ui->beep_count == 1 && elapsed_ms > DEV_DELAY_BEEP2_MS)) {
		vb2ex_beep(250, 400);
		ui->beep_count++;
	}

	return VB2_SUCCESS;
}

static vb2_error_t draw_developer_mode_desc(
	const struct ui_state *state,
	const struct ui_state *prev_state,
	int32_t *y)
{
	struct ui_bitmap bitmap;
	const char *locale_code = state->locale->code;
	const int reverse = state->locale->rtl;
	int32_t x;
	const int32_t w = UI_SIZE_AUTO;
	int32_t h;
	uint32_t flags = PIVOT_H_LEFT | PIVOT_V_TOP;

	x = UI_MARGIN_H;

	/*
	 * Description about returning to secure mode. When the "Return to
	 * secure mode" button is hidden, hide this description line.
	 */
	if (!VB2_GET_BIT(state->hidden_item_mask,
			 DEVELOPER_MODE_ITEM_RETURN_TO_SECURE)) {
		VB2_TRY(ui_get_bitmap("dev_desc0.bmp", locale_code, 0,
				      &bitmap));
		h = UI_DESC_TEXT_HEIGHT * ui_get_bitmap_num_lines(&bitmap);
		VB2_TRY(ui_draw_bitmap(&bitmap, x, *y, w, h, flags, reverse));
		*y += h + UI_DESC_TEXT_LINE_SPACING;
	}

	/*
	 * Description about automatically booting from the default boot target.
	 * After the timer in developer mode is disabled, this description no
	 * longer makes sense, so hide it.
	 */
	VB2_TRY(ui_get_bitmap("dev_desc1.bmp", locale_code, 0,
			      &bitmap));
	h = UI_DESC_TEXT_HEIGHT * ui_get_bitmap_num_lines(&bitmap);
	/* Either clear the desc line, or draw it again. */
	if (state->timer_disabled)
		VB2_TRY(ui_draw_box(x, *y, UI_SCALE - x, h, &ui_color_bg,
				    reverse));
	else
		VB2_TRY(ui_draw_bitmap(&bitmap, x, *y, w, h, flags, reverse));
	*y += h;

	return VB2_SUCCESS;
}

static const struct ui_menu_item developer_mode_items[] = {
	LANGUAGE_SELECT_ITEM,
	[DEVELOPER_MODE_ITEM_RETURN_TO_SECURE] = {
		.name = "Return to secure mode",
		.file = "btn_secure_mode.bmp",
		.target = UI_SCREEN_DEVELOPER_TO_NORM,
	},
	[DEVELOPER_MODE_ITEM_BOOT_INTERNAL] = {
		.name = "Boot from internal disk",
		.file = "btn_int_disk.bmp",
		.action = ui_developer_mode_boot_internal_action,
	},
	[DEVELOPER_MODE_ITEM_BOOT_EXTERNAL] = {
		.name = "Boot from external disk",
		.file = "btn_ext_disk.bmp",
		.action = ui_developer_mode_boot_external_action,
	},
	[DEVELOPER_MODE_ITEM_SELECT_ALTFW] = {
		.name = "Select alternate bootloader",
		.file = "btn_alt_bootloader.bmp",
		.target = UI_SCREEN_DEVELOPER_SELECT_ALTFW,
	},
	ADVANCED_OPTIONS_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info developer_mode_screen = {
	.id = UI_SCREEN_DEVELOPER_MODE,
	.name = "Developer mode",
	.icon = UI_ICON_TYPE_DEV_MODE,
	.title = "dev_title.bmp",
	.menu = UI_MENU(developer_mode_items),
	.init = developer_mode_init,
	.action = developer_mode_action,
	.draw_desc = draw_developer_mode_desc,
	.mesg = "You are in developer mode\n"
		"To return to the recommended secure mode,\n"
		"select \"Return to secure mode\" below.\n"
		"After timeout, the device will automatically boot from\n"
		"the default boot target.",
};

/******************************************************************************/
/* UI_SCREEN_DEVELOPER_TO_NORM */

#define DEVELOPER_TO_NORM_ITEM_CONFIRM 1
#define DEVELOPER_TO_NORM_ITEM_CANCEL 2

static vb2_error_t developer_to_norm_init(struct ui_context *ui)
{
	/* Don't allow to-norm if GBB forces dev mode */
	if (vb2api_gbb_get_flags(ui->ctx) & VB2_GBB_FLAG_FORCE_DEV_SWITCH_ON) {
		UI_ERROR("ERROR: to-norm not allowed\n");
		return set_ui_error_and_go_back(
			ui, UI_ERROR_TO_NORM_NOT_ALLOWED);
	}
	ui->state->selected_item = DEVELOPER_TO_NORM_ITEM_CONFIRM;
	/* Hide "Cancel" button if dev boot is not allowed */
	if (!(ui->ctx->flags & VB2_CONTEXT_DEV_BOOT_ALLOWED))
		VB2_SET_BIT(ui->state->hidden_item_mask,
			    DEVELOPER_TO_NORM_ITEM_CANCEL);
	return VB2_SUCCESS;
}

vb2_error_t developer_to_norm_action(struct ui_context *ui)
{
	if (vb2api_disable_developer_mode(ui->ctx) == VB2_SUCCESS)
		return VB2_REQUEST_REBOOT;
	else
		return VB2_SUCCESS;
}

static const char *const developer_to_norm_desc[] = {
	"dev_to_norm_desc0.bmp",
	"dev_to_norm_desc1.bmp",
};

static const struct ui_menu_item developer_to_norm_items[] = {
	LANGUAGE_SELECT_ITEM,
	[DEVELOPER_TO_NORM_ITEM_CONFIRM] = {
		.name = "Confirm",
		.file = "btn_confirm.bmp",
		.action = developer_to_norm_action,
	},
	[DEVELOPER_TO_NORM_ITEM_CANCEL] = {
		.name = "Cancel",
		.file = "btn_cancel.bmp",
		.action = ui_screen_back,
	},
	POWER_OFF_ITEM,
};

static const struct ui_screen_info developer_to_norm_screen = {
	.id = UI_SCREEN_DEVELOPER_TO_NORM,
	.name = "Transition to normal mode",
	.icon = UI_ICON_TYPE_RESTART,
	.title = "dev_to_norm_title.bmp",
	.desc = UI_DESC(developer_to_norm_desc),
	.menu = UI_MENU(developer_to_norm_items),
	.init = developer_to_norm_init,
	.mesg = "Confirm returning to secure mode.\n"
		"This option will disable developer mode and restore your\n"
		"device to its original state.\n"
		"Your user data will be wiped in the process.",
};

/******************************************************************************/
/* UI_SCREEN_DEVELOPER_BOOT_EXTERNAL */

#define DEVELOPER_BOOT_EXTERNAL_ITEM_BACK 1

static const char *const developer_boot_external_desc[] = {
	"dev_boot_ext_desc0.bmp",
};

static const struct ui_menu_item developer_boot_external_items[] = {
	LANGUAGE_SELECT_ITEM,
	[DEVELOPER_BOOT_EXTERNAL_ITEM_BACK] = BACK_ITEM,
	POWER_OFF_ITEM,
};

static vb2_error_t developer_boot_external_check(struct ui_context *ui)
{
	if (!(ui->ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) ||
	    !(ui->ctx->flags & VB2_CONTEXT_DEV_BOOT_ALLOWED) ||
	    !(ui->ctx->flags & VB2_CONTEXT_DEV_BOOT_EXTERNAL_ALLOWED)) {
		UI_ERROR("ERROR: Dev mode external boot not allowed\n");
		ui->error_beep = 1;
		return set_ui_error_and_go_back(
			ui, UI_ERROR_EXTERNAL_BOOT_DISABLED);
	}
	return VB2_SUCCESS;
}

static vb2_error_t developer_boot_external_init(struct ui_context *ui)
{
	vb2_error_t rv;

	ui->state->selected_item = DEVELOPER_BOOT_EXTERNAL_ITEM_BACK;
	VB2_TRY(developer_boot_external_check(ui));
	rv = VbTryLoadKernel(ui->ctx, VB_DISK_FLAG_REMOVABLE);
	/* If the status of the external disk doesn't match, skip the screen. */
	if (rv != VB2_ERROR_LK_NO_DISK_FOUND)
		return ui_screen_back(ui);

	return VB2_SUCCESS;
}

static const struct ui_screen_info developer_boot_external_screen = {
	.id = UI_SCREEN_DEVELOPER_BOOT_EXTERNAL,
	.name = "Developer boot from external disk",
	.icon = UI_ICON_TYPE_NONE,
	.title = "dev_boot_ext_title.bmp",
	.desc = UI_DESC(developer_boot_external_desc),
	.menu = UI_MENU(developer_boot_external_items),
	.init = developer_boot_external_init,
	.reinit = developer_boot_external_init,
	.action = ui_developer_mode_boot_external_action,
	.mesg = "Plug in your external disk\n"
		"If your external disk is ready with a Chrome OS image,\n"
		"plug it into the device to boot.",
};

/******************************************************************************/
/* UI_SCREEN_DEVELOPER_INVALID_DISK */

#define DEVELOPER_INVALID_DISK_ITEM_BACK 1

static const char *const developer_invalid_disk_desc[] = {
	"dev_invalid_disk_desc0.bmp",
};

static const struct ui_menu_item developer_invalid_disk_items[] = {
	LANGUAGE_SELECT_ITEM,
	[DEVELOPER_INVALID_DISK_ITEM_BACK] = BACK_ITEM,
	POWER_OFF_ITEM,
};

static vb2_error_t developer_invalid_disk_init(struct ui_context *ui)
{
	vb2_error_t rv;

	ui->state->selected_item = DEVELOPER_INVALID_DISK_ITEM_BACK;
	VB2_TRY(developer_boot_external_check(ui));
	rv = VbTryLoadKernel(ui->ctx, VB_DISK_FLAG_REMOVABLE);
	/* If the status of the external disk doesn't match, skip the screen. */
	if (rv == VB2_SUCCESS || rv == VB2_ERROR_LK_NO_DISK_FOUND)
		return ui_screen_back(ui);

	return VB2_SUCCESS;
}

static const struct ui_screen_info developer_invalid_disk_screen = {
	.id = UI_SCREEN_DEVELOPER_INVALID_DISK,
	.name = "Invalid external disk in dev mode",
	.icon = UI_ICON_TYPE_ERROR,
	.title = "dev_invalid_disk_title.bmp",
	.desc = UI_DESC(developer_invalid_disk_desc),
	.menu = UI_MENU(developer_invalid_disk_items),
	.init = developer_invalid_disk_init,
	.reinit = developer_invalid_disk_init,
	.action = ui_developer_mode_boot_external_action,
	.mesg = "No valid image detected\n"
		"Make sure your external disk has a valid Chrome OS image,\n"
		"and re-insert the disk when ready.",
};

/******************************************************************************/
/* UI_SCREEN_DEVELOPER_SELECT_ALTFW */

static const struct ui_menu_item developer_select_altfw_items_before[] = {
	LANGUAGE_SELECT_ITEM,
};

static const struct ui_menu_item developer_select_altfw_items_after[] = {
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static vb2_error_t developer_select_bootloader_init(struct ui_context *ui)
{
	if (ui_get_menu(ui)->num_items == 0)
		return set_ui_error_and_go_back(ui, UI_ERROR_ALTFW_EMPTY);
	/* Select the first bootloader. */
	ui->state->selected_item =
		ARRAY_SIZE(developer_select_altfw_items_before);
	return VB2_SUCCESS;
}

static vb2_error_t developer_boot_altfw_impl(struct ui_context *ui,
					     uint32_t altfw_id)
{
	if (!(ui->ctx->flags & VB2_CONTEXT_DEVELOPER_MODE) ||
	    !(ui->ctx->flags & VB2_CONTEXT_DEV_BOOT_ALLOWED) ||
	    !(ui->ctx->flags & VB2_CONTEXT_DEV_BOOT_ALTFW_ALLOWED)) {
		UI_ERROR("ERROR: Dev mode alternate bootloader not allowed\n");
		return set_ui_error(ui, UI_ERROR_ALTFW_DISABLED);
	}

	if (vb2ex_get_altfw_count() == 0) {
		UI_ERROR("ERROR: No alternate bootloader was found\n");
		return set_ui_error(ui, UI_ERROR_ALTFW_EMPTY);
	}

	UI_INFO("Try booting from bootloader #%u\n", altfw_id);

	/* vb2ex_run_altfw will not return if successful. */
	vb2ex_run_altfw(altfw_id);

	UI_ERROR("ERROR: Alternate bootloader failed\n");
	return set_ui_error(ui, UI_ERROR_ALTFW_FAILED);
}

static vb2_error_t developer_boot_altfw_id_action(struct ui_context *ui)
{
	/* Validity check, should never happen. */
	if (ui->state->screen->id != UI_SCREEN_DEVELOPER_SELECT_ALTFW) {
		UI_ERROR("ERROR: Boot altfw id from wrong screen: %#x\n",
			 ui->state->screen->id);
		return VB2_SUCCESS;
	}

	const size_t menu_before_len =
		ARRAY_SIZE(developer_select_altfw_items_before);
	uint32_t altfw_id = ui->state->selected_item - menu_before_len + 1;
	return developer_boot_altfw_impl(ui, altfw_id);
}

vb2_error_t ui_developer_mode_boot_altfw_action(struct ui_context *ui)
{
	/* Bootloader #0 is the default. */
	return developer_boot_altfw_impl(ui, 0);
}

static const struct ui_menu *get_bootloader_menu(struct ui_context *ui)
{
	struct altfw_info *node;
	ListNode *head;
	uint32_t num_bootloaders = 0;
	struct ui_menu_item *items;
	size_t num_items;
	uint32_t i;

	/* Cached */
	if (ui->bootloader_menu.num_items > 0) {
		UI_INFO("Cached with %zu item(s)\n",
			ui->bootloader_menu.num_items);
		return &ui->bootloader_menu;
	}

	head = payload_get_altfw_list();
	if (!head) {
		UI_ERROR("ERROR: Failed to get altfw list\n");
		return NULL;
	}

	list_for_each(node, *head, list_node) {
		/* Discount default seqnum=0, since it is duplicated. */
		if (node->seqnum)
			num_bootloaders++;
	}

	if (num_bootloaders == 0) {
		UI_WARN("No bootloader was found\n");
		return NULL;
	}

	UI_INFO("num_bootloaders: %u\n", num_bootloaders);

	num_items = num_bootloaders +
		    ARRAY_SIZE(developer_select_altfw_items_before) +
		    ARRAY_SIZE(developer_select_altfw_items_after);
	items = malloc(num_items * sizeof(struct ui_menu_item));
	if (!items) {
		UI_ERROR("Failed to malloc menu items for bootloaders\n");
		return NULL;
	}

	/* Copy prefix items to the begin. */
	memcpy(&items[0], developer_select_altfw_items_before,
	       sizeof(developer_select_altfw_items_before));

	/* Copy bootloaders. */
	i = ARRAY_SIZE(developer_select_altfw_items_before);
	list_for_each(node, *head, list_node) {
		/* Discount default seqnum=0, since it is duplicated. */
		if (!node->seqnum)
			continue;
		UI_INFO("Bootloader: filename=%s, name=%s, "
			"desc=%s, seqnum=%d\n",
			node->filename, node->name,
			node->desc, node->seqnum);
		const char *name = node->name;
		if (!name || strlen(name) == 0) {
			UI_WARN("Failed to get bootloader name with "
				"seqnum=%d, use filename instead\n",
				node->seqnum);
			name = node->filename;
		}
		items[i] = (struct ui_menu_item){
			.name = name,
			.action = developer_boot_altfw_id_action,
		};
		i++;
	}

	/* Copy postfix items to the end. */
	memcpy(&items[i], developer_select_altfw_items_after,
	       sizeof(developer_select_altfw_items_after));

	ui->bootloader_menu.num_items = num_items;
	ui->bootloader_menu.items = items;

	return &ui->bootloader_menu;
}

static vb2_error_t draw_developer_select_bootloader(
	const struct ui_state *state,
	const struct ui_state *prev_state)
{
	static const struct ui_menu empty_menu = {
		.num_items = 0,
		.items = NULL,
	};
	const struct ui_menu *menu;
	int32_t y;

	/*
	 * Call default drawing function to clear the screen if necessary,
	 * draw the language dropdown header, draw the title and desc lines,
	 * and draw the footer.
	 */
	VB2_TRY(ui_draw_default(state, prev_state));

	/* Draw bootloaders and secondary buttons. */
	y = UI_MARGIN_TOP + UI_LANG_BOX_HEIGHT + UI_LANG_MARGIN_BOTTOM +
	    UI_TITLE_TEXT_HEIGHT + UI_TITLE_MARGIN_BOTTOM +
	    UI_DESC_MARGIN_BOTTOM;

	/*
	 * TODO(b/172339016): make ui_context available in this function, so
	 * that we can use ui_get_menu(ui) here.
	 */
	menu = state->screen->get_menu(NULL);
	if (!menu)
		menu = &empty_menu;

	return ui_draw_menu_items(menu, state, prev_state, y);
}

static const struct ui_screen_info developer_select_bootloader_screen = {
	.id = UI_SCREEN_DEVELOPER_SELECT_ALTFW,
	.name = "Select alternate bootloader",
	.icon = UI_ICON_TYPE_NONE,
	.title = "dev_select_bootloader_title.bmp",
	.init = developer_select_bootloader_init,
	.draw = draw_developer_select_bootloader,
	.mesg = "Select an alternate bootloader.",
	.get_menu = get_bootloader_menu,
};

/******************************************************************************/
/* UI_SCREEN_DIAGNOSTICS */

#define DIAGNOSTICS_ITEM_STORAGE_HEALTH 1
#define DIAGNOSTICS_ITEM_STORAGE_TEST_SHORT 2
#define DIAGNOSTICS_ITEM_STORAGE_TEST_EXTENDED 3

static vb2_error_t diagnostics_init(struct ui_context *ui)
{
	const char *unused_log_string;
	vb2_error_t rv = vb2ex_diag_get_storage_test_log(&unused_log_string);
	if (rv == VB2_ERROR_EX_UNIMPLEMENTED) {
		VB2_SET_BIT(ui->state->disabled_item_mask,
			    DIAGNOSTICS_ITEM_STORAGE_TEST_SHORT);
		VB2_SET_BIT(ui->state->disabled_item_mask,
			    DIAGNOSTICS_ITEM_STORAGE_TEST_EXTENDED);
	}
	ui->state->selected_item = DIAGNOSTICS_ITEM_STORAGE_HEALTH;
	return VB2_SUCCESS;
}

static const char *const diagnostics_desc[] = {
	"diag_menu_desc0.bmp",
};

static const struct ui_menu_item diagnostics_items[] = {
	LANGUAGE_SELECT_ITEM,
	[DIAGNOSTICS_ITEM_STORAGE_HEALTH] = {
		.name = "Storage health info",
		.file = "btn_diag_storage_health.bmp",
		.target = UI_SCREEN_DIAGNOSTICS_STORAGE_HEALTH,
	},
	[DIAGNOSTICS_ITEM_STORAGE_TEST_SHORT] = {
		.name = "Storage self-test (short)",
		.file = "btn_diag_storage_short_test.bmp",
		.target = UI_SCREEN_DIAGNOSTICS_STORAGE_TEST_SHORT,
	},
	[DIAGNOSTICS_ITEM_STORAGE_TEST_EXTENDED] = {
		.name = "Storage self-test (Extended)",
		.file = "btn_diag_storage_ext_test.bmp",
		.target = UI_SCREEN_DIAGNOSTICS_STORAGE_TEST_EXTENDED,
	},
	{
		.name = "Memory check (quick)",
		.file = "btn_diag_memory_quick.bmp",
		.target = UI_SCREEN_DIAGNOSTICS_MEMORY_QUICK,
	},
	{
		.name = "Memory check (full)",
		.file = "btn_diag_memory_full.bmp",
		.target = UI_SCREEN_DIAGNOSTICS_MEMORY_FULL,
	},
	POWER_OFF_ITEM,
};

static const struct ui_screen_info diagnostics_screen = {
	.id = UI_SCREEN_DIAGNOSTICS,
	.name = "Diagnostic tools",
	.icon = UI_ICON_TYPE_INFO,
	.title = "diag_menu_title.bmp",
	.desc = UI_DESC(diagnostics_desc),
	.menu = UI_MENU(diagnostics_items),
	.init = diagnostics_init,
	.mesg = "Select the component you'd like to check",
};

/******************************************************************************/
/* UI_SCREEN_DIAGNOSTICS_STORAGE_HEALTH */

#define DIAGNOSTICS_STORAGE_HEALTH_ITEM_PAGE_UP 0
#define DIAGNOSTICS_STORAGE_HEALTH_ITEM_PAGE_DOWN 1
#define DIAGNOSTICS_STORAGE_HEALTH_ITEM_BACK 2

static const struct ui_menu_item diagnostics_storage_health_items[] = {
	[DIAGNOSTICS_STORAGE_HEALTH_ITEM_PAGE_UP] = PAGE_UP_ITEM,
	[DIAGNOSTICS_STORAGE_HEALTH_ITEM_PAGE_DOWN] = PAGE_DOWN_ITEM,
	[DIAGNOSTICS_STORAGE_HEALTH_ITEM_BACK] = BACK_ITEM,
	POWER_OFF_ITEM,
};

static vb2_error_t diagnostics_storage_health_init_impl(
	struct ui_context *ui)
{
	const char *log_string;
	VB2_TRY(vb2ex_diag_get_storage_health(&log_string));
	VB2_TRY(log_page_update(ui, log_string));
	return log_page_reset_to_top(ui);
}

static vb2_error_t diagnostics_storage_health_init(struct ui_context *ui)
{
	if (vb2_is_error(diagnostics_storage_health_init_impl(ui)))
		return set_ui_error_and_go_back(ui, UI_ERROR_DIAGNOSTICS);
	return VB2_SUCCESS;
}

static const struct ui_screen_info diagnostics_storage_health_screen = {
	.id = UI_SCREEN_DIAGNOSTICS_STORAGE_HEALTH,
	.name = "Storage health info",
	.icon = UI_ICON_TYPE_NONE,
	.title = "diag_storage_health_title.bmp",
	.menu = UI_MENU(diagnostics_storage_health_items),
	.init = diagnostics_storage_health_init,
	.draw_desc = draw_log_desc,
	.mesg = "Storage health info",
	.page_up_item = DIAGNOSTICS_STORAGE_HEALTH_ITEM_PAGE_UP,
	.page_down_item = DIAGNOSTICS_STORAGE_HEALTH_ITEM_PAGE_DOWN,
	.back_item = DIAGNOSTICS_STORAGE_HEALTH_ITEM_BACK,
};

/******************************************************************************/
/* UI_SCREEN_DIAGNOSTICS_STORAGE_TEST_SHORT */
/* UI_SCREEN_DIAGNOSTICS_STORAGE_TEST_EXTENDED */

#define DIAGNOSTICS_STORAGE_TEST_ITEM_PAGE_UP 0
#define DIAGNOSTICS_STORAGE_TEST_ITEM_PAGE_DOWN 1
#define DIAGNOSTICS_STORAGE_TEST_ITEM_BACK 2
#define DIAGNOSTICS_STORAGE_TEST_ITEM_CANCEL 3

static vb2_error_t diagnostics_storage_test_update_impl(
	struct ui_context *ui)
{
	const char *log_string;
	int is_test_running = 0;

	/* Early return if the test is done. */
	if (ui->state->test_finished)
		return VB2_SUCCESS;

	vb2_error_t rv = vb2ex_diag_get_storage_test_log(&log_string);
	switch (rv) {
	case VB2_ERROR_EX_DIAG_TEST_RUNNING:
		is_test_running = 1;
		break;
	case VB2_SUCCESS:
		ui->state->test_finished = 1;
		break;
	default:
		UI_INFO("vb2ex_diag_get_storage_test_log returned %#x\n", rv);
		return rv;
	}
	VB2_TRY(log_page_show_back_or_cancel(ui, is_test_running));
	return log_page_update(ui, log_string);
}

static vb2_error_t diagnostics_storage_test_update(struct ui_context *ui)
{
	if (vb2_is_error(diagnostics_storage_test_update_impl(ui)))
		return set_ui_error_and_go_back(ui, UI_ERROR_DIAGNOSTICS);
	return VB2_SUCCESS;
}

static vb2_error_t diagnostics_storage_test_control(
	struct ui_context *ui, enum BlockDevTestOpsType op)
{
	if (vb2_is_error(diag_storage_test_control(op)))
		return set_ui_error_and_go_back(ui, UI_ERROR_DIAGNOSTICS);
	return VB2_SUCCESS;
}

static vb2_error_t diagnostics_storage_test_init(struct ui_context *ui)
{
	VB2_TRY(diagnostics_storage_test_update(ui));
	if (vb2_is_error(log_page_reset_to_top(ui)))
		return set_ui_error_and_go_back(ui, UI_ERROR_DIAGNOSTICS);
	return VB2_SUCCESS;
}

static vb2_error_t diagnostics_storage_test_short_init(
	struct ui_context *ui)
{
	VB2_TRY(diagnostics_storage_test_control(ui,
						 BLOCKDEV_TEST_OPS_TYPE_STOP));
	VB2_TRY(diagnostics_storage_test_control(ui,
						 BLOCKDEV_TEST_OPS_TYPE_SHORT));
	return diagnostics_storage_test_init(ui);
}

static vb2_error_t diagnostics_storage_test_extended_init(
	struct ui_context *ui)
{
	VB2_TRY(diagnostics_storage_test_control(ui,
						 BLOCKDEV_TEST_OPS_TYPE_STOP));
	VB2_TRY(diagnostics_storage_test_control(
		ui, BLOCKDEV_TEST_OPS_TYPE_EXTENDED));
	return diagnostics_storage_test_init(ui);
}

static vb2_error_t diagnostics_storage_test_cancel(struct ui_context *ui)
{
	VB2_TRY(diagnostics_storage_test_control(ui,
						 BLOCKDEV_TEST_OPS_TYPE_STOP));
	return ui_screen_back(ui);
}

static const struct ui_menu_item diagnostics_storage_test_items[] = {
	[DIAGNOSTICS_STORAGE_TEST_ITEM_PAGE_UP] = PAGE_UP_ITEM,
	[DIAGNOSTICS_STORAGE_TEST_ITEM_PAGE_DOWN] = PAGE_DOWN_ITEM,
	[DIAGNOSTICS_STORAGE_TEST_ITEM_BACK] = {
		.name = "Back",
		.file = "btn_back.bmp",
		.flags = UI_MENU_ITEM_FLAG_TRANSIENT,
		.action = ui_screen_back,
	},
	[DIAGNOSTICS_STORAGE_TEST_ITEM_CANCEL] = {
		.name = "Cancel",
		.file = "btn_cancel.bmp",
		.flags = UI_MENU_ITEM_FLAG_TRANSIENT,
		.action = diagnostics_storage_test_cancel,
	},
	POWER_OFF_ITEM,
};

static const struct ui_screen_info diagnostics_storage_test_short_screen = {
	.id = UI_SCREEN_DIAGNOSTICS_STORAGE_TEST_SHORT,
	.name = "Storage self-test (short)",
	.icon = UI_ICON_TYPE_NONE,
	.title = "diag_storage_srt_test_title.bmp",
	.menu = UI_MENU(diagnostics_storage_test_items),
	.init = diagnostics_storage_test_short_init,
	.action = diagnostics_storage_test_update,
	.draw_desc = draw_log_desc,
	.mesg = "Storage self test (short)",
	.page_up_item = DIAGNOSTICS_STORAGE_TEST_ITEM_PAGE_UP,
	.page_down_item = DIAGNOSTICS_STORAGE_TEST_ITEM_PAGE_DOWN,
	.back_item = DIAGNOSTICS_STORAGE_TEST_ITEM_BACK,
	.cancel_item = DIAGNOSTICS_STORAGE_TEST_ITEM_CANCEL,
};

static const struct ui_screen_info diagnostics_storage_test_extended_screen = {
	.id = UI_SCREEN_DIAGNOSTICS_STORAGE_TEST_EXTENDED,
	.name = "Storage self-test (extended)",
	.icon = UI_ICON_TYPE_NONE,
	.title = "diag_storage_ext_test_title.bmp",
	.menu = UI_MENU(diagnostics_storage_test_items),
	.init = diagnostics_storage_test_extended_init,
	.action = diagnostics_storage_test_update,
	.draw_desc = draw_log_desc,
	.mesg = "Storage self test (extended)",
	.page_up_item = DIAGNOSTICS_STORAGE_TEST_ITEM_PAGE_UP,
	.page_down_item = DIAGNOSTICS_STORAGE_TEST_ITEM_PAGE_DOWN,
	.back_item = DIAGNOSTICS_STORAGE_TEST_ITEM_BACK,
	.cancel_item = DIAGNOSTICS_STORAGE_TEST_ITEM_CANCEL,
};

/******************************************************************************/
/* UI_SCREEN_DIAGNOSTICS_MEMORY_QUICK */
/* UI_SCREEN_DIAGNOSTICS_MEMORY_FULL */

#define DIAGNOSTICS_MEMORY_ITEM_PAGE_UP 0
#define DIAGNOSTICS_MEMORY_ITEM_PAGE_DOWN 1
#define DIAGNOSTICS_MEMORY_ITEM_BACK 2
#define DIAGNOSTICS_MEMORY_ITEM_CANCEL 3

typedef vb2_error_t (*memory_test_op_t)(int reset, const char **out);
static vb2_error_t diagnostics_memory_update_screen_impl(
	struct ui_context *ui, memory_test_op_t op, int reset)
{
	const char *log_string = NULL;
	vb2_error_t rv;
	int is_test_running = 0;

	/* Early return if the memory test is done. */
	if (ui->state->test_finished)
		return VB2_SUCCESS;

	rv = op(reset, &log_string);
	switch (rv) {
	/* The test is still running but the output buffer was unchanged. */
	case VB2_ERROR_EX_DIAG_TEST_RUNNING:
		return VB2_SUCCESS;
	case VB2_ERROR_EX_DIAG_TEST_UPDATED:
		is_test_running = 1;
		break;
	case VB2_SUCCESS:
		ui->state->test_finished = 1;
		break;
	default:
		UI_INFO("memory_test_op returned %#x\n", rv);
		return rv;
	}
	VB2_TRY(log_page_show_back_or_cancel(ui, is_test_running));
	return log_page_update(ui, log_string);
}

static vb2_error_t diagnostics_memory_update_screen(struct ui_context *ui,
						    memory_test_op_t op,
						    int reset)
{
	if (vb2_is_error(diagnostics_memory_update_screen_impl(ui, op, reset)))
		return set_ui_error_and_go_back(ui, UI_ERROR_DIAGNOSTICS);
	return VB2_SUCCESS;
}

static vb2_error_t diagnostics_memory_init_quick(struct ui_context *ui)
{
	VB2_TRY(diagnostics_memory_update_screen(
		ui, &vb2ex_diag_memory_quick_test, 1));
	if (vb2_is_error(log_page_reset_to_top(ui)))
		return set_ui_error_and_go_back(ui, UI_ERROR_DIAGNOSTICS);
	return VB2_SUCCESS;
}

static vb2_error_t diagnostics_memory_init_full(struct ui_context *ui)
{
	VB2_TRY(diagnostics_memory_update_screen(
		ui, &vb2ex_diag_memory_full_test, 1));
	if (vb2_is_error(log_page_reset_to_top(ui)))
		return set_ui_error_and_go_back(ui, UI_ERROR_DIAGNOSTICS);
	return VB2_SUCCESS;
}

static vb2_error_t diagnostics_memory_update_quick(struct ui_context *ui)
{
	return diagnostics_memory_update_screen(
		ui, &vb2ex_diag_memory_quick_test, 0);
}

static vb2_error_t diagnostics_memory_update_full(struct ui_context *ui)
{
	return diagnostics_memory_update_screen(
		ui, &vb2ex_diag_memory_full_test, 0);
}

static const struct ui_menu_item diagnostics_memory_items[] = {
	[DIAGNOSTICS_MEMORY_ITEM_PAGE_UP] = PAGE_UP_ITEM,
	[DIAGNOSTICS_MEMORY_ITEM_PAGE_DOWN] = PAGE_DOWN_ITEM,
	[DIAGNOSTICS_MEMORY_ITEM_BACK] = {
		.name = "Back",
		.file = "btn_back.bmp",
		.flags = UI_MENU_ITEM_FLAG_TRANSIENT,
		.action = ui_screen_back,
	},
	[DIAGNOSTICS_MEMORY_ITEM_CANCEL] = {
		.name = "Cancel",
		.file = "btn_cancel.bmp",
		.flags = UI_MENU_ITEM_FLAG_TRANSIENT,
		.action = ui_screen_back,
	},
	POWER_OFF_ITEM,
};

static const struct ui_screen_info diagnostics_memory_quick_screen = {
	.id = UI_SCREEN_DIAGNOSTICS_MEMORY_QUICK,
	.name = "Memory check (quick)",
	.icon = UI_ICON_TYPE_NONE,
	.title = "diag_memory_quick_title.bmp",
	.menu = UI_MENU(diagnostics_memory_items),
	.init = diagnostics_memory_init_quick,
	.action = diagnostics_memory_update_quick,
	.draw_desc = draw_log_desc,
	.mesg = "Memory check (quick)",
	.page_up_item = DIAGNOSTICS_MEMORY_ITEM_PAGE_UP,
	.page_down_item = DIAGNOSTICS_MEMORY_ITEM_PAGE_DOWN,
	.back_item = DIAGNOSTICS_MEMORY_ITEM_BACK,
	.cancel_item = DIAGNOSTICS_MEMORY_ITEM_CANCEL,
};

static const struct ui_screen_info diagnostics_memory_full_screen = {
	.id = UI_SCREEN_DIAGNOSTICS_MEMORY_FULL,
	.name = "Memory check (full)",
	.icon = UI_ICON_TYPE_NONE,
	.title = "diag_memory_full_title.bmp",
	.menu = UI_MENU(diagnostics_memory_items),
	.init = diagnostics_memory_init_full,
	.action = diagnostics_memory_update_full,
	.draw_desc = draw_log_desc,
	.mesg = "Memory check (full)",
	.page_up_item = DIAGNOSTICS_MEMORY_ITEM_PAGE_UP,
	.page_down_item = DIAGNOSTICS_MEMORY_ITEM_PAGE_DOWN,
	.back_item = DIAGNOSTICS_MEMORY_ITEM_BACK,
	.cancel_item = DIAGNOSTICS_MEMORY_ITEM_CANCEL,
};

/******************************************************************************/
static const struct ui_screen_info *const screens[] = {
	&firmware_sync_screen,
	&language_select_screen,
	&broken_screen,
	&advanced_options_screen,
	&debug_info_screen,
	&firmware_log_screen,
	&recovery_to_dev_screen,
	&recovery_select_screen,
	&recovery_phone_step1_screen,
	&recovery_phone_step2_screen,
	&recovery_disk_step1_screen,
	&recovery_disk_step2_screen,
	&recovery_disk_step3_screen,
	&recovery_invalid_screen,
	&developer_mode_screen,
	&developer_to_norm_screen,
	&developer_boot_external_screen,
	&developer_invalid_disk_screen,
	&developer_select_bootloader_screen,
	&diagnostics_screen,
	&diagnostics_storage_health_screen,
	&diagnostics_storage_test_short_screen,
	&diagnostics_storage_test_extended_screen,
	&diagnostics_memory_quick_screen,
	&diagnostics_memory_full_screen,
};

const struct ui_screen_info *ui_get_screen_info(enum ui_screen screen_id)
{
	static const struct ui_screen_info *screen_info;
	int i;
	if (screen_info && screen_info->id == screen_id)
		return screen_info;
	for (i = 0; i < ARRAY_SIZE(screens); i++) {
		if (screens[i]->id == screen_id) {
			screen_info = screens[i];
			return screen_info;
		}
	}
	return NULL;
}
