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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <vboot_api.h>
#include <vboot/util/flag.h>

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

VbError_t VbExDisplayScreen(uint32_t screen_type)
{
	return display_screen((enum VbScreenType_t)screen_type);
}

VbError_t VbExDisplayImage(uint32_t x, uint32_t y,
			   void *buffer, uint32_t buffersize)
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

/*
 * This is a shortcut to get the storm design going, a different solution
 * might be required, tracked under crosbug.com/p/30705 and
 * crosbug.com/p/31325.
 */
uint32_t VbExKeyboardRead(void)
{
	uint32_t rv = flag_fetch(FLAG_PHYS_PRESENCE);

	if (rv) {
		printf("%s:%d phy presense asserted\n", __func__, __LINE__);
		return 0x15; /* That's ^U, i.e. boot from USB. */
	}
	return 0;
}

uint32_t VbExKeyboardReadWithFlags(uint32_t *flags_ptr)
{
	uint32_t c = VbExKeyboardRead();

	return c;
}
