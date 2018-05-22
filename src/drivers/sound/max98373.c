/*
 * max98373.c -- Maxim Integrated 98373 using Tone Generator
 *
 * Copyright 2018 Google Inc.
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

// MAX98373 uses 16bit registers
enum {
	REG_SOFT_RESET = 0x2000,
	REG_SAMPLE_RATE_2 = 0x2028,

	REG_AMP_VOLUME = 0x203d,
	REG_AMP_GAIN = 0x203e,

	REG_TONE_CONFIG = 0x2040,
	REG_SPEAKER_PATH = 0x2043,
	REG_CLOCK_MONITOR = 0x20fe,
	REG_GLOBAL_ENABLE = 0x20ff,
};

// REG_SPEAKER_GAIN
enum {
	SPEAKER_GAIN_MASK = 0xf0,
	SPEAKER_GAIN_0DB = 0x00,
	SPEAKER_GAIN_1DB = 0x20,
	SPEAKER_GAIN_2DB = 0x40,
	SPEAKER_GAIN_3DB = 0x60,
	SPEAKER_GAIN_4DB = 0x80,
	SPEAKER_GAIN_5DB = 0x90,
	SPEAKER_GAIN_6DB = 0xa0,
};

enum {
	SPEAKER_GAIN_MAX_MASK = 0x0f,
	SPEAKER_GAIN_MAX_5P4V = 0x00, /* 5.43 Vp */
};

enum {
	SPEAKER_AMP_VOLUME_MASK = 0x7f,
	SPEAKER_AMP_VOLUME_0DB = 0x00,
	SPEAKER_AMP_VOLUME_M6DB = 0x0c, /* -6 dB */
};

// REG_PCM_SAMPLE_RATE
enum {
	SAMPLE_RATE_MASK = 0x0f,
	SAMPLE_RATE_16000HZ = 0x03,
	SAMPLE_RATE_22050HZ = 0x04,
	SAMPLE_RATE_24000HZ = 0x05,
	SAMPLE_RATE_32000HZ = 0x06,
	SAMPLE_RATE_44100HZ = 0x07,
	SAMPLE_RATE_48000HZ = 0x08,
};

// REG_SPEAKER_PATH
enum {
	SPEAKER_ENABLE = 0x01,
};

// REG_CLOCK_MONITOR
enum {
	CMON_EN = 0x01,
};

// REG_TONE_CONFIG
enum {
	TONE_CONFIG_MASK = 0x0f,
	TONE_CONFIG_DISABLED = 0x00,
	TONE_CONFIG_FS_4 = 0x01,
	TONE_CONFIG_FS_6 = 0x02,
	TONE_CONFIG_FS_8 = 0x03,
	TONE_CONFIG_FS_16 = 0x04,
	TONE_CONFIG_FS_32 = 0x05,
	TONE_CONFIG_FS_64 = 0x06,
	TONE_CONFIG_FS_128 = 0x07,
};

// REG_GLOBAL_ENABLE
enum {
	GLOBAL_ENABLE = 0x01,
};

// REG_SOFT_RESET
enum {
	SOFT_RESET_TRIGGER = 0x01,
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
