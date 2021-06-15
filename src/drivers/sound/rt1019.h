/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * rt1019.h -- RealTek ALC1019 ALSA SoC Audio driver
 *
 * Copyright 2021 Realtek Semiconductor Corp.
 */

#ifndef __DRIVERS_SOUND_RT1019_H__
#define __DRIVERS_SOUND_RT1019_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/route.h"
#include "drivers/sound/sound.h"

#define AUD_RT1019_DEVICE_ADDR_L	0x28
#define AUD_RT1019_DEVICE_ADDR_R	0x29
#define AUD_RT1019_DEVICE_ADDR_L1	0x2A
#define AUD_RT1019_DEVICE_ADDR_R1	0x2B

typedef struct {
	SoundRouteComponent component;
	SoundOps ops;
	I2cOps *i2c;
	uint8_t chip;
} rt1019Codec;

/*
 * Create a new RT1019 codec
 * @i2c:		I2C Bus operations for the concerned I2C bus.
 * @i2c_dev_addr:	I2C Address at which the amplifier is present.
 */
rt1019Codec *new_rt1019_codec(I2cOps *i2c, uint8_t i2c_dev_addr);

#endif /* __DRIVERS_SOUND_RT1019_H__ */
