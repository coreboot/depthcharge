/*
 * Copyright 2015 Google Inc.
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

#include "drivers/ec/cros/commands.h"
#include "drivers/ec/cros/ec.h"
#include "fastboot/ec.h"

int ec_fb_keyboard_mask(void)
{
	uint32_t ec_events;
	const uint32_t kb_fastboot_mask =
		EC_HOST_EVENT_MASK(EC_HOST_EVENT_KEYBOARD_FASTBOOT);

	cros_ec_get_host_events(&ec_events);

	if (kb_fastboot_mask & ec_events) {
		cros_ec_clear_host_events(kb_fastboot_mask);
		return 1;
	}

	return 0;
}

int ec_fb_battery_cutoff(void)
{
	/* Cut-off immediately. The system will keep running by USB power. */
	return cros_ec_battery_cutoff(0);
}
