/*
 * max98927.c -- MAX98927 ALSA SoC Audio driver
 *
 * Copyright 2016 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <libpayload.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/max98927.h"

// MAX98927 uses 16bit registers
enum {
	REG_THERMAL_WARNING = 0x0014,
	REG_THERMAL_SHUTDOWN = 0x0015,
	REG_PCM_RX_ENABLE = 0x0018,
	REG_PCM_MODE_CONFIG = 0x0020,
	REG_PCM_CLOCK_SETUP = 0x0022,
	REG_PCM_SAMPLE_RATE_1 = 0x0023,
	REG_PCM_SAMPLE_RATE_2 = 0x0024,
	REG_AMP_VOLUME = 0x0036,
	REG_AMP_ENABLE = 0x003a,
	REG_SPEAKER_SOURCE = 0x003b,
	REG_SPEAKER_GAIN = 0x003c,
	REG_MEAS_ADC_CONFIG = 0x0043,
	REG_GLOBAL_ENABLE = 0x00ff,
	REG_SOFT_RESET = 0x0100,
};

// REG_SPEAKER_GAIN: PCM[2:0] PDM[6:4]
enum {
	SPEAKER_GAIN_MASK = 0x07,
	SPEAKER_GAIN_MUTE = 0x00,
	SPEAKER_GAIN_3DB = 0x01,
	SPEAKER_GAIN_6DB = 0x02,
	SPEAKER_GAIN_9DB = 0x03,
	SPEAKER_GAIN_12DB = 0x04,
	SPEAKER_GAIN_15DB = 0x05,
	SPEAKER_GAIN_18DB = 0x06,
};

// REG_AMP_VOLUME
enum {
	AMP_VOLUME_0DB = 0x40,
};

// REG_THERMAL_WARNING and REG_THERMAL_SHUTDOWN
enum {
	THERMAL_100C = 0x6b,
	THERMAL_124C = 0x78,
	THERMAL_150C = 0x8c,
};

// REG_PCM_RX_ENABLE
enum {
	PCM_RX_EN_CH7 = 0x80,
	PCM_RX_EN_CH6 = 0x40,
	PCM_RX_EN_CH5 = 0x20,
	PCM_RX_EN_CH4 = 0x10,
	PCM_RX_EN_CH3 = 0x08,
	PCM_RX_EN_CH2 = 0x04,
	PCM_RX_EN_CH1 = 0x02,
	PCM_RX_EN_CH0 = 0x01,
};

// REG_MEAS_ADC_CONFIG
enum {
	MEAS_ADC_EN_MASK = 0x07,
	MEAS_ADC_CH2_EN = 0x04,
	MEAS_ADC_CH1_EN = 0x02,
	MEAS_ADC_CH0_EN = 0x01,
};

// REG_PCM_MODE_CONFIG
enum {
	PCM_CHANSZ_MASK = 0xc0,
	PCM_CHANSZ_16 = 0x40,
	PCM_CHANSZ_24 = 0x80,
	PCM_CHANSZ_32 = 0xc0,

	PCM_FORMAT_MASK = 0x38,
	PCM_FORMAT_I2S = 0x00,
	PCM_FORMAT_LEFT_JUSTIFIED = 0x08,
	PCM_FORMAT_TDM_MODE_0 = 0x18,
	PCM_FORMAT_TDM_MODE_1 = 0x20,
	PCM_FORMAT_TDM_MODE_2 = 0x30,

	PCM_BCLKEDGE_MASK = 0x04,
	PCM_BCLKEDGE_RISING = 0x00,
	PCM_BCLKEDGE_FALLING = 0x04,

	PCM_CHANSEL_MASK = 0x02,
	PCM_CHANSEL_LRCLK_POLARITY_I2S_FALLING = 0x00,
	PCM_CHANSEL_LRCLK_POLARITY_I2S_RISING = 0x02,
	PCM_CHANSEL_LRCLK_POLARITY_TDM_FALLING = 0x02,
	PCM_CHANSEL_LRCLK_POLARITY_TDM_RISING = 0x00,

	PCM_TX_EXTRA_HIZ = 0x01,
	PCM_TX_EXTRA_HIZ_DOUT_ZERO = 0x00,
	PCM_TX_EXTRA_HIZ_DOUT_HIZ = 0x01,
};

// REG_PCM_CLOCK_SETUP
enum {
	PCM_BSEL_MASK = 0x0f,
	PCM_BSEL_32 = 0x02,
	PCM_BSEL_48 = 0x03,
	PCM_BSEL_64 = 0x04,
	PCM_BSEL_96 = 0x05,
	PCM_BSEL_128 = 0x06,
	PCM_BSEL_192 = 0x07,
	PCM_BSEL_256 = 0x08,
	PCM_BSEL_384 = 0x09,
	PCM_BSEL_512 = 0x0a,
};

// REG_PCM_SAMPLE_RATE
enum {
	SAMPLE_RATE_MASK = 0x0f,
	SAMPLE_RATE_8000HZ = 0x00,
	SAMPLE_RATE_11025HZ = 0x01,
	SAMPLE_RATE_12000HZ = 0x02,
	SAMPLE_RATE_16000HZ = 0x03,
	SAMPLE_RATE_22050HZ = 0x04,
	SAMPLE_RATE_24000HZ = 0x05,
	SAMPLE_RATE_32000HZ = 0x06,
	SAMPLE_RATE_44100HZ = 0x07,
	SAMPLE_RATE_48000HZ = 0x08,
	SAMPLE_RATE_88200HZ = 0x09,
	SAMPLE_RATE_96000HZ = 0x0a,
	SAMPLE_RATE_176400HZ = 0x0b,
	SAMPLE_RATE_192000HZ = 0x0c,
};

// REG_AMP_ENABLE
enum {
	AMP_ENABLE_SPEAKER_EN = 0x01,
};

// REG_SPEAKER_SOURCE
enum {
	SPEAKER_SOURCE_MASK = 0x03,
	SPEAKER_SOURCE_DAI = 0x00,
	SPEAKER_SOURCE_TONE = 0x02,
	SPEAKER_SOURCE_PDM = 0x03,
};

// REG_GLOBAL_ENABLE
enum {
	GLOBAL_ENABLE = 0x01,
};

// REG_SOFT_RESET
enum {
	SOFT_RESET_TRIGGER = 0x01,
};

static int max98927_write(Max98927Codec *codec, uint16_t reg, uint8_t data)
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

static int max98927_read(Max98927Codec *codec, uint16_t reg, uint8_t *data)
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

static int max98927_update(Max98927Codec *codec, uint8_t reg,
				uint8_t mask, uint8_t value)
{
	uint8_t old;
	if (max98927_read(codec, reg, &old))
		return 1;
	uint8_t new = (old & ~mask) | (value & mask);
	if (old != new && max98927_write(codec, reg, new))
		return 1;
	return 0;
}

static int max98927_hw_params(Max98927Codec *codec)
{
	// Set thermal warning and shutdown values for measurement ADC
	max98927_write(codec, REG_THERMAL_WARNING, THERMAL_124C);
	max98927_write(codec, REG_THERMAL_SHUTDOWN, THERMAL_150C);

	// Enable Measurment ADC channel 2 as required for AMP power
	max98927_update(codec, REG_MEAS_ADC_CONFIG,
			MEAS_ADC_EN_MASK, MEAS_ADC_CH2_EN);

	// Enable PCM Rx Enable for channel 0+1
	max98927_write(codec, REG_PCM_RX_ENABLE,
		       PCM_RX_EN_CH0 | PCM_RX_EN_CH1);

	// Set amplifier volume to 0dB
	max98927_write(codec, REG_AMP_VOLUME, AMP_VOLUME_0DB);

	// Set speaker gain to 6dB
	max98927_write(codec, REG_SPEAKER_GAIN, SPEAKER_GAIN_6DB);

	return 0;
}

static int max98927_sample_rate(int sample_rate)
{
	const int max98927_sample_rates[] = {
		[SAMPLE_RATE_8000HZ] = 8000,
		[SAMPLE_RATE_11025HZ] = 11025,
		[SAMPLE_RATE_12000HZ] = 12000,
		[SAMPLE_RATE_16000HZ] = 16000,
		[SAMPLE_RATE_22050HZ] = 22050,
		[SAMPLE_RATE_24000HZ] = 24000,
		[SAMPLE_RATE_32000HZ] = 32000,
		[SAMPLE_RATE_44100HZ] = 44100,
		[SAMPLE_RATE_48000HZ] = 48000,
		[SAMPLE_RATE_88200HZ] = 88200,
		[SAMPLE_RATE_96000HZ] = 96000,
		[SAMPLE_RATE_176400HZ] = 176400,
		[SAMPLE_RATE_192000HZ] = 192000,
	};
	const int sample_rate_count = ARRAY_SIZE(max98927_sample_rates);
	int rate;

	for (rate = 0; rate < sample_rate_count; rate++)
		if (sample_rate < max98927_sample_rates[rate])
			return rate;
	return 1;
}

static int max98927_bclk_to_lrclk(int lr_frame_size)
{
	const int max98927_bclk_to_lrclk[] = {
		[PCM_BSEL_32] = 32,
		[PCM_BSEL_48] = 48,
		[PCM_BSEL_64] = 64,
		[PCM_BSEL_96] = 96,
		[PCM_BSEL_128] = 128,
		[PCM_BSEL_192] = 192,
		[PCM_BSEL_256] = 256,
		[PCM_BSEL_384] = 384,
		[PCM_BSEL_512] = 512,
	};
	const int bclk_to_lrclk_count = ARRAY_SIZE(max98927_bclk_to_lrclk);
	int ratio;

	for (ratio = 0; ratio < bclk_to_lrclk_count; ratio++)
		if (lr_frame_size == max98927_bclk_to_lrclk[ratio])
			return ratio;
	return 1;
}

static int max98927_set_sysclk(Max98927Codec *codec)
{
	int sample_rate = max98927_sample_rate(codec->sample_rate);
	int lr_frame_size = max98927_bclk_to_lrclk(codec->lr_frame_size);

	if (sample_rate < 0 || lr_frame_size < 0)
		return 1;

	// Set PCM sample rate
	max98927_update(codec, REG_PCM_SAMPLE_RATE_1,
			SAMPLE_RATE_MASK, sample_rate);

	// Set speaker sample rate in upper nibble
	max98927_update(codec, REG_PCM_SAMPLE_RATE_2,
			SAMPLE_RATE_MASK << 4, sample_rate << 4);

	// Set BCLK to LRCLK ratio
	max98927_update(codec, REG_PCM_CLOCK_SETUP,
			PCM_BSEL_MASK, lr_frame_size);

	return 0;
}

static int max98927_set_fmt(Max98927Codec *codec)
{
	uint8_t mode;

	// Select I2S speaker source
	if (max98927_update(codec, REG_SPEAKER_SOURCE,
			    SPEAKER_SOURCE_MASK, SPEAKER_SOURCE_DAI))
		return 1;

	// Bits per sample
	if (codec->bits_per_sample == 16)
		mode = PCM_CHANSZ_16;
	else if (codec->bits_per_sample == 24)
		mode = PCM_CHANSZ_24;
	else if (codec->bits_per_sample == 32)
		mode = PCM_CHANSZ_32;
	else
		return 1;

	// PCM format is I2S
	mode |= PCM_FORMAT_I2S;

	// Falling LRCLK starts channel 0
	mode |= PCM_CHANSEL_LRCLK_POLARITY_I2S_FALLING;

	// Data captured and valid on falling edge of BCLK
	mode |= PCM_BCLKEDGE_FALLING;

	return max98927_write(codec, REG_PCM_MODE_CONFIG, mode);
}

static int max98927_reset(Max98927Codec *codec)
{
	// Reset the codec, the DSP core, and disable all interrupts
	if (max98927_write(codec, REG_SOFT_RESET, SOFT_RESET_TRIGGER))
		return 1;
	mdelay(1);
	return 0;
}

static int max98927_power(Max98927Codec *codec, uint8_t enable)
{
	// Enable the amplifier
	if (max98927_update(codec, REG_AMP_ENABLE,
			    AMP_ENABLE_SPEAKER_EN, enable))
		return 1;
	mdelay(1);

	// Global enable to power up codec
	if (max98927_update(codec, REG_GLOBAL_ENABLE, GLOBAL_ENABLE, enable))
		return 1;
	mdelay(1);

	return 0;
}

static int max98927_enable(SoundRouteComponentOps *me)
{
	Max98927Codec *codec = container_of(me, Max98927Codec, component.ops);

	return max98927_reset(codec) ||
		max98927_hw_params(codec) ||
		max98927_set_fmt(codec) ||
		max98927_set_sysclk(codec) ||
		max98927_power(codec, 1);
}

Max98927Codec *new_max98927_codec(I2cOps *i2c, uint8_t chip,
				  int bits_per_sample, int sample_rate,
				  int lr_frame_size)
{
	Max98927Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &max98927_enable;

	codec->i2c = i2c;
	codec->chip = chip;

	codec->bits_per_sample = bits_per_sample;
	codec->sample_rate = sample_rate;
	codec->lr_frame_size = lr_frame_size;

	return codec;
}
