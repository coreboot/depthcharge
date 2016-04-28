/*
 * Copyright 2016 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <assert.h>
#include <libpayload.h>

#include "drivers/ec/vboot_ec.h"

VbootEcOps *vboot_ec[NUM_MAX_VBOOT_ECS] = { NULL };

void register_vboot_ec(VbootEcOps *ec, int devidx)
{
	assert(devidx >= 0 && devidx < NUM_MAX_VBOOT_ECS);
	assert(!vboot_ec[devidx]);

	vboot_ec[devidx] = ec;
}

void reboot_all_ecs(void)
{
	int devidx;

	printf("Preparing all ECs to reboot to RO...\n");
	for (devidx = 0; devidx < NUM_MAX_VBOOT_ECS; devidx++) {
		VbootEcOps *ec = vboot_ec[devidx];
		if (!ec)
			continue;
		if (ec->reboot_to_ro(ec) != VBERROR_SUCCESS)
			printf("...EC[%d] failed, ignoring\n", devidx);
	}
	printf("...done!\n");
}
