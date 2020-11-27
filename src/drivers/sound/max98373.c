/*
 * max98373.c -- Maxim Integrated 98373 using DSP & I2S driven approach
 *
 * Copyright 2020 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/gpio/gpio.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/max98373.h"

/* I2S controller settings for Max98373 codec*/
const I2sSettings max98373_settings = {
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

static int max98373_write(Max98373Codec *codec, uint16_t reg, uint8_t data)
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

static int max98373_read(Max98373Codec *codec, uint16_t reg, uint8_t *data)
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

static int max98373_update(Max98373Codec *codec, uint16_t reg,
			   uint8_t mask, uint8_t value)
{
	uint8_t old;
	if (max98373_read(codec, reg, &old))
		return 1;
	uint8_t new = (old & ~mask) | (value & mask);
	if (old != new && max98373_write(codec, reg, new))
		return 1;
	return 0;
}

static int max98373_sample_rate(int sample_rate)
{
	const int max98373_sample_rates[] = {
		[SAMPLE_RATE_16000HZ] = 16000,
		[SAMPLE_RATE_22050HZ] = 22050,
		[SAMPLE_RATE_24000HZ] = 24000,
		[SAMPLE_RATE_32000HZ] = 32000,
		[SAMPLE_RATE_44100HZ] = 44100,
		[SAMPLE_RATE_48000HZ] = 48000,
	};
	const int sample_rate_count = ARRAY_SIZE(max98373_sample_rates);
	int rate;
	for (rate = 0; rate < sample_rate_count; rate++)
		if (sample_rate <= max98373_sample_rates[rate])
			return rate;
	return -1;
}

static int max98373_set_sample_rate(Max98373Codec *codec)
{
	int sample_rate = max98373_sample_rate(codec->sample_rate);
	if (sample_rate < 0)
		return 1;
	// Set speaker sample rate in upper nibble
	max98373_update(codec, REG_SAMPLE_RATE_2,
			SAMPLE_RATE_MASK << 4, sample_rate << 4);
	return 0;
}

static int max98373_tone_frequency(uint32_t divisor)
{
	const int max98373_freq_divs[] = {
		[TONE_CONFIG_FS_4] = 4,
		[TONE_CONFIG_FS_6] = 6,
		[TONE_CONFIG_FS_8] = 8,
		[TONE_CONFIG_FS_16] = 16,
		[TONE_CONFIG_FS_32] = 32,
		[TONE_CONFIG_FS_64] = 64,
		[TONE_CONFIG_FS_128] = 128,
	};
	const int freq_divs_count = ARRAY_SIZE(max98373_freq_divs);
	int val;

	for (val = 0; val < freq_divs_count; val++)
		if (divisor <= max98373_freq_divs[val])
			return val;
	return 0;
}

static int max98373_tone(Max98373Codec *codec, uint32_t frequency)
{
	// Set tone generator based on sample rate and frequency
	uint8_t tone_config = TONE_CONFIG_DISABLED;
	if (frequency > 0) {
		uint32_t divisor = codec->sample_rate / frequency;
		tone_config = max98373_tone_frequency(divisor);
	}
	max98373_update(codec, REG_TONE_CONFIG, TONE_CONFIG_MASK,
			tone_config);
	return 0;
}

static int max98373_hw_params(Max98373Codec *codec)
{
	// Set speaker gain to 0 dB, max gain 8 dB
	if (max98373_write(codec, REG_AMP_GAIN,
			   SPEAKER_GAIN_0DB | SPEAKER_GAIN_MAX_5P4V))
		return 1;

	// Set speaker amp to -6 dB
	if (max98373_write(codec, REG_AMP_VOLUME,
			   SPEAKER_AMP_VOLUME_M6DB))
		return 1;

	return 0;
}

static int max98373_reset(Max98373Codec *codec)
{
	// Reset the codec and disable all interrupts
	if (max98373_write(codec, REG_SOFT_RESET, SOFT_RESET_TRIGGER))
		return 1;
	mdelay(1);
	// Enable clock monitor
	if (max98373_write(codec, REG_CLOCK_MONITOR, CMON_EN))
		return 1;
	return 0;
}

static int max98373_power(Max98373Codec *codec, uint8_t enable)
{
	// Enable speaker path
	if (max98373_update(codec, REG_SPEAKER_PATH, SPEAKER_ENABLE, enable))
		return 1;
	// Global enable to power up codec
	if (max98373_update(codec, REG_GLOBAL_ENABLE, GLOBAL_ENABLE, enable))
		return 1;
	mdelay(1);
	return 0;
}

static int max98373_play(SoundOps *me, uint32_t msec, uint32_t frequency)
{
	Max98373Codec *codec = container_of(me, Max98373Codec, ops);
	int bclk = 0;
	int lrclk = 0;
	u64 start;

	if (!frequency)
		return -1;

	max98373_reset(codec);
	max98373_set_sample_rate(codec);
	max98373_power(codec, 1);
	max98373_hw_params(codec);
	max98373_tone(codec, frequency << 2);

	start = timer_us(0);

	while (timer_us(start)/1000 < msec) {
		gpio_set(codec->bclk_gpio, bclk++ & 1);
		if ((bclk % 16) == 0)
			gpio_set(codec->lrclk_gpio, lrclk++ & 1);
	}

	max98373_tone(codec, 0);
	max98373_power(codec, 0);

	return 0;
}

Max98373Codec *new_max98373_tone_generator(I2cOps *i2c, uint8_t chip,
					   int sample_rate,
					   GpioOps *bclk_gpio,
					   GpioOps *lrclk_gpio)
{
	/* Max sample rate with tone generator is 48kHz */
	if (sample_rate > 48000)
		return NULL;

	Max98373Codec *codec = xzalloc(sizeof(*codec));

	codec->ops.play = &max98373_play;

	codec->i2c = i2c;
	codec->chip = chip;
	codec->sample_rate = sample_rate;
	codec->bclk_gpio = bclk_gpio;
	codec->lrclk_gpio = lrclk_gpio;

	return codec;
}

static int max98373_i2s_pcm_format(Max98373Codec *codec)
{
	// Set PCM FORMAT to I2S
	if (max98373_write(codec, REG_PCM_INTERFACE, PCM_INTERFACE))
		return 1;
	// Set SPK Rx path enable
	if (max98373_write(codec, REG_DATA_INPUT_EN, DATA_INPUT_EN))
		return 1;
	return 0;
}
static int max98373_enable(SoundRouteComponentOps *me)
{
	Max98373Codec *codec = container_of(me, Max98373Codec, component.ops);
	max98373_reset(codec);
	max98373_power(codec, 1);
	max98373_i2s_pcm_format(codec);
	max98373_hw_params(codec);
	return 0;
}

static int max98373_disable(SoundRouteComponentOps *me)
 {
	Max98373Codec *codec = container_of(me, Max98373Codec, component.ops);
	max98373_power(codec, 0);
	return 0;
 }

Max98373Codec *new_max98373_codec(I2cOps *i2c, uint8_t chip)
{
	Max98373Codec *codec = xzalloc(sizeof(*codec));
	codec->component.ops.enable = &max98373_enable;
	codec->component.ops.disable = &max98373_disable;
	codec->i2c = i2c;
	codec->chip = chip;
	return codec;
 }
