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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <assert.h>
#include <libpayload.h>
#include <sysinfo.h>
#include <vboot_api.h>
#include <vboot_struct.h>

#include "base/cleanup_funcs.h"
#include "drivers/video/coreboot_fb.h"
#include "drivers/video/display.h"
#include "vboot/firmware_id.h"
#include "vboot/util/commonparams.h"
#include "vboot/stages.h"

VbError_t VbExDisplayInit(uint32_t *width, uint32_t *height)
{
	if (lib_sysinfo.framebuffer) {
		*width = lib_sysinfo.framebuffer->x_resolution;
		*height = lib_sysinfo.framebuffer->y_resolution;
	} else {
		*width = *height = 0;
	}
	return VBERROR_SUCCESS;
}

VbError_t VbExDisplayBacklight(uint8_t enable)
{
	if (backlight_update(enable))
		return VBERROR_UNKNOWN;

	return VBERROR_SUCCESS;
}

VbError_t VbExDisplayScreen(uint32_t screen_type)
{
	return vboot_draw_screen(screen_type, 0, 1);
}

VbError_t VbExDisplayImage(uint32_t x, uint32_t y,
			   void *buffer, uint32_t buffersize)
{
	if (dc_corebootfb_draw_bitmap(x, y, buffer))
		return VBERROR_UNKNOWN;

	return VBERROR_SUCCESS;
}

VbError_t VbExDisplaySetDimension(uint32_t width, uint32_t height)
{
	// TODO(hungte) Shift or scale images if width/height is not equal to
	// lib_sysinfo.framebuffer->{x,y}_resolution.
	return VBERROR_SUCCESS;
}

VbError_t VbExDisplayDebugInfo(const char *info_str)
{
	video_console_set_cursor(0, 0);
	video_printf(15, 0, 0, info_str);

	video_printf(15, 0, 0, "read-only firmware id: %s\n", get_ro_fw_id());

	const char *id = get_active_fw_id();
	if (id == NULL)
		id = "NOT FOUND";
	video_printf(15, 0, 0, "active firmware id: %s\n", id);

	return VBERROR_SUCCESS;
}
