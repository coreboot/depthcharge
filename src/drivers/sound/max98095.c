/*
 * max98095.c -- MAX98095 ALSA SoC Audio driver
 *
 * Copyright 2013 Google Inc.  All rights reserved.
 * Copyright 2011 Maxim Integrated Products
 *
 * Modified for uboot by R. Chandrasekar (rcsekar@samsung.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <libpayload.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/max98095.h"

static int max98095_i2c_write(Max98095Codec *codec, uint8_t reg, uint8_t data)
{
	return i2c_writeb(codec->i2c, codec->chip, reg, data);
}

static int max98095_i2c_read(Max98095Codec *codec, uint8_t reg, uint8_t *data)
{
	return i2c_readb(codec->i2c, codec->chip, reg, data);
}

static int max98095_update_bits(Max98095Codec *codec, uint8_t reg,
				uint8_t mask, uint8_t value)
{
	uint8_t old;
	if (max98095_i2c_read(codec, reg, &old))
		return 1;
	uint8_t new = (old & ~mask) | (value & mask);
	if (old != new && max98095_i2c_write(codec, reg, new))
		return 1;

	return 0;
}

static int max98095_hw_params(Max98095Codec *codec)
{
	switch (codec->bits_per_sample) {
	case 16:
		if (max98095_update_bits(codec, M98095_02A_DAI1_FORMAT,
					 M98095_DAI_WS, 0))
			return 1;
		if (max98095_update_bits(codec, M98095_034_DAI2_FORMAT,
					 M98095_DAI_WS, 0))
			return 1;
		break;
	case 24:
		if (max98095_update_bits(codec, M98095_02A_DAI1_FORMAT,
					 M98095_DAI_WS, M98095_DAI_WS))
			return 1;
		if (max98095_update_bits(codec, M98095_034_DAI2_FORMAT,
					 M98095_DAI_WS, M98095_DAI_WS))
			return 1;
		break;
	default:
		printf("%s: Illegal bits per sample %d.\n", __func__,
			codec->bits_per_sample);
		return 1;
	}

	// Index 0 is reserved.
	static const int rate_table[] = {
		0, 8000, 11025, 16000, 22050, 24000,
		32000, 44100, 48000, 88200, 96000
	};
	static const int table_size = ARRAY_SIZE(rate_table);

	uint8_t index = table_size;
	for (int i = 0; i < table_size; i++) {
		if (rate_table[i] >= codec->sample_rate) {
			index = i;
			break;
		}
	}
	if (index == table_size) {
		printf("%s: Failed to set sample rate %d.\n", __func__,
			codec->sample_rate);
		return 1;
	}

	if (max98095_update_bits(codec, M98095_027_DAI1_CLKMODE,
				 M98095_CLKMODE_MASK, index))
		return 1;

	if (max98095_update_bits(codec, M98095_031_DAI2_CLKMODE,
				 M98095_CLKMODE_MASK, index))
		return 1;

	/* Update sample rate mode */
	int rate = (codec->sample_rate < 50000) ? 0 : M98095_DAI_DHF;

	if (max98095_update_bits(codec, M98095_02E_DAI1_FILTERS,
				 M98095_DAI_DHF, rate))
		return 1;

	if (max98095_update_bits(codec, M98095_038_DAI2_FILTERS,
				 M98095_DAI_DHF, rate))
		return 1;

	return 0;
}

// Configures audio interface system clock for the selected frequency.
static int max98095_set_sysclk(Max98095Codec *codec)
{
	enum { MHz = 1000000 };

	int freq = codec->lr_frame_size * codec->sample_rate;
	uint8_t mclksel = (codec->master_clock == 2) ? 1 : 0;

	/*
	 * Setup clocks for slave mode, and using the PLL
	 * PSCLK = 0x01 (when master clk is 10MHz to 20MHz)
	 *	0x02 (when master clk is 20MHz to 40MHz)..
	 *	0x03 (when master clk is 40MHz to 60MHz)..
	 */
	if (freq >= 10 * MHz && freq < 20 * MHz) {
		mclksel |= M98095_PSCLK_DIV1;
	} else if (freq >= 20 * MHz && freq < 40 * MHz) {
		mclksel |= M98095_PSCLK_DIV2;
	} else if (freq >= 40 * MHz && freq < 60 * MHz) {
		mclksel |= M98095_PSCLK_DIV4;
	} else {
		printf("%s: Invalid master clock frequency\n", __func__);
		return 1;
	}

	if (max98095_i2c_write(codec, M98095_026_SYS_CLK, mclksel))
		return 1;

	printf("%s: Clock at %uHz on MCLK%d\n", __func__, freq,
		codec->master_clock);

	return 0;
}

// Sets Max98095 I2S format.
static int max98095_set_fmt(Max98095Codec *codec)
{
	// Slave mode PLL.
	if (max98095_i2c_write(codec, M98095_028_DAI1_CLKCFG_HI, 0x80) ||
		max98095_i2c_write(codec, M98095_029_DAI1_CLKCFG_LO, 0x00))
		return 1;

	if (max98095_update_bits(codec, M98095_02A_DAI1_FORMAT,
			M98095_DAI_MAS | M98095_DAI_DLY | M98095_DAI_BCI |
			M98095_DAI_WCI, M98095_DAI_DLY))
		return 1;

	if (max98095_i2c_write(codec, M98095_02B_DAI1_CLOCK, M98095_DAI_BSEL64))
		return 1;

	if (max98095_i2c_write(codec, M98095_032_DAI2_CLKCFG_HI, 0x80) ||
		max98095_i2c_write(codec, M98095_033_DAI2_CLKCFG_LO, 0x00))
		return 1;

	if (max98095_update_bits(codec, M98095_034_DAI2_FORMAT,
			M98095_DAI_MAS | M98095_DAI_DLY | M98095_DAI_BCI |
			M98095_DAI_WCI, M98095_DAI_DLY))
		return 1;

	if (max98095_i2c_write(codec, M98095_035_DAI2_CLOCK, M98095_DAI_BSEL64))
		return 1;

	return 0;
}

// Resets the audio codec.
static int max98095_reset(Max98095Codec *codec)
{
	// Gracefully reset the DSP core and the codec hardware in a proper
	// sequence.
	if (max98095_i2c_write(codec, M98095_00F_HOST_CFG, 0) ||
	    max98095_i2c_write(codec, M98095_097_PWR_SYS, 0))
		return 1;

	// Reset to hardware default for registers, as there is not a soft
	// reset hardware control register.
	for (int i = M98095_010_HOST_INT_CFG; i < M98095_REG_MAX_CACHED; i++)
		if (max98095_i2c_write(codec, i, 0))
			return 1;

	return 0;
}

// Intialize max98095 codec device.
static int max98095_device_init(Max98095Codec *codec)
{
	// Reset the codec, the DSP core, and disable all interrupts.
	if (max98095_reset(codec))
		return 1;

	uint8_t id;
	if (max98095_i2c_read(codec, M98095_0FF_REV_ID, &id))
		return 1;
	printf("%s: Hardware revision: %c\n", __func__, (id - 0x40) + 'A');

	int res = 0;
	res |= max98095_i2c_write(codec, M98095_097_PWR_SYS, M98095_PWRSV);

	// Initialize registers to hardware default configuring audio
	// interface 1 and 2 to DAC.
	res |= max98095_i2c_write(codec, M98095_048_MIX_DAC_LR,
		M98095_DAI1L_TO_DACL | M98095_DAI1R_TO_DACR |
		M98095_DAI2M_TO_DACL | M98095_DAI2M_TO_DACR);

	res |= max98095_i2c_write(codec, M98095_092_PWR_EN_OUT,
			M98095_SPK_SPREADSPECTRUM);
	res |= max98095_i2c_write(codec, M98095_045_CFG_DSP, M98095_DSPNORMAL);
	res |= max98095_i2c_write(codec, M98095_04E_CFG_HP, M98095_HPNORMAL);

	res |= max98095_i2c_write(codec, M98095_02C_DAI1_IOCFG,
			M98095_S1NORMAL | M98095_SDATA);

	res |= max98095_i2c_write(codec, M98095_036_DAI2_IOCFG,
			M98095_S2NORMAL | M98095_SDATA);

	res |= max98095_i2c_write(codec, M98095_040_DAI3_IOCFG,
			M98095_S3NORMAL | M98095_SDATA);

	// Take the codec out of the shut down.
	res |= max98095_update_bits(codec, M98095_097_PWR_SYS, M98095_SHDNRUN,
			M98095_SHDNRUN);
	// Route DACL and DACR output to HO and speakers.
	res |= max98095_i2c_write(codec, M98095_050_MIX_SPK_LEFT, 0x01); // DACL
	res |= max98095_i2c_write(codec, M98095_051_MIX_SPK_RIGHT, 0x01);// DACR
	res |= max98095_i2c_write(codec, M98095_04C_MIX_HP_LEFT, 0x01);  // DACL
	res |= max98095_i2c_write(codec, M98095_04D_MIX_HP_RIGHT, 0x01); // DACR

	// Power enable.
	res |= max98095_i2c_write(codec, M98095_091_PWR_EN_OUT, 0xF3);

	// Set volume.
	res |= max98095_i2c_write(codec, M98095_064_LVL_HP_L, 15);
	res |= max98095_i2c_write(codec, M98095_065_LVL_HP_R, 15);
	res |= max98095_i2c_write(codec, M98095_067_LVL_SPK_L, 16);
	res |= max98095_i2c_write(codec, M98095_068_LVL_SPK_R, 16);

	// Enable DAIs.
	res |= max98095_i2c_write(codec, M98095_093_BIAS_CTRL, 0x30);
	res |= max98095_i2c_write(codec, M98095_096_PWR_DAC_CK, 0x07);

	return res;
}

static int max98095_enable(SoundRouteComponentOps *me)
{
	Max98095Codec *codec = container_of(me, Max98095Codec, component.ops);

	return max98095_device_init(codec) || max98095_set_sysclk(codec) ||
		max98095_hw_params(codec) || max98095_set_fmt(codec);
}

Max98095Codec *new_max98095_codec(I2cOps *i2c, uint8_t chip,
				  int bits_per_sample, int sample_rate,
				  int lr_frame_size, uint8_t master_clock)
{
	Max98095Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &max98095_enable;

	codec->i2c = i2c;
	codec->chip = chip;

	codec->bits_per_sample = bits_per_sample;
	codec->sample_rate = sample_rate;
	codec->lr_frame_size = lr_frame_size;

	codec->master_clock = master_clock;
	return codec;
}
