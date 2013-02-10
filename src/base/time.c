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

#include "base/time.h"
#include "drivers/timer/timer.h"

static uint64_t base_value;

uint64_t get_timer_value(void)
{
	uint64_t time_now = get_raw_timer_value();
	if (!base_value)
		base_value = time_now;
	return time_now - base_value;
}

void set_base_timer_value(uint64_t new_base)
{
	base_value = new_base;
}
