/*
 * Copyright 2013 Google Inc.
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

#include "drivers/ec/cros/ec.h"
#include "vboot/util/ec.h"

void reboot_ec_to_ro()
{
	if (cros_ec_reboot(0, EC_REBOOT_COLD, EC_REBOOT_FLAG_ON_AP_SHUTDOWN)) {
		printf("Failed to reboot EC to RO.\n");
		halt();
	}

	// TODO(rspangler@chromium.org): need to reboot PD chip too, if present
}
