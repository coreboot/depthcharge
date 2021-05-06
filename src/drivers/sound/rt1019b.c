/* SPDX-License-Identifier: GPL-2.0 */
/*
 * rt1019b.c - driver for RT1019 beep function in auto mode
 *
 * Copyright 2021 Mediatek Inc.
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

#include "base/container_of.h"
#include "rt1019b.h"

static int rt1019b_start(SoundOps *me, uint32_t frequency)
{
	rt1019bCodec *codec = container_of(me, rt1019bCodec, ops);

	gpio_set(codec->sdb, 1);

	/* delay 2ms for rt1019 internal circuit */
	mdelay(2);

	gpio_set(codec->beep, 1);

	return 0;
}

static int rt1019b_stop(SoundOps *me)
{
	rt1019bCodec *codec = container_of(me, rt1019bCodec, ops);

	gpio_set(codec->beep, 0);
	gpio_set(codec->sdb, 0);

	return 0;
}

static int rt1019b_play(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	sound_start(frequency);
	mdelay(msec);
	sound_stop();

	return 0;
}

rt1019bCodec *new_rt1019b_codec(GpioOps *sdb, GpioOps *beep)
{
	rt1019bCodec *codec = xzalloc(sizeof(*codec));

	codec->sdb = sdb;
	codec->beep = beep;

	codec->ops.start = &rt1019b_start;
	codec->ops.stop = &rt1019b_stop;
	codec->ops.play = &rt1019b_play;

	return codec;
}
