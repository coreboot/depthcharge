/*
 * Copyright (C) 2015 Google Inc.
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

/*
 * Analog Devices SSM4567 Audio Amplifier Driver
 * www.analog.com/media/en/technical-documentation/data-sheets/SSM4567.pdf
 */

#include <libpayload.h>
#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/ssm4567.h"

static int ssm4567_read(ssm4567Codec *codec, uint8_t reg, uint8_t *data)
{
	return i2c_readb(codec->i2c, codec->chip, reg, data);
}

static int ssm4567_write(ssm4567Codec *codec, uint8_t reg, uint8_t data)
{
	return i2c_writeb(codec->i2c, codec->chip, reg, data);
}

static int ssm4567_update(ssm4567Codec *codec, uint8_t reg,
			  uint8_t mask, uint8_t value)
{
	uint8_t old, new_value;

	if (codec->read(codec, reg, &old)) {
		printf("%s: Error reading reg 0x%02x!\n", __func__, reg);
		return 1;
	}

	new_value = (old & ~mask) | (value & mask);

	if (old != new_value && codec->write(codec, reg, new_value)) {
		printf("%s: Error writing reg 0x%02x to 0x%02x\n",
		       __func__, reg, new_value);
		return 1;
	}

	return 0;
}

static int ssm4567_set_mode(ssm4567Codec *codec)
{
	uint8_t sai1_mask, sai1_value;

	switch (codec->mode) {
	case SSM4567_MODE_PDM:
		/* Disable PDM Pattern Control */
		codec->update(codec, SSM4567_PDM_CTRL,
			      SSM4567_PDM_CTRL_PAT_CTRL_EN, 0);
		sai1_mask = SSM4567_SAI_CTRL_1_PDM_MODE;
		sai1_value = SSM4567_SAI_CTRL_1_PDM_MODE_PDM;
		break;
	case SSM4567_MODE_TDM_PCM:
		sai1_mask = SSM4567_SAI_CTRL_1_PDM_MODE |
			SSM4567_SAI_CTRL_1_SAI_MODE;
		sai1_value = SSM4567_SAI_CTRL_1_PDM_MODE_SERIAL |
			SSM4567_SAI_CTRL_1_SAI_MODE_TDM_PCM;
		break;
	case SSM4567_MODE_I2S:
		sai1_mask = SSM4567_SAI_CTRL_1_PDM_MODE |
			SSM4567_SAI_CTRL_1_SAI_MODE |
			SSM4567_SAI_CTRL_1_SDATA_FMT;
		sai1_value = SSM4567_SAI_CTRL_1_PDM_MODE_SERIAL |
			SSM4567_SAI_CTRL_1_SAI_MODE_TDM_PCM |
			SSM4567_SAI_CTRL_1_SDATA_FMT_I2S;
		break;
	case SSM4567_MODE_I2S_LEFT:
		sai1_mask = SSM4567_SAI_CTRL_1_PDM_MODE |
			SSM4567_SAI_CTRL_1_SAI_MODE |
			SSM4567_SAI_CTRL_1_SDATA_FMT;
		sai1_value = SSM4567_SAI_CTRL_1_PDM_MODE_SERIAL |
			SSM4567_SAI_CTRL_1_SAI_MODE_TDM_PCM |
			SSM4567_SAI_CTRL_1_SDATA_FMT_LEFT;
		break;
	default:
		printf("%s: Illegal mode %d.\n", __func__, codec->mode);
		return 1;
	}

	/* Setup Serial Audio Interface */
	if (codec->update(codec, SSM4567_SAI_CTRL_1, sai1_mask, sai1_value)) {
		printf("%s: Error setting Serial Audio Interface\n", __func__);
		return 1;
	}

	return 0;
}

/* Reset the audio codec */
static int ssm4567_reset(ssm4567Codec *codec)
{
	if (codec->write(codec, SSM4567_SOFT_RESET, SSM4567_SOFT_RESET_VALUE)) {
		printf("%s: Error resetting codec!\n", __func__);
		return 1;
	}

	mdelay(20);
	return 0;
}

/* Power up codec */
static int ssm4567_powerup(ssm4567Codec *codec)
{
	if (codec->update(codec, SSM4567_POWER_CTRL,
			  SSM4567_POWER_CTRL_SPWDN, 0)) {
		printf("%s: Error powering up codec\n", __func__);
		return 1;
	}
	return 0;
}

/* Initialize codec device */
static int ssm4567_enable(SoundRouteComponentOps *me)
{
	ssm4567Codec *codec = container_of(me, ssm4567Codec, component.ops);
	int ret;

	/* Reset the codec */
	ret = ssm4567_reset(codec);

	/* Power up codec */
	ret |= ssm4567_powerup(codec);

	/* Put codec into requested mode */
	ret |= ssm4567_set_mode(codec);

	return ret;
}

ssm4567Codec *new_ssm4567_codec(I2cOps *i2c, uint8_t chip,
				ssm4567CodecMode mode)
{
	ssm4567Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &ssm4567_enable;

	codec->i2c = i2c;
	codec->chip = chip;
	codec->mode = mode;

	codec->read = &ssm4567_read;
	codec->write = &ssm4567_write;
	codec->update = &ssm4567_update;

	return codec;
}
