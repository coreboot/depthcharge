/*
 * max98090.c -- MAX98090 ALSA SoC Audio driver
 *
 * Copyright 2011 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <asm/arch/clk.h>
#include <asm/arch/cpu.h>
#include <asm/arch/power.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <common.h>
#include <div64.h>
#include <fdtdec.h>
#include <i2c.h>
#include <sound.h>
#include "i2s.h"
#include "maxim_codec.h"
#include "max98090.h"

/*
 * Sets hw params for max98090
 *
 * @param max98090	max98090 information pointer
 * @param rate		Sampling rate
 * @param bits_per_sample	Bits per sample
 *
 * @return -1 for error  and 0  Success.
 */
int max98090_hw_params(struct maxim_codec_priv *max98090,
			unsigned int rate, unsigned int bits_per_sample)
{
	int error;
	unsigned char value;

	switch (bits_per_sample) {
	case 16:
		maxim_codec_i2c_read(M98090_REG_INTERFACE_FORMAT, &value);
		error = maxim_codec_update_bits(M98090_REG_INTERFACE_FORMAT,
			M98090_WS_MASK, 0);
		maxim_codec_i2c_read(M98090_REG_INTERFACE_FORMAT, &value);
		break;
	default:
		debug("%s: Illegal bits per sample %d.\n",
		      __func__, bits_per_sample);
		return -1;
	}

	/* Update filter mode */
	if (rate < 240000)
		error |= maxim_codec_update_bits(M98090_REG_FILTER_CONFIG,
			M98090_MODE_MASK, 0);
	else
		error |= maxim_codec_update_bits(M98090_REG_FILTER_CONFIG,
			M98090_MODE_MASK, M98090_MODE_MASK);

	/* Update sample rate mode */
	if (rate < 50000)
		error |= maxim_codec_update_bits(M98090_REG_FILTER_CONFIG,
			M98090_DHF_MASK, 0);
	else
		error |= maxim_codec_update_bits(M98090_REG_FILTER_CONFIG,
			M98090_DHF_MASK, M98090_DHF_MASK);

	if (error < 0) {
		debug("%s: Error setting hardware params.\n", __func__);
		return -1;
	}

	return 0;
}

/*
 * Configures Audio interface system clock for the given frequency
 *
 * @param max98090	max98090 information
 * @param freq		Sampling frequency in Hz
 *
 * @return -1 for error and 0 success.
 */
int max98090_set_sysclk(struct maxim_codec_priv *max98090,
				unsigned int freq)
{
	int error = 0;

	/* Requested clock frequency is already setup */
	if (freq == max98090->sysclk)
		return 0;

	/* Setup clocks for slave mode, and using the PLL
	 * PSCLK = 0x01 (when master clk is 10MHz to 20MHz)
	 *	0x02 (when master clk is 20MHz to 40MHz)..
	 *	0x03 (when master clk is 40MHz to 60MHz)..
	 */
	if ((freq >= 10000000) && (freq < 20000000)) {
		error = maxim_codec_i2c_write(M98090_REG_SYSTEM_CLOCK,
							M98090_PSCLK_DIV1);
	} else if ((freq >= 20000000) && (freq < 40000000)) {
		error = maxim_codec_i2c_write(M98090_REG_SYSTEM_CLOCK,
							M98090_PSCLK_DIV2);
	} else if ((freq >= 40000000) && (freq < 60000000)) {
		error = maxim_codec_i2c_write(M98090_REG_SYSTEM_CLOCK,
							M98090_PSCLK_DIV4);
	} else {
		debug("%s: Invalid master clock frequency\n", __func__);
		return -1;
	}

	debug("%s: Clock at %uHz\n", __func__, freq);

	if (error < 0)
		return -1;

	max98090->sysclk = freq;

	return 0;
}

/*
 * Sets Max98090 I2S format
 *
 * @param max98090	max98090 information
 * @param fmt		i2S format - supports a subset of the options defined
 *			in i2s.h.
 *
 * @return -1 for error and 0  Success.
 */
int max98090_set_fmt(struct maxim_codec_priv *max98090, int fmt)
{
	u8 regval = 0;
	int error = 0;

	if (fmt == max98090->fmt)
		return 0;

	max98090->fmt = fmt;

	switch (fmt & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		/* Set to slave mode PLL - MAS mode off */
		error |= maxim_codec_i2c_write(M98090_REG_CLOCK_RATIO_NI_MSB,
						0x00);
		error |= maxim_codec_i2c_write(M98090_REG_CLOCK_RATIO_NI_LSB,
						0x00);
		error |= maxim_codec_update_bits(M98090_REG_CLOCK_MODE,
			M98090_USE_M1_MASK, 0);
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		/* Set to master mode */
		debug("Master mode not supported\n");
		break;
	case SND_SOC_DAIFMT_CBS_CFM:
	case SND_SOC_DAIFMT_CBM_CFS:
	default:
		debug("%s: Clock mode unsupported\n", __func__);
		return -1;
	}

	error |= maxim_codec_i2c_write(M98090_REG_MASTER_MODE, regval);

	regval = 0;
	switch (fmt & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_I2S:
		regval |= M98090_DLY_MASK;
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		break;
	case SND_SOC_DAIFMT_RIGHT_J:
		regval |= M98090_RJ_MASK;
		break;
	case SND_SOC_DAIFMT_DSP_A:
		/* Not supported mode */
	default:
		debug("%s: Unrecognized format.\n", __func__);
		return -1;
	}

	switch (fmt & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		regval |= M98090_WCI_MASK;
		break;
	case SND_SOC_DAIFMT_IB_NF:
		regval |= M98090_BCI_MASK;
		break;
	case SND_SOC_DAIFMT_IB_IF:
		regval |= M98090_BCI_MASK|M98090_WCI_MASK;
		break;
	default:
		debug("%s: Unrecognized inversion settings.\n", __func__);
		return -1;
	}

	error |= maxim_codec_i2c_write(M98090_REG_INTERFACE_FORMAT,
		regval);

	if (error < 0) {
		debug("%s: Error setting i2s format.\n", __func__);
		return -1;
	}

	return 0;
}

/*
 * resets the audio codec
 *
 * @return -1 for error and 0 success.
 */
static int max98090_reset(void)
{
	int ret;

	/*
	 * Gracefully reset the DSP core and the codec hardware in a proper
	 * sequence.
	 */
	ret = maxim_codec_i2c_write(M98090_REG_SOFTWARE_RESET,
					M98090_SWRESET_MASK);
	if (ret != 0) {
		debug("%s: Failed to reset DSP: %d\n", __func__, ret);
		return ret;
	}

	mdelay(20);
	return 0;
}

/*
 * Intialise max98090 codec device
 *
 * @param max98090	max98090 information
 *
 * @returns -1 for error  and 0 Success.
 */
int max98090_device_init(struct maxim_codec_priv *max98090)
{
	unsigned char id;
	int error = 0;

	/* reset the codec, the DSP core, and disable all interrupts */
	error = max98090_reset();
	if (error != 0) {
		debug("Reset\n");
		return error;
	}

	/* initialize private data */
	max98090->sysclk = -1U;
	max98090->rate = -1U;
	max98090->fmt = -1U;

	error = maxim_codec_i2c_read(M98090_REG_REVISION_ID, &id);
	if (error < 0) {
		debug("%s: Failure reading hardware revision: %d\n",
		      __func__, id);
		return -1;
	}
	/* Reading interrupt status to clear them */
	error = maxim_codec_i2c_read(M98090_REG_DEVICE_STATUS, &id);

	error |= maxim_codec_i2c_write(M98090_REG_DAC_CONTROL,
							M98090_DACHP_MASK);
	error |= maxim_codec_i2c_write(M98090_REG_BIAS_CONTROL,
					M98090_VCM_MODE_MASK);

	error |= maxim_codec_i2c_write(M98090_REG_LEFT_SPK_MIXER, 0x1);
	error |= maxim_codec_i2c_write(M98090_REG_RIGHT_SPK_MIXER, 0x2);

	error |= maxim_codec_i2c_write(M98090_REG_LEFT_SPK_VOLUME, 0x25);
	error |= maxim_codec_i2c_write(M98090_REG_RIGHT_SPK_VOLUME, 0x25);

	error |= maxim_codec_i2c_write(M98090_REG_CLOCK_RATIO_NI_MSB, 0x0);
	error |= maxim_codec_i2c_write(M98090_REG_CLOCK_RATIO_NI_LSB, 0x0);
	error |= maxim_codec_i2c_write(M98090_REG_MASTER_MODE, 0x0);
	error |= maxim_codec_i2c_write(M98090_REG_INTERFACE_FORMAT, 0x0);
	error |= maxim_codec_i2c_write(M98090_REG_IO_CONFIGURATION,
					M98090_SDIEN_MASK);
	error |= maxim_codec_i2c_write(M98090_REG_DEVICE_SHUTDOWN,
					M98090_SHDNN_MASK);
	error |= maxim_codec_i2c_write(M98090_REG_OUTPUT_ENABLE,
							M98090_HPREN_MASK |
							M98090_HPLEN_MASK |
							M98090_SPREN_MASK |
							M98090_SPLEN_MASK |
							M98090_DAREN_MASK |
							M98090_DALEN_MASK);
	error |= maxim_codec_i2c_write(M98090_REG_IO_CONFIGURATION,
					M98090_SDOEN_MASK | M98090_SDIEN_MASK);

	if (error < 0)
		return -1;

	return 0;
}
