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

#include <libpayload-config.h>
#include <libpayload.h>

#include <vboot_api.h>

VbError_t VbExTpmInit(void)
{
	printf("VbExTpmInit called but not implemented.\n");
	return VBERROR_SUCCESS;
}

VbError_t VbExTpmClose(void)
{
	printf("VbExTpmClose called but not implemented.\n");
	return VBERROR_SUCCESS;
}

VbError_t VbExTpmOpen(void)
{
	printf("VbExTpmOpen called but not implemented.\n");
	return VBERROR_SUCCESS;
}

VbError_t VbExTpmSendReceive(const uint8_t *request, uint32_t request_length,
			     uint8_t *response, uint32_t *response_length)
{
	printf("VbExTpmSendReceive called but not implemented.\n");
	return VBERROR_SUCCESS;
}
