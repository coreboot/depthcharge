// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2020 Google LLC
 *
 * Vboot EC callbacks that are always required.
 */

#include <assert.h>
#include <vb2_api.h>

#include "drivers/ec/vboot_ec.h"

vb2_error_t vb2ex_ec_battery_cutoff(void)
{
	VbootEcOps *ec = vboot_get_ec();

	assert(ec && ec->battery_cutoff);
	return (ec->battery_cutoff(ec) == 0
		? VB2_SUCCESS : VB2_ERROR_UNKNOWN);
}
