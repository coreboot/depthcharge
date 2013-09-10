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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include "base/init_funcs.h"
#include "drivers/bus/usb/usb.h"
#include "drivers/input/input.h"

static void usb_input_init(void)
{
	dc_usb_initialize();
	usb_poll();
}

static int dc_usb_install_on_demand_input(void)
{
	static OnDemandInput dev =
	{
		&usb_input_init,
		1
	};

	list_insert_after(&dev.list_node, &on_demand_input_devices);
	return 0;
}

INIT_FUNC(dc_usb_install_on_demand_input);
