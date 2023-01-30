/* SPDX-License-Identifier: GPL-2.0 */
/*
 * nau8318.h - driver for NAU8318 beep function
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

#ifndef __DRIVERS_SOUND_NAU8318_H__
#define __DRIVERS_SOUND_NAU8318_H__

#include "drivers/gpio/gpio.h"
#include "drivers/sound/sound.h"

typedef struct
{
	SoundOps ops;
	GpioOps *enable;
	GpioOps *beep;
} nau8318Codec;

nau8318Codec *new_nau8318_codec(GpioOps *enable, GpioOps *beep);

#endif /* __DRIVERS_SOUND_NAU8318_H__ */
