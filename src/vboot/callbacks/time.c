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
#include <vboot_api.h>

#include "drivers/sound/sound.h"

uint64_t VbExGetTimer(void)
{
	static uint64_t start = 0;
	if (!start)
		start = timer_us(0);
	return timer_us(start);
}

void VbExSleepMs(uint32_t msec)
{
	mdelay(msec);
}

VbError_t VbExBeep(uint32_t msec, uint32_t frequency)
{
	int res;
        if (frequency) {
                res = sound_start(frequency);
        } else {
                res = sound_stop();
	}

	if (res > 0) {
		// The previous call had an error.
		return VBERROR_UNKNOWN;
	} else if (res < 0) {
		// Non-blocking beeps aren't supported.
		if (msec > 0 && sound_play(msec, frequency))
			return VBERROR_UNKNOWN;
		return VBERROR_NO_BACKGROUND_SOUND;
	} else {
		// The non-blocking call worked. Delay if requested.
		if (msec > 0) {
			mdelay(msec);
			if (sound_stop())
				return VBERROR_UNKNOWN;
		}
		return VBERROR_SUCCESS;
        }
}
