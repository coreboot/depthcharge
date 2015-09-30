/*
 * max98357a.c -- MAX98357A Audio driver
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc.
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "max98357a.h"

static int max98357a_enable(SoundRouteComponentOps *me)
{
	max98357aCodec *codec = container_of(me, max98357aCodec, component.ops);

	return gpio_set(codec->sdmode_gpio, 1);
}

max98357aCodec *new_max98357a_codec(GpioCfg *sdmode)
{
	max98357aCodec *codec = xzalloc(sizeof(*codec));

	codec->sdmode_gpio = &sdmode->ops;
	codec->component.ops.enable = &max98357a_enable;

	return codec;
}
