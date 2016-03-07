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

#include <gbb_header.h>
#include <libpayload.h>

#include "base/init_funcs.h"
#include "drivers/ec/cros/ec.h"
#include "vboot/util/commonparams.h"

static int cros_ec_disable_lid_shutdown(void)
{
	uint32_t smi_event_mask;

	if (!(gbb_get_flags() & GBB_FLAG_DISABLE_LID_SHUTDOWN))
		return 0;

	if (cros_ec_get_event_mask(EC_CMD_HOST_EVENT_GET_SMI_MASK,
				   &smi_event_mask) < 0)
		return -1;

	// Disable lid close event in the EC SMI event mask
	smi_event_mask &= ~EC_HOST_EVENT_MASK(EC_HOST_EVENT_LID_CLOSED);

	if (cros_ec_set_event_mask(EC_CMD_HOST_EVENT_SET_SMI_MASK,
				   smi_event_mask) < 0)
		return -1;

	printf("EC: Disabled lid close event due to GBB override\n");
	return 0;
}

INIT_FUNC(cros_ec_disable_lid_shutdown);
