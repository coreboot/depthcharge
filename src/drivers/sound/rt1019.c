// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * rt1019.c -- RealTek ALC1019 ALSA SoC Audio driver
 *
 * Copyright 2021 Realtek Semiconductor Corp.
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "base/list.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/rt1019.h"

/* rt1019 registers address */
#define RT1019_RESET			0x0000
#define RT1019_ASEL_CTRL		0x0013
#define   RT1019_BEEP_TONE_HIGH		0x08
#define   RT1019_BEEP_TONE_LOW		0x0
#define RT1019_BEEP_TONE		0x001b
#define   RT1019_BEEP_TONE_SEL		0x01
#define RT1019_BEEP_1			0x0b00
#define RT1019_BEEP_2			0x0b01

struct reg_config {
	uint16_t addr;
	uint8_t data;
};

static const struct reg_config init_config[] = {
	{RT1019_BEEP_1,		0x2d},
	{RT1019_BEEP_2,		0x6a},
	{RT1019_BEEP_TONE,	RT1019_BEEP_TONE_SEL},
};

static int rt1019_i2c_writeb(rt1019Codec *codec, uint16_t reg, uint8_t data)
{
	if (i2c_addrw_writeb(codec->i2c, codec->chip, reg, data)) {
		printf("rt1019 i2c reg:%x write err\n", reg);
		return 1;
	}
	return 0;
}

/* Resets the audio codec */
static int rt1019_reset(rt1019Codec *codec)
{
	/* Reset the codec registers to their defaults */
	if (rt1019_i2c_writeb(codec, RT1019_RESET, 0x00)) {
		printf("%s: Error resetting codec!\n", __func__);
		return 1;
	}
	return 0;
}

/* Initialize rt1019 codec device */
static int rt1019_device_init(rt1019Codec *codec)
{
	size_t i;

	/* codec reset */
	if (rt1019_reset(codec))
		return 1;

	/* Regs config for internal tone generation */
	for (i = 0; i < ARRAY_SIZE(init_config); i++) {
		if (rt1019_i2c_writeb(codec, init_config[i].addr,
						init_config[i].data))
			return 1;
	}

	return 0;
}

static int rt1019_play_boot_beep(SoundOps *me, uint32_t msec,
				uint32_t frequency)
{
	uint8_t reg_data = RT1019_BEEP_TONE_HIGH, pulse_cnt = 2, i;

	/* ignore frequency param since rt1019 generates internal tone */
	/* rt1019 does not require square wave generation with frequency */
	rt1019Codec *codec = container_of(me, rt1019Codec, ops);
	/* Internal Tone generation */
	for (i = 0; i < pulse_cnt; i++) {
		if (rt1019_i2c_writeb(codec, RT1019_ASEL_CTRL, reg_data))
			return 1;

		/* Alter data between High and Low */
		if (reg_data == RT1019_BEEP_TONE_HIGH)
			reg_data = RT1019_BEEP_TONE_LOW;
		else
			reg_data = RT1019_BEEP_TONE_HIGH;

		mdelay(msec);
	}
	return 0;
}

static int rt1019_enable(SoundRouteComponentOps *me)
{
	rt1019Codec *codec = container_of(me, rt1019Codec, component.ops);

	return rt1019_device_init(codec);
}

static int rt1019_disable(SoundRouteComponentOps *me)
{
	rt1019Codec *codec = container_of(me, rt1019Codec, component.ops);

	return rt1019_reset(codec);
}

rt1019Codec *new_rt1019_codec(I2cOps *i2c, uint8_t chip)
{
	rt1019Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &rt1019_enable;
	codec->component.ops.disable = &rt1019_disable;

	codec->ops.play = &rt1019_play_boot_beep;
	codec->i2c = i2c;
	codec->chip = chip;

	return codec;
}
