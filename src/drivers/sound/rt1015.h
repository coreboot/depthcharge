/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * RT1015 ALSA SoC audio amplifier driver
 *
 * Copyright 2020 Intel Corporation.
 */

#ifndef __DRIVERS_SOUND_RT1015_H__
#define __DRIVERS_SOUND_RT1015_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/route.h"

#define AUD_RT1015_DEVICE_ADDR			0x28
#define RT1015_DEVICE_ID_VAL			0x1011
#define RT1015_DEVICE_ID_VAL2			0x1015

#define RT1015_RESET				0x0000
#define RT1015_PLL1				0x000a
#define RT1015_DEVICE_ID			0x007d
#define RT1015_TDM_MASTER			0x0111
#define RT1015_PWR4				0x0328
#define RT1015_CLSD_INTERNAL8			0x130c
#define RT1015_CLSD_INTERNAL9			0x130e
#define RT1015_GAT_BOOST			0x00f3
#define RT1015_PWR_STATE_CTRL			0x0338

typedef struct {
	SoundRouteComponent component;
	SoundOps ops;
	I2cOps *i2c;
	uint8_t chip;
} rt1015Codec;

rt1015Codec *new_rt1015_codec(I2cOps *i2c, uint8_t chip);

#endif /* __DRIVERS_SOUND_RT1015_H__ */
