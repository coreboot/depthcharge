/*
 * max98396.c -- Maxim Integrated 98396
 *
 * Copyright 2022 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <libpayload.h>

#include "drivers/bus/i2c/i2c.h"
// reuse max98373 register define
#include "drivers/sound/max98373.h"
#include "drivers/sound/max98396.h"

/* I2S controller settings for Max98396 codec*/
const I2sSettings max98396_settings = {
	/* To set MOD bit in SSC0 - defining as network/normal mode */
	.mode = SSP_IN_NETWORK_MODE,
	/* To set FRDC bit in SSC0 - timeslot per frame in network mode */
	.frame_rate_divider_ctrl = FRAME_RATE_CONTROL_STEREO,
	/* To set EDMYSTOP bit in SSPSP - number of SCLK cycles after data */
	.ssp_psp_T4 = 2,
	/* To set SFRMWDTH bit in SSPSP - frame width */
	.ssp_psp_T6 = 0x18,
	/* To set TTSA bit n SSTSA - data transmit timeslot */
	.ssp_active_tx_slots_map = 3,
	/* To set RTSA bit n SSRSA - data receive timeslot */
	.ssp_active_rx_slots_map = 3,
};

static int max98396_write(Max98396Codec *codec, uint16_t reg, uint8_t data)
{
	I2cSeg seg;
	uint16_t i2c_reg = htobe16(reg);
	uint8_t buffer[] = { i2c_reg & 0xff, i2c_reg >> 8, data };

	seg.read = 0;
	seg.buf = buffer;
	seg.len = ARRAY_SIZE(buffer);
	seg.chip = codec->chip;

	return codec->i2c->transfer(codec->i2c, &seg, 1);
}

static int max98396_read(Max98396Codec *codec, uint16_t reg, uint8_t *data)
{
	I2cSeg seg[2];
	uint16_t i2c_reg = htobe16(reg);

	seg[0].read = 0;
	seg[0].buf = (uint8_t *)&i2c_reg;
	seg[0].len = sizeof(i2c_reg);
	seg[0].chip = codec->chip;

	seg[1].read = 1;
	seg[1].buf = data;
	seg[1].len = sizeof(*data);
	seg[1].chip = codec->chip;

	return codec->i2c->transfer(codec->i2c, seg, ARRAY_SIZE(seg));
}

static int max98396_update(Max98396Codec *codec, uint16_t reg,
			   uint8_t mask, uint8_t value)
{
	uint8_t old;
	if (max98396_read(codec, reg, &old))
		return 1;
	uint8_t new = (old & ~mask) | (value & mask);
	if (old != new && max98396_write(codec, reg, new))
		return 1;
	return 0;
}

static int max98396_hw_params(Max98396Codec *codec)
{
	// Set speaker gain to 0 dB
	if (max98396_write(codec, MAX396_REG_AMP_GAIN, SPEAKER_GAIN_0DB))
		return 1;

	// Set speaker amp to 0 dB
	if (max98396_write(codec, MAX396_REG_AMP_VOLUME, SPEAKER_GAIN_0DB))
		return 1;
	/*
	 * Disable safe mode, if enabled safe mode SPK_VOL and SPK_GAIN_MAX
	 * settings are ignored and the amplifier output is set to -18dBFS.
	 */
	if (max98396_update(codec, MAX396_REG_DSP_CONFIG, SPK_SAFE_EN_MASK,
			    GLOBAL_DISABLE))
		return 1;

	return 0;
}

static int max98396_i2s_pcm_format(Max98396Codec *codec)
{
	// Set PCM FORMAT to I2S
	if (max98396_write(codec, MAX396_REG_PCM_MODE_CONFIG, PCM_INTERFACE))
		return 1;
	// Set SPK Rx path enable
	if (max98396_write(codec, MAX396_REG_DATA_INPUT_EN, DATA_INPUT_EN))
		return 1;
	if (max98396_update(codec, MAX396_REG_TONE_CONFIG, TONE_CONFIG_MASK,
			    TONE_CONFIG_1K))
		return 1;
	if (max98396_write(codec, MAX396_REG_TONE_ENABLE, GLOBAL_ENABLE))
		return 1;
	return 0;
}


static int max98396_reset(Max98396Codec *codec)
{
	// Reset the codec and disable all interrupts
	if (max98396_write(codec, MAX396_REG_SOFT_RESET, SOFT_RESET_TRIGGER))
		return 1;
	mdelay(1);
	// Enable clock monitor
	if (max98396_write(codec, MAX396_REG_CLOCK_MONITOR, CMON_EN))
		return 1;
	return 0;
}

static int max98396_power(Max98396Codec *codec, uint8_t enable)
{
	// Enable speaker path
	if (max98396_update(codec, MAX396_REG_SPEAKER_PATH, SPEAKER_ENABLE,
			    enable))
		return 1;
	// Global enable to power up codec
	if (max98396_update(codec, MAX396_REG_GLOBAL_ENABLE, GLOBAL_ENABLE,
			    enable))
		return 1;
	mdelay(1);
	return 0;
}

static int max98396_enable(SoundRouteComponentOps *me)
{
	Max98396Codec *codec = container_of(me, Max98396Codec, component.ops);
	max98396_reset(codec);
	max98396_i2s_pcm_format(codec);
	max98396_hw_params(codec);
	max98396_power(codec, 1);
	return 0;
}

static int max98396_disable(SoundRouteComponentOps *me)
{
	Max98396Codec *codec = container_of(me, Max98396Codec, component.ops);
	max98396_power(codec, 0);
	return 0;
}

Max98396Codec *new_max98396_codec(I2cOps *i2c, uint8_t chip)
{
	Max98396Codec *codec = xzalloc(sizeof(*codec));
	codec->component.ops.enable = &max98396_enable;
	codec->component.ops.disable = &max98396_disable;
	codec->i2c = i2c;
	codec->chip = chip;
	return codec;
}
