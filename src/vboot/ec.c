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

int VbExTrustEC(void)
{
	printf("VbExTrustEC not implemented.\n");
	return 1;
}

VbError_t VbExEcRunningRW(int *in_rw)
{
	printf("VbExEcRunningRW not implemented.\n");
	return VBERROR_UNKNOWN;
}

VbError_t VbExEcJumpToRW(void)
{
	printf("VbExEcJumpToRw not implemented.\n");
	return VBERROR_UNKNOWN;
}

VbError_t VbExEcStayInRO(void)
{
	printf("VbExEcStayInRO not implemented.\n");
	return VBERROR_UNKNOWN;
}

VbError_t VbExEcHashRW(const uint8_t **hash, int *hash_size)
{
	printf("VbExEcHashRW not implemented.\n");
	return VBERROR_UNKNOWN;
}

VbError_t VbExEcGetExpectedRW(enum VbSelectFirmware_t select,
			      const uint8_t **image, int *image_size)
{
	printf("VbExEcGetExpectedRW not implemented.\n");
	return VBERROR_UNKNOWN;
}

VbError_t VbExEcUpdateRW(const uint8_t *image, int image_size)
{
	printf("VbExEcUpdateRW not implemented.\n");
	return VBERROR_UNKNOWN;
}

VbError_t VbExEcProtectRW(void)
{
	printf("VbExEcProtectRW not implemented.\n");
	return VBERROR_UNKNOWN;
}
