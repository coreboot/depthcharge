/*
 * Copyright 2012 Google Inc.
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

#include "base/init_funcs.h"
#include "base/cleanup_funcs.h"
#include "drivers/input/input.h"

static int dc_keyboard_install_on_demand_input(void)
{
	static OnDemandInput dev =
	{
		&keyboard_init,
		1
	};

	list_insert_after(&dev.list_node, &on_demand_input_devices);
	return 0;
}

INIT_FUNC(dc_keyboard_install_on_demand_input);

static int disconnect_keyboard_wrapper(struct CleanupFunc *cleanup,
				       CleanupType type)
{
	keyboard_disconnect();
	return 0;
}

static int dc_keyboard_install_cleanup(void)
{
	static CleanupFunc dev =
	{
		&disconnect_keyboard_wrapper,
		CleanupOnHandoff | CleanupOnLegacy,
		NULL
	};

	list_insert_after(&dev.list_node, &cleanup_funcs);
	return 0;
}

INIT_FUNC(dc_keyboard_install_cleanup);
