/*
 * Copyright 2014 Google Inc.
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
#include <vboot_api.h>
#include <vb2_api.h>

#include "drivers/video/display.h"

vb2_error_t VbExDisplayScreen(uint32_t screen_type, uint32_t locale,
			      const VbScreenData *data)
{
	return display_screen((enum VbScreenType_t)screen_type);
}

vb2_error_t VbExDisplayMenu(uint32_t screen_type, uint32_t locale,
			    uint32_t selected_index, uint32_t disabled_idx_mask,
			    uint32_t redraw_base)
{
	printf("%s:%d invoked\n", __func__, __LINE__);
	return VB2_SUCCESS;
}

vb2_error_t VbExDisplayDebugInfo(const char *info_str, int full_info)
{
	printf("%s:%d invoked\n", __func__, __LINE__);
	return VB2_SUCCESS;
}

int video_console_init(void)
{
	printf("%s:%d invoked\n", __func__, __LINE__);
	return 0;
}
