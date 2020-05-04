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

#define UI_FILES(a) ((struct ui_files) {	\
	.files = a,				\
	.count = ARRAY_SIZE(a),			\
})

static const char *const empty_files[] = {};

/******************************************************************************/
/* VB2_SCREEN_BLANK */

static vb2_error_t draw_blank(const struct ui_screen_info *screen,
			      const struct ui_state *state,
			      const struct ui_state *prev_state)
{
	clear_screen(&ui_color_bg);
	return VB2_SUCCESS;
}

static const struct ui_screen_info blank_screen = {
	.id = VB2_SCREEN_BLANK,
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
	.desc = UI_FILES(firmware_sync_desc),
	.mesg = "Please do not power off your device.\n"
		"Your system is applying a critical update.",
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_BROKEN */

static const char *const broken_desc[] = {
	"broken_desc0.bmp",
	"broken_desc1.bmp",
};

static const struct ui_screen_info broken_screen = {
	.id = VB2_SCREEN_RECOVERY_BROKEN,
	.icon = UI_ICON_TYPE_INFO,
	.title = "broken_title.bmp",
	.desc = UI_FILES(broken_desc),
	.menu = UI_FILES(empty_files),
	.has_advanced_options = 1,
	.mesg = "Something is wrong.\n"
		"Please remove all connected devices and hold down Esc,\n"
		"Refresh, and Power to initiate recovery.",
};

/******************************************************************************/
/* VB2_SCREEN_ADVANCED_OPTIONS */

static const char *const advanced_options_menu[] = {
	"btn_dev_mode.bmp",
	/* TODO(yupingso): Add debug info & firmware log items. */
	"btn_back.bmp",
};

static const struct ui_screen_info advanced_options_screen = {
	.id = VB2_SCREEN_ADVANCED_OPTIONS,
	.icon = UI_ICON_TYPE_NONE,
	.title = "adv_options_title.bmp",
	.desc = UI_FILES(empty_files),
	.menu = UI_FILES(advanced_options_menu),
	.mesg = "Advanced options",
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_SELECT */

static const char *const recovery_select_desc[] = {
	"rec_sel_desc0.bmp",
	"rec_sel_desc1.bmp",
};

static const char *const recovery_select_menu[] = {
	"btn_rec_by_phone.bmp",
	"btn_rec_by_disk.bmp",
};

static const struct ui_screen_info recovery_select_screen = {
	.id = VB2_SCREEN_RECOVERY_SELECT,
	.icon = UI_ICON_TYPE_INFO,
	.title = "rec_sel_title.bmp",
	.desc = UI_FILES(recovery_select_desc),
	.menu = UI_FILES(recovery_select_menu),
	.has_advanced_options = 1,
	.mesg = "Select how you'd like to recover.\n"
		"You can recover using a USB drive or an SD card.",
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_DISK_STEP1 */

static const char *const recovery_disk_step1_desc[] = {
	"rec_disk_step1_desc0.bmp",
	"rec_disk_step1_desc1.bmp",
	/* TODO(yupingso): Draw rec_disk_step1_desc2_low_bat.bmp if battery is
	   low. */
	"rec_disk_step1_desc2.bmp",
};

static const char *const recovery_disk_step1_menu[] = {
	"btn_next.bmp",
	"btn_back.bmp",
};

static const struct ui_screen_info recovery_disk_step1_screen = {
	.id = VB2_SCREEN_RECOVERY_DISK_STEP1,
	.icon = UI_ICON_TYPE_STEP,
	.step = 1,
	.num_steps = 3,
	.title = "rec_disk_step2_title.bmp",
	.desc = UI_FILES(recovery_disk_step1_desc),
	.menu = UI_FILES(recovery_disk_step1_menu),
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

static const char *const recovery_disk_step2_menu[] = {
	"btn_next.bmp",
	"btn_back.bmp",
};

static const struct ui_screen_info recovery_disk_step2_screen = {
	.id = VB2_SCREEN_RECOVERY_DISK_STEP2,
	.icon = UI_ICON_TYPE_STEP,
	.step = 2,
	.num_steps = 3,
	.title = "rec_disk_step2_title.bmp",
	.desc = UI_FILES(recovery_disk_step2_desc),
	.menu = UI_FILES(recovery_disk_step2_menu),
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

static const char *const recovery_disk_step3_menu[] = {
	"btn_back.bmp",
};

static const struct ui_screen_info recovery_disk_step3_screen = {
	.id = VB2_SCREEN_RECOVERY_DISK_STEP3,
	.icon = UI_ICON_TYPE_STEP,
	.step = 3,
	.num_steps = 3,
	.title = "rec_disk_step3_title.bmp",
	.desc = UI_FILES(recovery_disk_step3_desc),
	.menu = UI_FILES(recovery_disk_step3_menu),
	.mesg = "Do you have your external disk ready?\n"
		"If your external disk is ready with a recovery image, plug\n"
		"it into the device to start the recovery process.",
};

/******************************************************************************/
/* VB2_SCREEN_RECOVERY_INVALID */

static vb2_error_t draw_recovery_invalid_desc(
	const struct ui_screen_info *screen,
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
	const struct ui_files desc = vb2api_phone_recovery_enabled(ctx) ?
		UI_FILES(desc_files) : UI_FILES(desc_no_phone_files);

	return ui_draw_desc(&desc, state, height);
}

static const struct ui_screen_info recovery_invalid_screen = {
	.id = VB2_SCREEN_RECOVERY_INVALID,
	.icon = UI_ICON_TYPE_STEP,
	.step = -3,
	.num_steps = 3,
	.title = "rec_invalid_title.bmp",
	.desc = UI_FILES(empty_files),
	.menu = UI_FILES(empty_files),
	.draw_desc = draw_recovery_invalid_desc,
	.mesg = "No valid image detected.\n"
		"Make sure your external disk has a valid recovery image,\n"
		"and re-insert the disk when ready.",
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
	&broken_screen,
	&advanced_options_screen,
	&recovery_select_screen,
	&recovery_disk_step1_screen,
	&recovery_disk_step2_screen,
	&recovery_disk_step3_screen,
	&recovery_invalid_screen,
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
