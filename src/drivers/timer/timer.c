/*
 * Copyright (C) 2017 Intel Corporation.
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
 */

#include <libpayload.h>

#include "drivers/timer/timer.h"

void stopwatch_init_usecs_expire(struct stopwatch *sw, unsigned long us)
{
	sw->start = timer_us(0);
	sw->expires = us;
}

void stopwatch_init_msecs_expire(struct stopwatch *sw, unsigned long ms)
{
	stopwatch_init_usecs_expire(sw, USECS_PER_MSEC * ms);
}

int stopwatch_expired(struct stopwatch *sw)
{
	return timer_us(sw->start) > sw->expires;
}
