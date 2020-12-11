/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * rt1015p.h - driver for RT1015Q audio amplifier in auto mode
 *
 * Copyright 2020 Google Inc.
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

#ifndef __DRIVERS_SOUND_RT1015P_H__
#define __DRIVERS_SOUND_RT1015P_H__

#include "drivers/gpio/gpio.h"
#include "drivers/sound/route.h"

typedef struct
{
	SoundRouteComponent component;
	GpioOps *sdb;

	u64 early_init_time_us;
	int early_init;
	int calibrated;

} rt1015pCodec;

rt1015pCodec *new_rt1015p_codec(GpioOps *ops);

/*
 * Early init (pre-calibrate) RT1015P to prevent extra delay in first play.
 *
 * Call this after you have setup all components in route (especially I2S source
 * and RT1015P itself).
 */
void rt1015p_early_init(SoundRouteComponentOps *me, SoundRoute *route);

#endif /* __DRIVERS_SOUND_RT1015P_H__ */
