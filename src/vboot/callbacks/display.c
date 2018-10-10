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

#include "config.h"
#include "base/cleanup_funcs.h"
#include "base/graphics.h"
#include "drivers/tpm/tpm.h"
#include "drivers/video/coreboot_fb.h"
#include "drivers/video/display.h"
#include "vboot/firmware_id.h"
#include "vboot/screens.h"
#include "vboot/util/commonparams.h"

static void print_string_newline(const char *str)
{
	vboot_print_string(str);
	vboot_print_string("\n");
}

void print_on_center(const char *msg)
{
	unsigned int rows, cols;
	video_get_rows_cols(&rows, &cols);
	video_console_set_cursor((cols - strlen(msg)) / 2, rows / 2);
	vboot_print_string(msg);
}

VbError_t VbExDisplayScreen(uint32_t screen_type, uint32_t locale)
{
	const char *msg = NULL;

	if (vboot_draw_screen(screen_type, locale) == CBGFX_SUCCESS)
		return VBERROR_SUCCESS;

	/*
	 * Show the debug messages for development. It is a backup method
	 * when GBB does not contain a full set of bitmaps.
	 */
	switch (screen_type) {
	case VB_SCREEN_BLANK:
		// clear the screen
		video_console_clear();
		break;
	case VB_SCREEN_DEVELOPER_WARNING:
		msg = "developer mode warning";
		break;
	case VB_SCREEN_RECOVERY_INSERT:
		msg = "insert recovery image";
		break;
	case VB_SCREEN_RECOVERY_NO_GOOD:
		msg = "insert image invalid";
		break;
	case VB_SCREEN_WAIT:
		msg = "wait for ec update";
		break;
	default:
		printf("Not a valid screen type: %d.\n", screen_type);
		return VBERROR_INVALID_SCREEN_INDEX;
	}

	if (msg)
		print_on_center(msg);

	return VBERROR_SUCCESS;
}

VbError_t VbExDisplayMenu(uint32_t screen_type, uint32_t locale,
			  uint32_t selected_index, uint32_t disabled_idx_mask,
			  uint32_t redraw_base)
{
	return vboot_draw_ui(screen_type, locale, selected_index,
			     disabled_idx_mask, redraw_base);
}

VbError_t VbExDisplayDebugInfo(const char *info_str)
{
	video_console_set_cursor(0, 0);
	vboot_print_string(info_str);

	vboot_print_string("read-only firmware id: ");
	print_string_newline(get_ro_fw_id());

	vboot_print_string("active firmware id: ");
	const char *id = get_active_fw_id();
	if (id == NULL)
		id = "NOT FOUND";
	print_string_newline(id);

	vboot_print_string("TPM state: ");
	if (IS_ENABLED(CONFIG_DRIVER_TPM)) {
		char *tpm_state = tpm_report_state();
		if (tpm_state) {
			vboot_print_string(tpm_state);
			free(tpm_state);
		} else {
			print_string_newline(" not supported");
		}
	} else {
		if (IS_ENABLED(CONFIG_MOCK_TPM))
			print_string_newline(" MOCK TPM");
		else
			print_string_newline(" not supported");
	}

	return VBERROR_SUCCESS;
}

VbError_t VbExGetLocalizationCount(uint32_t *count)
{
	*count = vboot_get_locale_count();
	return VBERROR_SUCCESS;
}
