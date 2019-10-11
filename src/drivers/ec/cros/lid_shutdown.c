/*
 * Copyright (C) 2016 Google Inc.
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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>
#include <vb2_api.h>

#include "base/init_funcs.h"
#include "drivers/ec/cros/ec.h"
#include "vboot/util/commonparams.h"
#include "vboot/util/init_funcs.h"

int cros_ec_get_lid_shutdown_mask(void)
{
	uint32_t mask;

	if (cros_ec_get_event_mask(EC_CMD_HOST_EVENT_GET_SMI_MASK, &mask) < 0)
		return -1;

	return !!(mask & EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_CLOSED));
}

int cros_ec_set_lid_shutdown_mask(int enable)
{
	uint32_t mask;

	if (cros_ec_get_event_mask(EC_CMD_HOST_EVENT_GET_SMI_MASK, &mask) < 0)
		return -1;

	// Set lid close event state in the EC SMI event mask
	if (enable)
		mask |= EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_CLOSED);
	else
		mask &= ~EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_CLOSED);

	if (cros_ec_set_event_mask(EC_CMD_HOST_EVENT_SET_SMI_MASK, mask) < 0)
		return -1;

	printf("EC: %sabled lid close event\n", enable ? "en" : "dis");
	return 0;
}

static int cros_ec_disable_lid_shutdown_at_startup(VbootInitFunc *init)
{
	if (!(vb2api_gbb_get_flags(vboot_get_context())
	    & VB2_GBB_FLAG_DISABLE_LID_SHUTDOWN))
		return 0;

	printf("EC: Disabling lid close event due to GBB override\n");

	return cros_ec_set_lid_shutdown_mask(0);
}

/* Needs to run at vboot time to ensure board_init() has set up EC driver. */
VBOOT_INIT_FUNC(cros_ec_disable_lid_shutdown_at_startup);
