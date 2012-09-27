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

VbError_t VbExDisplayScreen(uint32_t screen_type)
{
	printf("VbExDisplayScreen called but not implemented.\n");
	return VBERROR_SUCCESS;
}

VbError_t VbExDisplayImage(uint32_t x, uint32_t y,
			   void *buffer, uint32_t buffersize)
{
	printf("VbExDisplayImage called but not implemented.\n");
	return VBERROR_SUCCESS;
}

VbError_t VbExDisplayDebugInfo(const char *info_str)
{
	printf("VbExDisplayDebugInfo called but not implemented.\n");
	return VBERROR_SUCCESS;
}
