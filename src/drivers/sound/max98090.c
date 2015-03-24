/*
 * max98090.c -- MAX98090 ALSA SoC Audio driver
 *
 * Copyright 2011 Maxim Integrated Products
 * Copyright 2013 Google Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <libpayload.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/max98090.h"

static int max98090_i2c_write(Max98090Codec *codec, uint8_t reg, uint8_t data)
{
	return i2c_writeb(codec->i2c, codec->chip, reg, data);
}

static int max98090_i2c_read(Max98090Codec *codec, uint8_t reg, uint8_t *data)
{
	return i2c_readb(codec->i2c, codec->chip, reg, data);
}

static int max98090_update_bits(Max98090Codec *codec, uint8_t reg,
				uint8_t mask, uint8_t value)
{
	uint8_t old;
	if (max98090_i2c_read(codec, reg, &old))
		return 1;
	uint8_t new_value = (old & ~mask) | (value & mask);
	if (old != new_value && max98090_i2c_write(codec, reg, new_value))
		return 1;

	return 0;
}

static int max98090_hw_params(Max98090Codec *codec)
{
	unsigned char value;
	unsigned int sample_rate = codec->sample_rate;
	unsigned int bits_per_sample = codec->bits_per_sample;

	switch (bits_per_sample) {
	case 16:
		max98090_i2c_read(codec, M98090_REG_INTERFACE_FORMAT, &value);
		if (max98090_update_bits(codec, M98090_REG_INTERFACE_FORMAT,
					 M98090_WS_MASK, 0))
			return 1;
		max98090_i2c_read(codec, M98090_REG_INTERFACE_FORMAT, &value);
		break;
	default:
		printf("%s: Illegal bits per sample %d.\n",
		       __func__, bits_per_sample);
		return 1;
	}

	/* Update filter mode */
	int mode_mask = (sample_rate < 24000) ? 0 : M98090_MODE_MASK;
	if (max98090_update_bits(codec, M98090_REG_FILTER_CONFIG,
				 M98090_MODE_MASK, mode_mask))
		return 1;

	/* Update sample rate mode */
	int rate = (sample_rate < 50000) ? 0 : M98090_DHF_MASK;
	if (max98090_update_bits(codec, M98090_REG_FILTER_CONFIG,
				 M98090_DHF_MASK, rate))
		return 1;
	return 0;
}

// Configures audio interface system clock for the selected frequency.
static int max98090_set_sysclk(Max98090Codec *codec)
{
	enum { MHz = 1000000 };

	int freq = codec->lr_frame_size * codec->sample_rate;
	// TODO(hungte) Should we handle codec->master_clock?
	uint8_t mclksel = 0;

	/*
	 * Setup clocks for slave mode, and using the PLL
	 * PSCLK = 0x01 (when master clk is 10MHz to 20MHz)
	 *	0x02 (when master clk is 20MHz to 40MHz)..
	 *	0x03 (when master clk is 40MHz to 60MHz)..
	 */
	if ((freq >= 10 * MHz) && (freq < 20 * MHz)) {
		mclksel |= M98090_PSCLK_DIV1;
	} else if ((freq >= 20 * MHz) && (freq < 40 * MHz)) {
		mclksel |= M98090_PSCLK_DIV2;
	} else if ((freq >= 40 * MHz) && (freq < 60 * MHz)) {
		mclksel |= M98090_PSCLK_DIV4;
	} else {
		printf("%s: Invalid master clock frequency\n", __func__);
		return 1;
	}

	if (max98090_i2c_write(codec, M98090_REG_SYSTEM_CLOCK, mclksel))
		return 1;

	printf("%s: Clock at %uHz\n", __func__, freq);
	return 0;
}

// Sets Max98090 I2S format.
static int max98090_set_fmt(Max98090Codec *codec)
{
	// Set to slave mode PLL - MAS mode off.
	if (max98090_i2c_write(codec, M98090_REG_CLOCK_RATIO_NI_MSB, 0x00) ||
	    max98090_i2c_write(codec, M98090_REG_CLOCK_RATIO_NI_LSB, 0x00))
		return 1;

	if (max98090_update_bits(codec, M98090_REG_CLOCK_MODE,
				 M98090_USE_M1_MASK, 0))
		return 1;

	if (max98090_i2c_write(codec, M98090_REG_MASTER_MODE, 0))
		return 1;

	// Format: I2S, IB_IF.
	if (max98090_i2c_write(codec, M98090_REG_INTERFACE_FORMAT,
			       M98090_DLY_MASK | M98090_BCI_MASK |
			       M98090_WCI_MASK))
		return 1;

	return 0;
}

// Resets the audio codec.
static int max98090_reset(Max98090Codec *codec)
{
	// Gracefully reset the DSP core and the codec hardware in a proper
	// sequence.
	if (max98090_i2c_write(codec, M98090_REG_SOFTWARE_RESET,
			       M98090_SWRESET_MASK))
		return 1;

	mdelay(20);
	return 0;
}

// Initialize max98090 codec device.
int max98090_device_init(Max98090Codec *codec)
{
	// Reset the codec, the DSP core, and disable all interrupts.
	if (max98090_reset(codec))
		return 1;


	uint8_t id;
	if (max98090_i2c_read(codec, M98090_REG_REVISION_ID, &id))
		return 1;
	printf("%s: Hardware revision: %c\n", __func__, (id - 0x40) + 'A');

	/* Reading interrupt status to clear them */
	int res = 0;
	res = max98090_i2c_read(codec, M98090_REG_DEVICE_STATUS, &id);

	res |= max98090_i2c_write(codec, M98090_REG_DAC_CONTROL,
				  M98090_DACHP_MASK);
	res |= max98090_i2c_write(codec, M98090_REG_BIAS_CONTROL,
				  M98090_VCM_MODE_MASK);

	res |= max98090_i2c_write(codec, M98090_REG_LEFT_SPK_MIXER, 0x1);
	res |= max98090_i2c_write(codec, M98090_REG_RIGHT_SPK_MIXER, 0x2);

	res |= max98090_i2c_write(codec, M98090_REG_LEFT_SPK_VOLUME, 0x25);
	res |= max98090_i2c_write(codec, M98090_REG_RIGHT_SPK_VOLUME, 0x25);

	res |= max98090_i2c_write(codec, M98090_REG_CLOCK_RATIO_NI_MSB, 0x0);
	res |= max98090_i2c_write(codec, M98090_REG_CLOCK_RATIO_NI_LSB, 0x0);
	res |= max98090_i2c_write(codec, M98090_REG_MASTER_MODE, 0x0);
	res |= max98090_i2c_write(codec, M98090_REG_INTERFACE_FORMAT, 0x0);
	res |= max98090_i2c_write(codec, M98090_REG_IO_CONFIGURATION,
				  M98090_SDIEN_MASK);
	res |= max98090_i2c_write(codec, M98090_REG_DEVICE_SHUTDOWN,
				  M98090_SHDNN_MASK);
	res |= max98090_i2c_write(codec, M98090_REG_OUTPUT_ENABLE,
				  M98090_HPREN_MASK | M98090_HPLEN_MASK |
				  M98090_SPREN_MASK | M98090_SPLEN_MASK |
				  M98090_DAREN_MASK | M98090_DALEN_MASK);
	res |= max98090_i2c_write(codec, M98090_REG_IO_CONFIGURATION,
				  M98090_SDOEN_MASK | M98090_SDIEN_MASK);

	return res;
}

static int max98090_enable(SoundRouteComponentOps *me)
{
	Max98090Codec *codec = container_of(me, Max98090Codec, component.ops);

	return max98090_device_init(codec) || max98090_set_sysclk(codec) ||
		max98090_hw_params(codec) || max98090_set_fmt(codec);
}

Max98090Codec *new_max98090_codec(I2cOps *i2c, uint8_t chip,
				  int bits_per_sample, int sample_rate,
				  int lr_frame_size, uint8_t master_clock)
{
	Max98090Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &max98090_enable;

	codec->i2c = i2c;
	codec->chip = chip;

	codec->bits_per_sample = bits_per_sample;
	codec->sample_rate = sample_rate;
	codec->lr_frame_size = lr_frame_size;

	codec->master_clock = master_clock;
	return codec;
}
