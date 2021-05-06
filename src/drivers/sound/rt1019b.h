/* SPDX-License-Identifier: GPL-2.0 */
/*
 * rt1019b.h - driver for RT1019 beep function in auto mode
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

#ifndef __DRIVERS_SOUND_RT1019B_H__
#define __DRIVERS_SOUND_RT1019B_H__

#include "drivers/gpio/gpio.h"
#include "drivers/sound/sound.h"

typedef struct
{
	SoundOps ops;
	GpioOps *sdb;
	GpioOps *beep;
} rt1019bCodec;

rt1019bCodec *new_rt1019b_codec(GpioOps *sdb, GpioOps *beep);

#endif /* __DRIVERS_SOUND_RT1019B_H__ */
