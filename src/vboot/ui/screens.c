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
	.file = NULL,					\
	.type = UI_MENU_ITEM_TYPE_LANGUAGE,		\
})

#define PAGE_UP_ITEM ((struct ui_menu_item) {			\
	.file = "btn_page_up.bmp",				\
	.disabled_help_text_file = "page_up_disabled_help.bmp",	\
})

#define PAGE_DOWN_ITEM ((struct ui_menu_item) {				\
	.file = "btn_page_down.bmp",					\
	.disabled_help_text_file = "page_down_disabled_help.bmp",	\
})

#define BACK_ITEM ((struct ui_menu_item){	\
	.file = "btn_back.bmp",			\
})

#define ADVANCED_OPTIONS_ITEM ((struct ui_menu_item){	\
	.file = "btn_adv_options.bmp",			\
	.type = UI_MENU_ITEM_TYPE_SECONDARY,		\
	.icon_file = "ic_settings.bmp",			\
})

#define POWER_OFF_ITEM ((struct ui_menu_item){	\
	.file = "btn_power_off.bmp",		\
	.type = UI_MENU_ITEM_TYPE_SECONDARY,	\
	.icon_file = "ic_power.bmp",		\
	.flags = UI_MENU_ITEM_FLAG_NO_ARROW,	\
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

	buf = ui_log_get_page_content(state->log, state->current_page);
	if (!buf)
		return VB2_ERROR_UI_LOG_INIT;
	buf_len = strlen(buf);
	/* Redraw only if screen or text changed. */
	if (!prev_state || state->screen->id != prev_state->screen->id ||
	    state->error_code != prev_state->error_code ||
	    !prev_buf || buf_len != prev_buf_len ||
	    strncmp(buf, prev_buf, buf_len))
		rv = ui_draw_log_textbox(buf, state, y);
	else
		*y = prev_y;

	if (prev_buf)
		free(prev_buf);
	prev_buf = buf;
	prev_buf_len = buf_len;
	prev_y = *y;

	return rv;
}

/******************************************************************************/
/* VB2_SCREEN_FIRMWARE_SYNC */

static const char *const firmware_sync_desc[] = {
	"firmware_sync_desc.bmp",
};

static const struct ui_screen_info firmware_sync_screen = {
	.id = VB2_SCREEN_FIRMWARE_SYNC,
	.icon = UI_ICON_TYPE_NONE,
	.title = "firmware_sync_title.bmp",
	.desc = UI_DESC(firmware_sync_desc),
	.no_footer = 1,
	.mesg = "Please do not power off your device.\n"
		"Your system is applying a critical update.",
};

/******************************************************************************/
/* VB2_SCREEN_LANGUAGE_SELECT */

static vb2_error_t draw_language_select(const struct ui_state *state,
					const struct ui_state *prev_state)
{
	int id;
	const int reverse = state->locale->rtl;
	uint32_t num_lang;
	uint32_t locale_id;
	int32_t x, x_begin, x_end, y, y_begin, y_end, y_center, menu_height, h;
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
	x = x_end - UI_LANG_MENU_SCROLLBAR_MARGIN_RIGHT -
		UI_LANG_MENU_SCROLLBAR_WIDTH;
	y = y_begin + id_begin * menu_height / num_lang;
	h = num_lang_per_page * menu_height / num_lang;
	VB2_TRY(ui_draw_rounded_box(x, y, UI_LANG_MENU_SCROLLBAR_WIDTH, h,
				    &ui_color_lang_scrollbar,
				    0, UI_LANG_MENU_SCROLLBAR_CORNER_RADIUS,
				    reverse));

	return VB2_SUCCESS;
}

static const struct ui_screen_info language_select_screen = {
	.id = VB2_SCREEN_LANGUAGE_SELECT,
	.draw = draw_language_select,
	.mesg = "Language selection",
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_BROKEN */

static const char *const broken_desc[] = {
	"broken_desc.bmp",
};

static const struct ui_menu_item broken_items[] = {
	LANGUAGE_SELECT_ITEM,
	ADVANCED_OPTIONS_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info broken_screen = {
	.id = VB2_SCREEN_RECOVERY_BROKEN,
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
/* VB2_SCREEN_ADVANCED_OPTIONS */

static const struct ui_menu_item advanced_options_items[] = {
	LANGUAGE_SELECT_ITEM,
	{ "btn_dev_mode.bmp" },
	{ "btn_debug_info.bmp" },
	{ "btn_firmware_log.bmp" },
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info advanced_options_screen = {
	.id = VB2_SCREEN_ADVANCED_OPTIONS,
	.icon = UI_ICON_TYPE_NONE,
	.title = "adv_options_title.bmp",
	.menu = UI_MENU(advanced_options_items),
	.mesg = "Advanced options",
};

/******************************************************************************/
/* VB2_SCREEN_DEBUG_INFO */

static const struct ui_menu_item debug_info_items[] = {
	LANGUAGE_SELECT_ITEM,
	PAGE_UP_ITEM,
	PAGE_DOWN_ITEM,
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info debug_info_screen = {
	.id = VB2_SCREEN_DEBUG_INFO,
	.icon = UI_ICON_TYPE_NONE,
	.title = "debug_info_title.bmp",
	.menu = UI_MENU(debug_info_items),
	.draw_desc = draw_log_desc,
	.mesg = "Debug info",
};

/******************************************************************************/
/* VB2_SCREEN_FIRMWARE_LOG */

static const struct ui_menu_item firmware_log_items[] = {
	LANGUAGE_SELECT_ITEM,
	PAGE_UP_ITEM,
	PAGE_DOWN_ITEM,
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info firmware_log_screen = {
	.id = VB2_SCREEN_FIRMWARE_LOG,
	.icon = UI_ICON_TYPE_NONE,
	.title = "firmware_log_title.bmp",
	.menu = UI_MENU(firmware_log_items),
	.draw_desc = draw_log_desc,
	.mesg = "Firmware log",
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_TO_DEV */

static const char *const recovery_to_dev_desc[] = {
	"rec_to_dev_desc0.bmp",
	"rec_to_dev_desc1.bmp",
};

static const struct ui_menu_item recovery_to_dev_items[] = {
	LANGUAGE_SELECT_ITEM,
	{ "btn_confirm.bmp" },
	{ "btn_cancel.bmp" },
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_to_dev_screen = {
	.id = VB2_SCREEN_RECOVERY_TO_DEV,
	.icon = UI_ICON_TYPE_INFO,
	.title = "rec_to_dev_title.bmp",
	.desc = UI_DESC(recovery_to_dev_desc),
	.menu = UI_MENU(recovery_to_dev_items),
	.mesg = "You are attempting to enable developer mode\n"
		"This involves erasing all data from your device,\n"
		"and will make your device insecure.\n"
		"Select \"Confirm\" or press the RECOVERY/POWER button to\n"
		"enable developer mode,\n"
		"or select \"Cancel\" to remain protected",
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_SELECT */

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

static const struct ui_menu_item recovery_select_items[] = {
	LANGUAGE_SELECT_ITEM,
	{ "btn_rec_by_phone.bmp" },
	{ "btn_rec_by_disk.bmp" },
	{
		.file = "btn_launch_diag.bmp",
		.type = UI_MENU_ITEM_TYPE_SECONDARY,
		.icon_file = "ic_search.bmp",
		.flags = UI_MENU_ITEM_FLAG_NO_ARROW,
	},
	ADVANCED_OPTIONS_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_select_screen = {
	.id = VB2_SCREEN_RECOVERY_SELECT,
	.icon = UI_ICON_TYPE_INFO,
	.title = "rec_sel_title.bmp",
	.draw_desc = draw_recovery_select_desc,
	.menu = UI_MENU(recovery_select_items),
	.mesg = "Select how you'd like to recover.\n"
		"You can recover using a USB drive or an SD card.",
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_PHONE_STEP1 */

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
	{ "btn_next.bmp" },
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_phone_step1_screen = {
	.id = VB2_SCREEN_RECOVERY_PHONE_STEP1,
	.icon = UI_ICON_TYPE_STEP,
	.step = 1,
	.num_steps = 3,
	.title = "rec_step1_title.bmp",
	.draw_desc = draw_recovery_phone_step1_desc,
	.menu = UI_MENU(recovery_phone_step1_items),
	.mesg = "To proceed with the recovery process, you’ll need\n"
		"1. An Android phone with internet access\n"
		"2. A USB cable which connects your phone and this device\n"
		"We also recommend that you connect to a power source during\n"
		"the recovery process.",
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_PHONE_STEP2 */

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
	.id = VB2_SCREEN_RECOVERY_PHONE_STEP2,
	.icon = UI_ICON_TYPE_STEP,
	.step = 2,
	.num_steps = 3,
	.title = "rec_phone_step2_title.bmp",
	.draw_desc = draw_recovery_phone_step2_desc,
	.menu = UI_MENU(recovery_phone_step2_items),
	.mesg = "Download the Chrome OS recovery app on your Android phone\n"
		"by plugging in your phone or by scanning the QR code on the\n"
		"right. Once you launch the app, connect your phone to your\n"
		"device and recovery will start automatically.",
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_DISK_STEP1 */

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
	{ "btn_next.bmp" },
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_disk_step1_screen = {
	.id = VB2_SCREEN_RECOVERY_DISK_STEP1,
	.icon = UI_ICON_TYPE_STEP,
	.step = 1,
	.num_steps = 3,
	.title = "rec_step1_title.bmp",
	.draw_desc = draw_recovery_disk_step1_desc,
	.menu = UI_MENU(recovery_disk_step1_items),
	.mesg = "To proceed with the recovery process, you'll need\n"
		"1. An external storage disk such as a USB drive or an SD card"
		"\n2. An additional device with internet access\n"
		"We also recommend that you connect to a power source during\n"
		"the recovery process.",
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_DISK_STEP2 */

static const char *const recovery_disk_step2_desc[] = {
	"rec_disk_step2_desc0.bmp",
	"rec_disk_step2_desc1.bmp",
	"rec_disk_step2_desc2.bmp",
};

static const struct ui_menu_item recovery_disk_step2_items[] = {
	LANGUAGE_SELECT_ITEM,
	{ "btn_next.bmp" },
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_disk_step2_screen = {
	.id = VB2_SCREEN_RECOVERY_DISK_STEP2,
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
/* VB2_SCREEN_RECOVERY_DISK_STEP3 */

static const char *const recovery_disk_step3_desc[] = {
	"rec_disk_step3_desc0.bmp",
};

static const struct ui_menu_item recovery_disk_step3_items[] = {
	LANGUAGE_SELECT_ITEM,
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_disk_step3_screen = {
	.id = VB2_SCREEN_RECOVERY_DISK_STEP3,
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
/* VB2_SCREEN_RECOVERY_INVALID */

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
	.id = VB2_SCREEN_RECOVERY_INVALID,
	.icon = UI_ICON_TYPE_STEP,
	.step = -3,
	.num_steps = 3,
	.title = "rec_invalid_title.bmp",
	.draw_desc = draw_recovery_invalid_desc,
	.menu = UI_MENU(recovery_invalid_items),
	.mesg = "No valid image detected.\n"
		"Make sure your external disk has a valid recovery image,\n"
		"and re-insert the disk when ready.",
};

/******************************************************************************/
/* VB2_SCREEN_DEVELOPER_MODE */

#define DEVELOPER_MODE_ITEM_RETURN_TO_SECURE 1

static const struct ui_menu_item developer_mode_items[] = {
	LANGUAGE_SELECT_ITEM,
	[DEVELOPER_MODE_ITEM_RETURN_TO_SECURE] = { "btn_secure_mode.bmp" },
	{ "btn_int_disk.bmp" },
	{ "btn_ext_disk.bmp" },
	{ "btn_alt_bootloader.bmp" },
	ADVANCED_OPTIONS_ITEM,
	POWER_OFF_ITEM,
};

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

static const struct ui_screen_info developer_mode_screen = {
	.id = VB2_SCREEN_DEVELOPER_MODE,
	.icon = UI_ICON_TYPE_DEV_MODE,
	.title = "dev_title.bmp",
	.menu = UI_MENU(developer_mode_items),
	.draw_desc = draw_developer_mode_desc,
	.mesg = "You are in developer mode\n"
		"To return to the recommended secure mode,\n"
		"select \"Return to secure mode\" below.\n"
		"After timeout, the device will automatically boot from\n"
		"the default boot target.",
};

/******************************************************************************/
/* VB2_SCREEN_DEVELOPER_TO_NORM */

static const char *const developer_to_norm_desc[] = {
	"dev_to_norm_desc0.bmp",
	"dev_to_norm_desc1.bmp",
};

static const struct ui_menu_item developer_to_norm_items[] = {
	LANGUAGE_SELECT_ITEM,
	{ "btn_confirm.bmp" },
	{ "btn_cancel.bmp" },
	POWER_OFF_ITEM,
};

static const struct ui_screen_info developer_to_norm_screen = {
	.id = VB2_SCREEN_DEVELOPER_TO_NORM,
	.icon = UI_ICON_TYPE_RESTART,
	.title = "dev_to_norm_title.bmp",
	.desc = UI_DESC(developer_to_norm_desc),
	.menu = UI_MENU(developer_to_norm_items),
	.mesg = "Confirm returning to secure mode.\n"
		"This option will disable developer mode and restore your\n"
		"device to its original state.\n"
		"Your user data will be wiped in the process.",
};

/******************************************************************************/
/* VB2_SCREEN_DEVELOPER_BOOT_EXTERNAL */

static const char *const developer_boot_external_desc[] = {
	"dev_boot_ext_desc0.bmp",
};

static const struct ui_menu_item developer_boot_external_items[] = {
	LANGUAGE_SELECT_ITEM,
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info developer_boot_external_screen = {
	.id = VB2_SCREEN_DEVELOPER_BOOT_EXTERNAL,
	.icon = UI_ICON_TYPE_NONE,
	.title = "dev_boot_ext_title.bmp",
	.desc = UI_DESC(developer_boot_external_desc),
	.menu = UI_MENU(developer_boot_external_items),
	.mesg = "Plug in your external disk\n"
		"If your external disk is ready with a Chrome OS image,\n"
		"plug it into the device to boot.",
};

/******************************************************************************/
/* VB2_SCREEN_DEVELOPER_INVALID_DISK */

static const char *const developer_invalid_disk_desc[] = {
	"dev_invalid_disk_desc0.bmp",
};

static const struct ui_menu_item developer_invalid_disk_items[] = {
	LANGUAGE_SELECT_ITEM,
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info developer_invalid_disk_screen = {
	.id = VB2_SCREEN_DEVELOPER_INVALID_DISK,
	.icon = UI_ICON_TYPE_ERROR,
	.title = "dev_invalid_disk_title.bmp",
	.desc = UI_DESC(developer_invalid_disk_desc),
	.menu = UI_MENU(developer_invalid_disk_items),
	.mesg = "No valid image detected\n"
		"Make sure your external disk has a valid Chrome OS image,\n"
		"and re-insert the disk when ready.",
};

/******************************************************************************/
/* VB2_SCREEN_DEVELOPER_SELECT_ALTFW */

static const struct ui_menu_item developer_select_bootloader_items[] = {
	LANGUAGE_SELECT_ITEM,
};

static vb2_error_t get_bootloader_menu(struct ui_menu *ret_menu)
{
	struct altfw_info *node;
	ListNode *head;
	uint32_t num_bootloaders = 0;
	static struct ui_menu_item *items;
	static struct ui_menu menu;
	uint32_t i;
	static const struct ui_menu_item menu_before[] = {
		LANGUAGE_SELECT_ITEM,
	};
	static const struct ui_menu_item menu_after[] = {
		BACK_ITEM,
		POWER_OFF_ITEM,
	};

	*ret_menu = menu;

	/* Cached */
	if (menu.num_items > 0) {
		UI_INFO("Cached with %zu item(s)\n", menu.num_items);
		return VB2_SUCCESS;
	}

	if (ARRAY_SIZE(developer_select_bootloader_items) >
	    ARRAY_SIZE(menu_before) + ARRAY_SIZE(menu_after))
		return VB2_SUCCESS;

	head = payload_get_altfw_list();
	if (!head) {
		UI_ERROR("Failed to get altfw list\n");
		return VB2_SUCCESS;
	}

	list_for_each(node, *head, list_node) {
		/* Discount default seqnum=0, since it is duplicated. */
		if (node->seqnum)
			num_bootloaders++;
	}

	menu.num_items = num_bootloaders + ARRAY_SIZE(menu_before) +
			 ARRAY_SIZE(menu_after);
	items = malloc(menu.num_items * sizeof(struct ui_menu_item));
	if (!items) {
		UI_ERROR("Failed to malloc menu items for bootloaders\n");
		return VB2_ERROR_UI_MEMORY_ALLOC;
	}

	/* Copy prefix items to the begin. */
	memcpy(&items[0], menu_before, sizeof(menu_before));

	/* Copy bootloaders. */
	i = ARRAY_SIZE(menu_before);
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
			.text = name,
		};
		i++;
	}

	/* Copy postfix items to the end. */
	memcpy(&items[i], menu_after, sizeof(menu_after));

	menu.items = items;
	*ret_menu = menu;

	UI_INFO("Returned with %zu item(s)\n", menu.num_items);
	return VB2_SUCCESS;
}

static vb2_error_t draw_developer_select_bootloader(
	const struct ui_state *state,
	const struct ui_state *prev_state)
{
	struct ui_menu menu;
	int32_t y;
	vb2_error_t rv;

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
	VB2_TRY(get_bootloader_menu(&menu));
	rv = ui_draw_menu_items(&menu, state, prev_state, y);

	return rv;
}

static const struct ui_screen_info developer_select_bootloader_screen = {
	.id = VB2_SCREEN_DEVELOPER_SELECT_ALTFW,
	.icon = UI_ICON_TYPE_NONE,
	.title = "dev_select_bootloader_title.bmp",
	.menu = UI_MENU(developer_select_bootloader_items),
	.draw = draw_developer_select_bootloader,
	.mesg = "Select an alternate bootloader.",
};

/******************************************************************************/
/* VB2_SCREEN_DIAGNOSTICS */

static const char *const diagnostics_desc[] = {
	"diag_menu_desc0.bmp",
};

static const struct ui_menu_item diagnostics_items[] = {
	LANGUAGE_SELECT_ITEM,
	{ "btn_diag_storage_health.bmp" },
	{ "btn_diag_storage_short_test.bmp" },
	{ "btn_diag_storage_ext_test.bmp" },
	{ "btn_diag_memory_quick.bmp" },
	{ "btn_diag_memory_full.bmp" },
	POWER_OFF_ITEM,
};

static const struct ui_screen_info diagnostics_screen = {
	.id = VB2_SCREEN_DIAGNOSTICS,
	.icon = UI_ICON_TYPE_INFO,
	.title = "diag_menu_title.bmp",
	.desc = UI_DESC(diagnostics_desc),
	.menu = UI_MENU(diagnostics_items),
	.mesg = "Select the component you'd like to check",
};

/******************************************************************************/
/* VB2_SCREEN_DIAGNOSTICS_STORAGE_HEALTH */

static const struct ui_menu_item diagnostics_storage_health_items[] = {
	PAGE_UP_ITEM,
	PAGE_DOWN_ITEM,
	BACK_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info diagnostics_storage_health_screen = {
	.id = VB2_SCREEN_DIAGNOSTICS_STORAGE_HEALTH,
	.icon = UI_ICON_TYPE_NONE,
	.title = "diag_storage_health_title.bmp",
	.menu = UI_MENU(diagnostics_storage_health_items),
	.draw_desc = draw_log_desc,
	.mesg = "Storage health info",
};

/******************************************************************************/
/* VB2_SCREEN_DIAGNOSTICS_STORAGE_TEST_SHORT,
   VB2_SCREEN_DIAGNOSTICS_STORAGE_TEST_EXTENDED,
   VB2_SCREEN_DIAGNOSTICS_MEMORY_QUICK,
   VB2_SCREEN_DIAGNOSTICS_MEMORY_FULL */

static const struct ui_menu_item diagnostics_test_items[] = {
	PAGE_UP_ITEM,
	PAGE_DOWN_ITEM,
	{
		.file = "btn_back.bmp",
		.flags = UI_MENU_ITEM_FLAG_TRANSIENT,
	},
	{
		.file = "btn_cancel.bmp",
		.flags = UI_MENU_ITEM_FLAG_TRANSIENT,
	},
	POWER_OFF_ITEM,
};

static const struct ui_screen_info diagnostics_storage_test_short_screen = {
	.id = VB2_SCREEN_DIAGNOSTICS_STORAGE_TEST_SHORT,
	.icon = UI_ICON_TYPE_NONE,
	.title = "diag_storage_srt_test_title.bmp",
	.menu = UI_MENU(diagnostics_test_items),
	.draw_desc = draw_log_desc,
	.mesg = "Storage self test (short)",
};

static const struct ui_screen_info diagnostics_storage_test_extended_screen = {
	.id = VB2_SCREEN_DIAGNOSTICS_STORAGE_TEST_EXTENDED,
	.icon = UI_ICON_TYPE_NONE,
	.title = "diag_storage_ext_test_title.bmp",
	.menu = UI_MENU(diagnostics_test_items),
	.draw_desc = draw_log_desc,
	.mesg = "Storage self test (extended)",
};

static const struct ui_screen_info diagnostics_memory_quick_screen = {
	.id = VB2_SCREEN_DIAGNOSTICS_MEMORY_QUICK,
	.icon = UI_ICON_TYPE_NONE,
	.title = "diag_memory_quick_title.bmp",
	.menu = UI_MENU(diagnostics_test_items),
	.draw_desc = draw_log_desc,
	.mesg = "Memory check (quick)",
};

static const struct ui_screen_info diagnostics_memory_full_screen = {
	.id = VB2_SCREEN_DIAGNOSTICS_MEMORY_FULL,
	.icon = UI_ICON_TYPE_NONE,
	.title = "diag_memory_full_title.bmp",
	.menu = UI_MENU(diagnostics_test_items),
	.draw_desc = draw_log_desc,
	.mesg = "Memory check (full)",
};

/******************************************************************************/
/*
 * TODO(chromium:1035800): Refactor UI code across vboot and depthcharge.
 * Currently vboot and depthcharge maintain their own copies of menus/screens.
 * vboot detects keyboard input and controls the navigation among different menu
 * items and screens, while depthcharge performs the actual rendering of each
 * screen, based on the menu information passed from vboot.
 */
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

const struct ui_screen_info *ui_get_screen_info(enum vb2_screen screen_id)
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
