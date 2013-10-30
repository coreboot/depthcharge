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

#include "base/time.h"
#include "drivers/timer/timer.h"

uint64_t timer_us(uint64_t base)
{
	static uint64_t hz;

	// Only check timer_hz once. Assume it doesn't change.
	if (hz == 0) {
		hz = timer_hz();
		if (hz < 1000000) {
			printf("Timer frequency %lld is too low, "
			       "must be at least 1MHz.\n", hz);
			halt();
		}
	}

	return timer_raw_value() / (hz / 1000000) - base;
}
