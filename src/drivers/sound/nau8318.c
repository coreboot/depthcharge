/* SPDX-License-Identifier: GPL-2.0 */
/*
 * nau8318.c - driver for NAU8318 beep function
 *
 * Copyright(c) 2023 Intel Corporation.
 * Copyright(c) 2023 Nuvoton Corporation.
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

#include "nau8318.h"

static int nau8318_start(SoundOps *me, uint32_t frequency)
{
	nau8318Codec *codec = container_of(me, nau8318Codec, ops);

	gpio_set(codec->enable, 1);
	gpio_set(codec->beep, 1);

	return 0;
}

static int nau8318_stop(SoundOps *me)
{
	nau8318Codec *codec = container_of(me, nau8318Codec, ops);

	gpio_set(codec->beep, 0);
	gpio_set(codec->enable, 0);

	return 0;
}

nau8318Codec *new_nau8318_codec(GpioOps *enable, GpioOps *beep)
{
	nau8318Codec *codec = xzalloc(sizeof(*codec));

	codec->enable = enable;
	codec->beep = beep;

	codec->ops.start = &nau8318_start;
	codec->ops.stop = &nau8318_stop;

	return codec;
}
