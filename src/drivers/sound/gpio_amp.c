/*
 * gpio_amp.c -- Audio driver for GPIO based amplifier
 *
 * Copyright 2015 Google LLC
 * Copyright (C) 2015 Intel Corporation.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.See the
 * GNU General Public License for more details.
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "gpio_amp.h"

static int gpio_amp_enable(SoundRouteComponentOps *me)
{
	GpioAmpCodec *codec = container_of(me, GpioAmpCodec, component.ops);
	int ret;

	ret = gpio_set(codec->enable_speaker, 1);

	if (ret)
		return ret;

	udelay(codec->enable_delay_us);

	return 0;
}

static int gpio_amp_disable(SoundRouteComponentOps *me)
{
	GpioAmpCodec *codec = container_of(me, GpioAmpCodec, component.ops);

	return gpio_set(codec->enable_speaker, 0);
}

GpioAmpCodec *new_gpio_amp_codec_with_delay(GpioOps *ops,
					    unsigned int enable_delay_us)
{
	GpioAmpCodec *codec = xzalloc(sizeof(*codec));

	codec->enable_speaker = ops;
	codec->enable_delay_us = enable_delay_us;
	codec->component.ops.enable = &gpio_amp_enable;
	codec->component.ops.disable = &gpio_amp_disable;

	return codec;
}

GpioAmpCodec *new_gpio_amp_codec(GpioOps *ops)
{
	return new_gpio_amp_codec_with_delay(ops, 0);
}
