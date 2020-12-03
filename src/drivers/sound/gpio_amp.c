/*
 * gpio_amp.c -- Audio driver for GPIO based amplifier
 *
 * Copyright (C) 2015 Google Inc.
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

static int max98357a_enable(SoundRouteComponentOps *me)
{
	GpioAmpCodec *codec = container_of(me, GpioAmpCodec, component.ops);

	return gpio_set(codec->sdmode_gpio, 1);
}

static int max98357a_disable(SoundRouteComponentOps *me)
{
	GpioAmpCodec *codec = container_of(me, GpioAmpCodec, component.ops);

	return gpio_set(codec->sdmode_gpio, 0);
}

GpioAmpCodec *new_gpio_amp_codec(GpioOps *ops)
{
	GpioAmpCodec *codec = xzalloc(sizeof(*codec));

	codec->sdmode_gpio = ops;
	codec->component.ops.enable = &max98357a_enable;
	codec->component.ops.disable = &max98357a_disable;

	return codec;
}
