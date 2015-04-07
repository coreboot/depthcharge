/*
 * rt5677.c -- RealTek ALC5677 ALSA SoC Audio driver
 *
 * Copyright 2014 Google Inc.  All rights reserved.
 * Copyright 2014 Nvidia Corp.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <libpayload.h>
#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/rt5677.h"

/* RT5677 has 256 8-bit register addresses, and 16-bit register data */
struct rt5677_init_reg {
	uint8_t reg;
	uint16_t val;
};

static struct rt5677_init_reg init_list[] = {
	{RT5677_LOUT1,		  0x0800},
	{RT5677_SIDETONE_CTRL,	  0x0000},
	{RT5677_STO1_ADC_DIG_VOL, 0x7F7F},
	{RT5677_STO2_ADC_MIXER,	  0xA441},
	{RT5677_STO1_ADC_MIXER,	  0x5480},
	{RT5677_STO1_DAC_MIXER,	  0x8A8A},
	{RT5677_PWR_DIG1,	  0xD800},
	{RT5677_PWR_ANLG1,	  0xE9D5},
	{RT5677_PWR_ANLG2,	  0x2CC0},
	{RT5677_PWR_DSP2,	  0x0C00},
	{RT5677_I2S2_SDP,	  0x0000},
	{RT5677_CLK_TREE_CTRL1,	  0x0111},
	{RT5677_PLL1_CTRL1,	  0x0000},
	{RT5677_PLL1_CTRL2,	  0x0000},
	{RT5677_GPIO_CTRL2,       0x61B0},      /* GPIO5 output, high for SPKR_PA_EN on Smaug */
	{RT5677_DIG_MISC,	  0x0029},
	{RT5677_GEN_CTRL1,	  0x00FF},
	{RT5677_PWR_DIG2,	  0x9024},
	{RT5677_PDM_OUT_CTRL,	  0x0088},
	{RT5677_PRIV_INDEX,	  0x003D},
	{RT5677_PRIV_DATA,	  0x364D},
	{RT5677_DAC1_DIG_VOL,	  0x9090},
};
#define RT5677_INIT_REG_LEN ARRAY_SIZE(init_list)

static int rt5677_i2c_readw(rt5677Codec *codec, uint8_t reg, uint16_t *data)
{
	return i2c_readw(codec->i2c, codec->chip, reg, data);
}

static int rt5677_i2c_writew(rt5677Codec *codec, uint8_t reg, uint16_t data)
{
	return i2c_writew(codec->i2c, codec->chip, reg, data);
}

static int rt5677_update_bits(rt5677Codec *codec, uint8_t reg,
				uint16_t mask, uint16_t value)
{
	uint16_t old, new_value;

	if (rt5677_i2c_readw(codec, reg, &old))
		return 1;

	new_value = (old & ~mask) | (value & mask);

	if (old != new_value && rt5677_i2c_writew(codec, reg, new_value))
		return 1;

	return 0;
}

/* Initialize codec regs w/static/base values */
static void rt5677_reg_init(rt5677Codec *codec)
{
	int i;

	for (i = 0; i < RT5677_INIT_REG_LEN; i++)
		rt5677_i2c_writew(codec, init_list[i].reg, init_list[i].val);
}


#ifdef DEBUG
static void debug_dump_5677_regs(rt5677Codec *codec, int swap)
{
	uint16_t i, reg_word;

	// Show all 16-bit codec regs
	for (i = 0; i < RT5677_REG_CNT; i++) {
		if (i % 8 == 0)
			printf("\nMX%02X: ", i);

		rt5677_i2c_readw(codec, (uint8_t)i, &reg_word);
		if (swap)
			printf("%04X ", swap_bytes16(reg_word));
		else
			printf("%04X ", reg_word);
	}
	printf("\n");

	// Show all 16-bit 'private' codec regs
	for (i = 0; i < RT5677_PR_REG_CNT; i++) {
		if (i % 8 == 0)
			printf("\nPR%02X: ", i);

		rt5677_i2c_writew(codec, RT5677_PRIV_INDEX, i);
		rt5677_i2c_readw(codec, RT5677_PRIV_DATA, &reg_word);
		if (swap)
			printf("%04X ", swap_bytes16(reg_word));
		else
			printf("%04X ", reg_word);
	}
	printf("\n");
}
#endif	//DEBUG

static int rt5677_hw_params(rt5677Codec *codec)
{
	unsigned int bits_per_sample = codec->bits_per_sample;

	switch (bits_per_sample) {
	case 16:
		if (rt5677_update_bits(codec, RT5677_I2S1_SDP,
				       RT5677_I2S_DL_MASK, 0)) {
			printf("%s: Error updating I2S1 Interface Ctrl reg!\n",
			       __func__);
			return 1;
		}
		break;
	default:
		printf("%s: Illegal bits per sample %d.\n", __func__,
			bits_per_sample);
		return 1;
	}

	return 0;
}

/* Sets rt5677 I2S format */
static int rt5677_set_fmt(rt5677Codec *codec)
{
	int ret = 0;

	/* Set format here: Assumes I2S, NB_NF, CBS_CFS */

	/* CBS_CFS (Codec Bit Slave/Codec Frame Slave) */
	ret = rt5677_update_bits(codec, RT5677_I2S1_SDP, RT5677_I2S_MS_MASK,
	      RT5677_I2S_MS_S);

	/* NB_NF (Normal Bit/Normal Frame) */
	ret |= rt5677_update_bits(codec, RT5677_I2S1_SDP, RT5677_I2S_BP_MASK,
	       RT5677_I2S_BP_NOR);

	/* I2S mode */
	ret |= rt5677_update_bits(codec, RT5677_I2S1_SDP, RT5677_I2S_DF_MASK,
	       RT5677_I2S_DF_I2S);

	/* A44: I2S2 (going to speaker amp) is master */
	ret = rt5677_update_bits(codec, RT5677_I2S2_SDP, RT5677_I2S_MS_MASK,
	      RT5677_I2S_MS_M);

	if (ret) {
		printf("%s: Error updating I2S1 Interface Ctrl reg!\n",
		       __func__);
		return 1;
	}
	return 0;
}

/* Resets the audio codec */
static int rt5677_reset(rt5677Codec *codec)
{
	/* Reset the codec registers to their defaults */
	if (rt5677_i2c_writew(codec, RT5677_RESET, RT5677_SW_RESET)) {
		printf("%s: Error resetting codec!\n", __func__);
		return 1;
	}

	mdelay(20);
	return 0;
}

/* Initialize rt5677 codec device */
int rt5677_device_init(rt5677Codec *codec)
{
	uint16_t id, reg;

	/* Read status reg */
	rt5677_i2c_readw(codec, RT5677_RESET, &reg);
	printf("%s: reg 00h, Software Reset & Status = 0x%04X\n", __func__,
	       reg);

	/* Reset the codec/regs */
	if (rt5677_reset(codec)) {
		return 1;
	}

	if (rt5677_i2c_readw(codec, RT5677_VENDOR_ID1, &id)) {
		printf("%s: Error reading vendor ID!\n", __func__);
		return 1;
	}
	printf("%s: Hardware ID: %04X\n", __func__, id);

	if (rt5677_i2c_readw(codec, RT5677_VENDOR_ID2, &reg)) {
		printf("%s: Error reading vendor rev!\n", __func__);
		return 1;
	}
	printf("%s: Hardware revision: %04X\n", __func__, reg);

	/* Initialize codec regs w/static/base values, same as Linux driver */
	rt5677_reg_init(codec);

	return 0;
}

static int rt5677_enable(SoundRouteComponentOps *me)
{
	int ret = 0;

	rt5677Codec *codec = container_of(me, rt5677Codec, component.ops);

	ret |= rt5677_device_init(codec) || rt5677_hw_params(codec)
	    || rt5677_set_fmt(codec);

#ifdef DEBUG
	/* Dump all regs after init is done */
	printf("%s: POST-INIT REGISTER DUMP!!!!!!!\n", __func__);
	debug_dump_5677_regs(codec, 0);

	printf("%s: exit w/ret code %d\n", __func__, ret);
#endif
	return ret;
}

rt5677Codec *new_rt5677_codec(I2cOps *i2c, uint8_t chip,
				  int bits_per_sample, int sample_rate,
				  int lr_frame_size, uint8_t master_clock)
{
	printf("%s: chip = 0x%02X, bits_per_sample = %d, sample_rate = %d\n",
		 __func__, chip, bits_per_sample, sample_rate);
	printf("  lr_frame_size = %d, master_clock = %d\n",
	       lr_frame_size, master_clock);

	rt5677Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &rt5677_enable;

	codec->i2c = i2c;
	codec->chip = chip;

	codec->bits_per_sample = bits_per_sample;
	codec->sample_rate = sample_rate;
	codec->lr_frame_size = lr_frame_size;

	codec->master_clock = master_clock;

	return codec;
}
