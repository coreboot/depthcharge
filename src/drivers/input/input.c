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
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 *
 */

#include <assert.h>
#include <libpayload.h>

#include "drivers/input/input.h"

ListNode on_demand_input_devices;

static void do_input_init(void)
{
	OnDemandInput *dev;
	list_for_each(dev, on_demand_input_devices, list_node) {
		if (dev->need_init) {
			assert(dev->init);
			dev->init();
			dev->need_init = 0;
		}
	}
}

static int fake_havekey(void)
{
	do_input_init();
	return 0;
}

static int fake_getchar(void)
{
	do_input_init();
	return 0;
}

static struct console_input_driver on_demand_input_driver =
{
	NULL,
	&fake_havekey,
	&fake_getchar
};

void input_enable(void)
{
	do_input_init();
}

void input_init(void)
{
	console_add_input_driver(&on_demand_input_driver);
}
