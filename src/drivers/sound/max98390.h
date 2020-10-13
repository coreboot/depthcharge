/*
 * max98390.h -- Maxim Integrated 98390
 *
 * Copyright 2020 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DRIVERS_SOUND_MAX98390_H__
#define __DRIVERS_SOUND_MAX98390_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/route.h"
#include <inttypes.h>

typedef struct
{
	SoundRouteComponent component;

	I2cOps *i2c;
	uint8_t chip;
} Max98390Codec;

Max98390Codec *new_max98390_codec(I2cOps *i2c, uint8_t chip);

#endif /* __DRIVERS_SOUND_MAX98095_H__ */
