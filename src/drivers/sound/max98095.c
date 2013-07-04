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

#include "config.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/bus/i2s/i2s.h"
#include "drivers/gpio/gpio.h"
#include "drivers/gpio/s5p.h"
#include "drivers/sound/i2s.h"
#include "drivers/sound/max98095.h"

static I2cOps *bus;

void max98095_set_i2c_bus(I2cOps *_bus)
{
	bus = _bus;
}

static int max98095_i2c_write(int reg, uint8_t data)
{
	if (!bus)
		return 1;
	if (bus->write(bus, CONFIG_DRIVER_SOUND_MAX98095_ADDR,
		       reg, 1, &data, 1)) {
		printf("%s: Error while writing register %#04x with %#04x\n",
			__func__, reg, data);
		return 1;
	}
	return 0;
}

static unsigned int max98095_i2c_read(int reg, uint8_t *data)
{
	if (!bus)
		return 1;
	if (bus->read(bus, CONFIG_DRIVER_SOUND_MAX98095_ADDR,
		      reg, 1, data, 1)) {
		printf("%s: Error while reading register %#04x\n",
			__func__, reg);
		return 1;
	}
	return 0;
}

static int max98095_update_bits(int reg, uint8_t mask, uint8_t value)
{
	uint8_t old;
	if (max98095_i2c_read(reg, &old))
		return 1;
	uint8_t new = (old & ~mask) | (value & mask);
	if (old != new && max98095_i2c_write(reg, new))
		return 1;

	return 0;
}

static int max98095_hw_params(int rate, int bits_per_sample)
{
	switch (bits_per_sample) {
	case 16:
		if (max98095_update_bits(M98095_034_DAI2_FORMAT,
					 M98095_DAI_WS, 0))
			return 1;
		break;
	case 24:
		if (max98095_update_bits(M98095_034_DAI2_FORMAT,
					 M98095_DAI_WS, M98095_DAI_WS))
			return 1;
		break;
	default:
		printf("%s: Illegal bits per sample %d.\n", __func__,
			bits_per_sample);
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
		if (rate_table[i] >= rate) {
			index = i;
			break;
		}
	}
	if (index == table_size) {
		printf("%s: Failed to set sample rate %d.\n", __func__, rate);
		return 1;
	}

	if (max98095_update_bits(M98095_031_DAI2_CLKMODE,
				 M98095_CLKMODE_MASK, index))
		return 1;

	/* Update sample rate mode */
	if (max98095_update_bits(M98095_038_DAI2_FILTERS,
			M98095_DAI_DHF, (rate < 50000) ? 0 : M98095_DAI_DHF))
		return 1;

	return 0;
}

// Configures audio interface system clock for the selected frequency.
static int max98095_set_sysclk(int freq, uint8_t master_clock)
{
	enum { MHz = 1000000 };

	uint8_t mclksel = (master_clock == 2) ? 1 : 0;

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
	if (max98095_i2c_write(M98095_026_SYS_CLK, mclksel)) {
		printf("Failed to set the master clock.\n");
		return 1;
	}

	printf("%s: Clock at %uHz on MCLK%d\n", __func__, freq,
		master_clock);

	return 0;
}

// Sets Max98095 I2S format.
static int max98095_set_fmt(void)
{
	// Slave mode PLL.
	if (max98095_i2c_write(M98095_032_DAI2_CLKCFG_HI, 0x80) ||
		max98095_i2c_write(M98095_033_DAI2_CLKCFG_LO, 0x00))
		return 1;

	if (max98095_update_bits(M98095_034_DAI2_FORMAT,
			M98095_DAI_MAS | M98095_DAI_DLY | M98095_DAI_BCI |
			M98095_DAI_WCI, M98095_DAI_DLY))
		return 1;

	if (max98095_i2c_write(M98095_035_DAI2_CLOCK, M98095_DAI_BSEL64))
		return 1;

	return 0;
}

// Resets the audio codec.
static int max98095_reset(void)
{
	// Gracefully reset the DSP core and the codec hardware in a proper
	// sequence.
	if (max98095_i2c_write(M98095_00F_HOST_CFG, 0)) {
		printf("%s: Failed to reset DSP\n", __func__);
		return 1;
	}

	if (max98095_i2c_write(M98095_097_PWR_SYS, 0)) {
		printf("%s: Failed to reset codec\n", __func__);
		return 1;
	}

	// Reset to hardware default for registers, as there is not a soft
	// reset hardware control register.
	for (int i = M98095_010_HOST_INT_CFG; i < M98095_REG_MAX_CACHED; i++) {
		if (max98095_i2c_write(i, 0)) {
			printf("%s: Failed to reset\n", __func__);
			return 1;
		}
	}

	return 0;
}

// Intialize max98095 codec device.
static int max98095_device_init(void)
{
	// Reset the codec, the DSP core, and disable all interrupts.
	if (max98095_reset())
		return 1;

	uint8_t id;
	if (max98095_i2c_read(M98095_0FF_REV_ID, &id)) {
		printf("%s: Failure reading hardware revision: %d\n",
			__func__, id);
		return 1;
	}
	printf("%s: Hardware revision: %c\n", __func__, (id - 0x40) + 'A');

	int res = 0;
	res |= max98095_i2c_write(M98095_097_PWR_SYS, M98095_PWRSV);

	// Initialize registers to hardware default configuring audio
	// interface2 to DAC.
	res |= max98095_i2c_write(M98095_048_MIX_DAC_LR,
		M98095_DAI2M_TO_DACL | M98095_DAI2M_TO_DACR);

	res |= max98095_i2c_write(M98095_092_PWR_EN_OUT,
			M98095_SPK_SPREADSPECTRUM);
	res |= max98095_i2c_write(M98095_045_CFG_DSP, M98095_DSPNORMAL);
	res |= max98095_i2c_write(M98095_04E_CFG_HP, M98095_HPNORMAL);

	res |= max98095_i2c_write(M98095_02C_DAI1_IOCFG,
			M98095_S1NORMAL | M98095_SDATA);

	res |= max98095_i2c_write(M98095_036_DAI2_IOCFG,
			M98095_S2NORMAL | M98095_SDATA);

	res |= max98095_i2c_write(M98095_040_DAI3_IOCFG,
			M98095_S3NORMAL | M98095_SDATA);

	// Take the codec out of the shut down.
	res |= max98095_update_bits(M98095_097_PWR_SYS, M98095_SHDNRUN,
			M98095_SHDNRUN);
	// Route DACL and DACR output to HO and speakers.
	res |= max98095_i2c_write(M98095_050_MIX_SPK_LEFT, 0x01); // DACL
	res |= max98095_i2c_write(M98095_051_MIX_SPK_RIGHT, 0x01);// DACR
	res |= max98095_i2c_write(M98095_04C_MIX_HP_LEFT, 0x01);  // DACL
	res |= max98095_i2c_write(M98095_04D_MIX_HP_RIGHT, 0x01); // DACR

	// Power enable.
	res |= max98095_i2c_write(M98095_091_PWR_EN_OUT, 0xF3);

	// Set volume.
	res |= max98095_i2c_write(M98095_064_LVL_HP_L, 15);
	res |= max98095_i2c_write(M98095_065_LVL_HP_R, 15);
	res |= max98095_i2c_write(M98095_067_LVL_SPK_L, 16);
	res |= max98095_i2c_write(M98095_068_LVL_SPK_R, 16);

	// Enable DAIs.
	res |= max98095_i2c_write(M98095_093_BIAS_CTRL, 0x30);
	res |= max98095_i2c_write(M98095_096_PWR_DAC_CK, 0x07);

	return res;
}

int codec_init(int bits_per_sample, int sample_rate, int lr_frame_size)
{
	if (max98095_device_init())
		return 1;

	uint8_t master_clock = CONFIG_DRIVER_SOUND_MAX98095_MASTER_CLOCK_SEL;
	if (max98095_set_sysclk(lr_frame_size * sample_rate, master_clock))
		return 1;

	if (max98095_hw_params(sample_rate, bits_per_sample))
		return 1;

	if (max98095_set_fmt())
		return 1;

	return 0;
}
