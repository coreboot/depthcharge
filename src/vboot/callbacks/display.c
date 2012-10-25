/*
 * Copyright (c) 2012 The Chromium OS Authors.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <vboot_api.h>

#include "drivers/coreboot_fb.h"
#include "image/fmap.h"
#include "vboot/util/acpi.h"

VbError_t VbExDisplayInit(uint32_t *width, uint32_t *height)
{
	printf("VbExDisplayInit called but not implemented.\n");
	return VBERROR_SUCCESS;
}

VbError_t VbExDisplayBacklight(uint8_t enable)
{
	printf("VbExDisplayBacklight called but not implemented.\n");
	return VBERROR_SUCCESS;
}

static void print_string(const char *str)
{
	int str_len = strlen(str);
	while (str_len--) {
		if (*str == '\n')
			video_console_putchar('\r');
		video_console_putchar(*str++);
	}
}

void print_on_center(const char *msg)
{
	unsigned int rows, cols;
	video_get_rows_cols(&rows, &cols);
	video_console_set_cursor((cols - strlen(msg)) / 2, rows / 2);
	print_string(msg);
}

VbError_t VbExDisplayScreen(uint32_t screen_type)
{
	const char *msg = NULL;

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
	case VB_SCREEN_DEVELOPER_EGG:
		msg = "easter egg";
		break;
	case VB_SCREEN_RECOVERY_REMOVE:
		msg = "remove inserted devices";
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

VbError_t VbExDisplayImage(uint32_t x, uint32_t y,
			   void *buffer, uint32_t buffersize)
{
	if (dc_corebootfb_draw_bitmap(x, y, buffer))
		return VBERROR_UNKNOWN;

	return VBERROR_SUCCESS;
}

VbError_t VbExDisplayDebugInfo(const char *info_str)
{
	chromeos_acpi_t *acpi_table = (chromeos_acpi_t *)lib_sysinfo.vdat_addr;

	video_console_set_cursor(0, 0);
	print_string(info_str);
	print_string("read-only firmware id: ");
	if (!fmap_ro_fwid) {
		print_string("NOT FOUND");
	} else {
		print_string(fmap_ro_fwid);
	}
	print_string("\nactive firmware id: ");
	if (acpi_table->main_fw == BINF_RW_A) {
		if (fmap_rwa_fwid)
			print_string(fmap_rwa_fwid);
		else
			print_string("RW A: ID NOT FOUND");
	} else if (acpi_table->main_fw == BINF_RW_B) {
		if (fmap_rwb_fwid)
			print_string(fmap_rwb_fwid);
		else
			print_string("RW B: ID NOT FOUND");
	} else if (acpi_table->main_fw == BINF_RECOVERY) {
		if (fmap_ro_fwid)
			print_string(fmap_ro_fwid);
		else
			print_string("RO: ID NOT FOUND");
	} else {
		print_string("NOT FOUND");
	}
	print_string("\n");
	return VBERROR_SUCCESS;
}
