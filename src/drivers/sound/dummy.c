/*
 * Copyright 2016 Google Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>

#include "drivers/sound/dummy.h"

static int dummy_start(SoundOps *me, uint32_t frequency)
{
	if (!frequency)
		return -1;

	return 0;
}

static int dummy_stop(SoundOps *me)
{
	return 0;
}

static int dummy_play(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	int res = sound_start(frequency);
	mdelay(msec);
	res |= sound_stop();
	return res;
}

SoundOps *new_dummy_sound(void)
{
	SoundOps *ops = xzalloc(sizeof(*ops));
	ops->start = dummy_start;
	ops->stop = dummy_stop;
	ops->play = dummy_play;
	return ops;
}
