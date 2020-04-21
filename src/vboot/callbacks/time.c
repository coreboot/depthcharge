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
 */

#include <libpayload.h>
#include <vb2_api.h>

#include "drivers/sound/sound.h"

uint32_t vb2ex_mtime(void)
{
	return timer_us(0) / USECS_PER_MSEC;
}

void vb2ex_msleep(uint32_t msec)
{
	mdelay(msec);
}

void vb2ex_beep(uint32_t msec, uint32_t frequency)
{
	uint64_t start;
	uint64_t elapsed;
	int ret;
	int need_stop = 1;

	/* Zero-length beep should return immediately. */
	if (msec == 0)
		return;

	start = timer_us(0);
	ret = sound_start(frequency);

	if (ret < 0) {
		/* Driver only supports blocking beep calls. */
		need_stop = 0;
		ret = sound_play(msec, frequency);
		if (ret)
			printf("WARNING: sound_play() returned %#x\n", ret);
	}

	/* Wait for requested time on a non-blocking call, and
	   enforce minimum delay in case of buggy sound drivers. */
	elapsed = timer_us(start);
	if (elapsed < msec * USECS_PER_MSEC)
		mdelay(msec - elapsed / USECS_PER_MSEC);

	if (need_stop)
		sound_stop();
}
