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

#include "vboot/ui.h"
#include "vboot/util/commonparams.h"

#define UI_DESC(a) ((struct ui_desc){	\
	.count = ARRAY_SIZE(a),		\
	.files = a,			\
})

#define UI_MENU(a) ((struct ui_menu){	\
	.num_items = ARRAY_SIZE(a),		\
	.items = a,			\
})

#define LANGUAGE_SELECT_ITEM ((struct ui_menu_item){	\
	.file = NULL,					\
	.type = UI_MENU_ITEM_TYPE_LANGUAGE,		\
})

#define BACK_ITEM ((struct ui_menu_item){	\
	.file = "btn_back.bmp",			\
	.type = UI_MENU_ITEM_TYPE_PRIMARY,	\
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
	.no_arrow = 1,				\
})

/******************************************************************************/
/* VB2_SCREEN_BLANK */

static vb2_error_t draw_blank(const struct ui_state *state,
			      const struct ui_state *prev_state)
{
	clear_screen(&ui_color_bg);
	return VB2_SUCCESS;
}

static const struct ui_screen_info blank_screen = {
	.id = VB2_SCREEN_BLANK,
	.no_footer = 1,
	.draw = draw_blank,
	.mesg = NULL,
};

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
	struct ui_bitmap bitmap;

	/*
	 * Call default drawing function to clear the screen if necessary, and
	 * draw the footer.
	 */
	VB2_TRY(ui_draw_default(state, prev_state));

	x_begin = UI_MARGIN_H;
	x_end = UI_SCALE - UI_MARGIN_H;
	box_width = x_end - x_begin;
	box_height = UI_LANG_MENU_BOX_HEIGHT;

	y_begin = UI_MARGIN_TOP + UI_LANG_BOX_HEIGHT + UI_LANG_MENU_MARGIN_TOP;
	y_end = UI_SCALE - UI_MARGIN_BOTTOM - UI_FOOTER_HEIGHT -
		UI_FOOTER_MARGIN_TOP;
	num_lang_per_page = (y_end - y_begin) / box_height;
	menu_height = box_height * num_lang_per_page;
	y_end = y_begin + menu_height;  /* Correct for integer division error */

	/* Get current locale_id */
	num_lang = ui_get_locale_count();
	if (num_lang == 0) {
		UI_ERROR("Locale count is 0\n");
		return VB2_ERROR_UI_INVALID_ARCHIVE;
	}

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
		/* Solid box */
		VB2_TRY(ui_draw_rounded_box(x_begin, y, box_width, box_height,
					    focused ? &ui_color_button :
					    &ui_color_lang_menu_bg,
					    0, 0, reverse));
		/* Separator between languages */
		if (id > id_begin)
			/* TODO(yupingso): Ensure consistent thickness */
			/* TODO(b/147424699): Reduce redraw of separator */
			VB2_TRY(ui_draw_rounded_box(x_begin, y, box_width,
						    border_thickness,
						    &ui_color_lang_menu_border,
						    0, 0, reverse));
		/* Text */
		y_center = y + box_height / 2;
		VB2_TRY(ui_get_locale_info(id, &locale));
		VB2_TRY(ui_get_language_name_bitmap(locale->code, 0, focused,
						    &bitmap));
		VB2_TRY(ui_draw_bitmap(&bitmap, x, y_center,
				       UI_SIZE_AUTO, UI_LANG_MENU_TEXT_HEIGHT,
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
	"broken_desc0.bmp",
	"broken_desc1.bmp",
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
	/* TODO(yupingso): Add debug info & firmware log items. */
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

static const char *const recovery_select_desc[] = {
	"rec_sel_desc0.bmp",
	"rec_sel_desc1.bmp",
};

static const struct ui_menu_item recovery_select_items[] = {
	LANGUAGE_SELECT_ITEM,
	{ "btn_rec_by_phone.bmp" },
	{ "btn_rec_by_disk.bmp" },
	ADVANCED_OPTIONS_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info recovery_select_screen = {
	.id = VB2_SCREEN_RECOVERY_SELECT,
	.icon = UI_ICON_TYPE_INFO,
	.title = "rec_sel_title.bmp",
	.desc = UI_DESC(recovery_select_desc),
	.menu = UI_MENU(recovery_select_items),
	.mesg = "Select how you'd like to recover.\n"
		"You can recover using a USB drive or an SD card.",
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_PHONE_STEP1 */

static const char *const recovery_phone_step1_desc[] = {
	"rec_phone_step1_desc0.bmp",
	"rec_phone_step1_desc1.bmp",
	/* TODO(yupingso): Draw rec_step1_desc2_low_bat.bmp if battery is
	   low. */
	"rec_step1_desc2.bmp",
};

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
	.desc = UI_DESC(recovery_phone_step1_desc),
	.menu = UI_MENU(recovery_phone_step1_items),
	.mesg = "To proceed with the recovery process, you’ll need\n"
		"1. An Android phone with internet access\n"
		"2. A USB cable which connects your phone and this device\n"
		"We also recommend that you connect to a power source during\n"
		"the recovery process.",
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_PHONE_STEP2 */

static vb2_error_t draw_recovery_phone_step2(
	const struct ui_state *state,
	const struct ui_state *prev_state)
{
	const int32_t x = UI_SCALE - UI_MARGIN_H - UI_REC_QR_MARGIN_R;
	const int32_t y = UI_MARGIN_TOP + UI_LANG_BOX_HEIGHT +
		UI_LANG_MARGIN_BOTTOM + UI_ICON_HEIGHT + UI_ICON_MARGIN_BOTTOM +
		UI_TITLE_TEXT_HEIGHT + UI_TITLE_MARGIN_BOTTOM;
	const int32_t w = UI_SIZE_AUTO;
	/* Match height from top of descriptions to bottom of "Back" button. */
	const int32_t h = (UI_DESC_TEXT_HEIGHT
			   + UI_DESC_TEXT_LINE_SPACING) * 4 +
		UI_DESC_MARGIN_BOTTOM + UI_BUTTON_HEIGHT;
	uint32_t flags = PIVOT_H_RIGHT | PIVOT_V_TOP;
	const int reverse = state->locale->rtl;
	struct ui_bitmap bitmap;

	VB2_TRY(ui_draw_default(state, prev_state));
	VB2_TRY(ui_get_bitmap("qr_rec_phone.bmp", NULL, 0, &bitmap));
	VB2_TRY(ui_draw_bitmap(&bitmap, x, y, w, h, flags, reverse));
	return VB2_SUCCESS;
}

static const char *const recovery_phone_step2_desc[] = {
	"rec_phone_step2_desc0.bmp",
	"rec_phone_step2_desc1.bmp",
	"rec_phone_step2_desc2.bmp",
	"rec_phone_step2_desc3.bmp",
};

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
	.desc = UI_DESC(recovery_phone_step2_desc),
	.menu = UI_MENU(recovery_phone_step2_items),
	.draw = draw_recovery_phone_step2,
	.mesg = "Download the Chrome OS recovery app on your Android phone\n"
		"by plugging in your phone or by scanning the QR code on the\n"
		"screen. Once you launch the app, connect your phone to your\n"
		"device and recovery will start automatically."
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_DISK_STEP1 */

static const char *const recovery_disk_step1_desc[] = {
	"rec_disk_step1_desc0.bmp",
	"rec_disk_step1_desc1.bmp",
	/* TODO(yupingso): Draw rec_step1_desc2_low_bat.bmp if battery is
	   low. */
	"rec_step1_desc2.bmp",
};

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
	.desc = UI_DESC(recovery_disk_step1_desc),
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
	int32_t *height)
{
	struct vb2_context *ctx = vboot_get_context();
	static const char *const desc_files[] = {
		"rec_invalid_desc0.bmp",
		"rec_invalid_desc1.bmp",
	};
	static const char *const desc_no_phone_files[] = {
		"rec_invalid_disk_desc0.bmp",
		"rec_invalid_disk_desc1.bmp",
	};
	const struct ui_desc desc = vb2api_phone_recovery_ui_enabled(ctx) ?
		UI_DESC(desc_files) : UI_DESC(desc_no_phone_files);

	return ui_draw_desc(&desc, state, height);
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

static const char *const developer_mode_desc[] = {
	"dev_desc0.bmp",
	"dev_desc1.bmp",
};

static const struct ui_menu_item developer_mode_items[] = {
	LANGUAGE_SELECT_ITEM,
	{ "btn_secure_mode.bmp" },
	{ "btn_int_disk.bmp" },
	{ "btn_ext_disk.bmp" },
	ADVANCED_OPTIONS_ITEM,
	POWER_OFF_ITEM,
};

static const struct ui_screen_info developer_mode_screen = {
	.id = VB2_SCREEN_DEVELOPER_MODE,
	.icon = UI_ICON_TYPE_DEV_MODE,
	.title = "dev_title.bmp",
	.desc = UI_DESC(developer_mode_desc),
	.menu = UI_MENU(developer_mode_items),
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
		"plug it into the device to boot."
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
		"and re-insert the disk when ready."
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
	&blank_screen,
	&firmware_sync_screen,
	&language_select_screen,
	&broken_screen,
	&advanced_options_screen,
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
};

const struct ui_screen_info *ui_get_screen_info(enum vb2_screen screen_id)
{
	int i;
	for (i = 0; i < ARRAY_SIZE(screens); i++) {
		if (screens[i]->id == screen_id)
			return screens[i];
	}
	return NULL;
}
