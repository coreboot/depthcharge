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

#include "drivers/video/display.h"

VbError_t VbExDisplayInit(uint32_t *width, uint32_t *height)
{
	printf("%s:%d invoked\n", __func__, __LINE__);
	return VBERROR_SUCCESS;
}

VbError_t VbExDisplayBacklight(uint8_t enable)
{
	printf("%s:%d invoked\n", __func__, __LINE__);
	return VBERROR_SUCCESS;
}

VbError_t VbExDisplayScreen(uint32_t screen_type, uint32_t locale)
{
	return display_screen((enum VbScreenType_t)screen_type);
}

VbError_t VbExDisplayImage(uint32_t x, uint32_t y,
			   void *buffer, uint32_t buffersize)
{
	printf("%s:%d invoked\n", __func__, __LINE__);
	return VBERROR_SUCCESS;
}

VbError_t VbExDisplayText(uint32_t x, uint32_t y,
			  const char *info_str)
{
	printf("%s:%d invoked\n", __func__, __LINE__);
	return VBERROR_SUCCESS;
}

VbError_t VbExDisplaySetDimension(uint32_t width, uint32_t height)
{
	printf("%s:%d invoked\n", __func__, __LINE__);
	return VBERROR_SUCCESS;
}

VbError_t VbExDisplayDebugInfo(const char *info_str)
{
	printf("%s:%d invoked\n", __func__, __LINE__);
	return VBERROR_SUCCESS;
}

int video_console_init(void)
{
	printf("%s:%d invoked\n", __func__, __LINE__);
	return 0;
}
