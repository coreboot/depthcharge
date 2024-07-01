/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright 2024 Texas Instruments
 */

#ifndef __DRIVERS_SOUND_TAS2563_H__
#define __DRIVERS_SOUND_TAS2563_H__

#include <inttypes.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/route.h"

enum pcm_channel {
	PCM_LEFT_ONLY = 0x00,
	PCM_RIGHT_ONLY = 0x01,
};

enum pcm_bitwidth {
	PCM_FMTBIT_S16_LE = 0x00,
	PCM_FMTBIT_S24_LE = 0x01,
	PCM_FMTBIT_S32_LE = 0x02,
};

typedef struct Tas2563Codec {
	SoundRouteComponent component;
	I2cOps *i2c;
	uint8_t chip;
	enum pcm_channel channel;
	enum pcm_bitwidth bitwidth;
	uint8_t cur_book;
	uint8_t cur_page;
} Tas2563Codec;

Tas2563Codec *new_tas2563_codec(I2cOps *i2c, uint8_t chip,
				enum pcm_channel channel,
				enum pcm_bitwidth bitwidth);

#endif /* __DRIVERS_SOUND_TAS2563_H__ */
