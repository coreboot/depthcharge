/*
 * Copyright 2012 Google Inc.
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

#include <assert.h>
#include <libpayload.h>
#include <sysinfo.h>
#include <vboot_api.h>

#include "base/cleanup_funcs.h"
#include "drivers/tpm/tpm.h"
#include "drivers/ec/cros/ec.h"
#include "drivers/video/coreboot_fb.h"
#include "drivers/video/display.h"
#include "vboot/firmware_id.h"
#include "vboot/screens.h"
#include "vboot/util/commonparams.h"

/*
 * When we don't know how much of a draw failed, draw a colored stripe as a
 * fallback so the screen can be identified in a pinch but it doesn't cover up
 * anything that was drawn successfully.
 */
void draw_fallback(uint32_t screen_type, int selected_index)
{
	const int32_t stripe_height = CANVAS_SCALE / 50;  // 2% screen height
	struct rect stripe = {
		{ .x = 0, .y = 0 },
		{ .width = CANVAS_SCALE, .height = stripe_height },
	};
	switch (screen_type) {
	case VB_SCREEN_BLANK:
		draw_box(&stripe, &color_black);
		break;
	case VB_SCREEN_DEVELOPER_WARNING_MENU:
	case VB_SCREEN_DEVELOPER_WARNING:
		draw_box(&stripe, &color_pink);
		break;
	case VB_SCREEN_RECOVERY_INSERT:
		draw_box(&stripe, &color_yellow);
		break;
	case VB_SCREEN_RECOVERY_NO_GOOD:
		draw_box(&stripe, &color_orange);
		break;
	case VB_SCREEN_RECOVERY_TO_DEV:
	case VB_SCREEN_RECOVERY_TO_DEV_MENU:
		draw_box(&stripe, &color_violet);
		break;
	case VB_SCREEN_DEVELOPER_TO_NORM:
	case VB_SCREEN_DEVELOPER_TO_NORM_MENU:
		draw_box(&stripe, &color_green);
		break;
	case VB_SCREEN_WAIT:
		draw_box(&stripe, &color_blue);
		break;
	case VB_SCREEN_OS_BROKEN:
		draw_box(&stripe, &color_red);
		break;
	case VB_SCREEN_LANGUAGES_MENU:
		draw_box(&stripe, &color_brown);
		break;
	case VB_SCREEN_OPTIONS_MENU:
		draw_box(&stripe, &color_teal);
		break;
	case VB_SCREEN_ALT_FW_PICK:
	case VB_SCREEN_ALT_FW_MENU:
		draw_box(&stripe, &color_light_blue);
		break;
	default:
		draw_box(&stripe, &color_grey);
		break;
	}

	if (selected_index >= 0) {
		struct rect cursor = {
			{ .x = selected_index * stripe_height, .y = 0 },
			{ .width = stripe_height, .height = stripe_height },
		};
		draw_box(&cursor, &color_black);
	}
}

VbError_t VbExDisplayScreen(uint32_t screen_type, uint32_t locale,
			    const VbScreenData *data)
{
	int ret = vboot_draw_screen(screen_type, locale, data);
	if (ret != VBERROR_SUCCESS)
		draw_fallback(screen_type, -1);

	return ret;
}

VbError_t VbExDisplayMenu(uint32_t screen_type, uint32_t locale,
			  uint32_t selected_index, uint32_t disabled_idx_mask,
			  uint32_t redraw_base)
{
	int ret =  vboot_draw_ui(screen_type, locale, selected_index,
				 disabled_idx_mask, redraw_base);
	if (ret != VBERROR_SUCCESS)
		draw_fallback(screen_type, selected_index);

	return ret;
}

VbError_t VbExDisplayDebugInfo(const char *info_str, int full_info)
{
	char buf[1024];

	if (!full_info) {
		strncpy(buf, info_str, sizeof(buf) - 2);
		buf[sizeof(buf) - 1] = '\0';
	} else {
		char *tpm_str = NULL;
		char batt_pct_str[16];

		if (CONFIG(MOCK_TPM))
			tpm_str = "MOCK TPM";
		else if (CONFIG(DRIVER_TPM))
			tpm_str = tpm_report_state();

		if (!tpm_str)
			tpm_str = "(unsupported)";

		if (!CONFIG(DRIVER_EC_CROS))
			strcpy(batt_pct_str, "(unsupported)");
		else {
			uint32_t batt_pct;
			if (cros_ec_read_batt_state_of_charge(&batt_pct))
				strcpy(batt_pct_str, "(read failure)");
			else
				snprintf(batt_pct_str, sizeof(batt_pct_str),
					 "%u%%", batt_pct);
		}
		snprintf(buf, sizeof(buf),
			 "%s"	// vboot output includes newline
			 "read-only firmware id: %s\n"
			 "active firmware id: %s\n"
			 "battery level: %s\n"
			 "TPM state: %s\n",
			 info_str, get_ro_fw_id(), get_active_fw_id(),
			 batt_pct_str, tpm_str);
	}
	return vboot_print_string(buf);
}

VbError_t VbExGetLocalizationCount(uint32_t *count)
{
	*count = vboot_get_locale_count();
	return VBERROR_SUCCESS;
}
