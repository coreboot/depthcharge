/*
 * rt5645.c -- RealTek ALC5645 ALSA SoC Audio driver
 *
 * Copyright 2015 Mediatek Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <libpayload.h>
#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/rt5645.h"

/* RT5645 has 256 8-bit register addresses, and 16-bit register data */
static int rt5645_i2c_readw(rt5645Codec *codec, uint8_t reg, uint16_t *data)
{
	return i2c_readw(codec->i2c, codec->chip, reg, data);
}

static int rt5645_i2c_writew(rt5645Codec *codec, uint8_t reg, uint16_t data)
{
	return i2c_writew(codec->i2c, codec->chip, reg, data);
}

static int rt5645_update_bits(rt5645Codec *codec, uint8_t reg,
			      uint16_t mask, uint16_t value)
{
	uint16_t old, new_value;

	if (rt5645_i2c_readw(codec, reg, &old))
		return 1;

	new_value = (old & ~mask) | (value & mask);

	if (old != new_value && rt5645_i2c_writew(codec, reg, new_value))
		return 1;

	return 0;
}

/* private registers are accessed by PRIV_INDEX and PRIV_DATA */
#ifdef DEBUG
static int rt5645_priv_readw(rt5645Codec *codec, uint8_t reg, uint16_t *data)
{
	if (i2c_writew(codec->i2c, codec->chip, RT5645_PRIV_INDEX, reg))
		return 1;
	return i2c_readw(codec->i2c, codec->chip, RT5645_PRIV_DATA, data);
}
#endif	//DEBUG

static int rt5645_priv_writew(rt5645Codec *codec, uint8_t reg, uint16_t data)
{
	if (i2c_writew(codec->i2c, codec->chip, RT5645_PRIV_INDEX, reg))
		return 1;
	return i2c_writew(codec->i2c, codec->chip, RT5645_PRIV_DATA, data);
}

#ifdef DEBUG
static void debug_dump_5645_regs(rt5645Codec *codec, int swap)
{
	uint16_t i, reg_word;

	// Show all 16-bit codec regs
	for (i = 0; i < RT5645_REG_CNT; i++) {
		if (i % 8 == 0)
				printf("\nMX%02X: ", i);

		rt5645_i2c_readw(codec, (uint8_t)i, &reg_word);
		if (swap)
			printf("%04X ", swap_bytes16(reg_word));
			else
			printf("%04X ", reg_word);
		}
	printf("\n");

	// Show all 16-bit 'private' codec regs
	for (i = 0; i < RT5645_PR_REG_CNT; i++) {
		if (i % 8 == 0)
			printf("\nPR%02X: ", i);

		rt5645_priv_readw(codec, (uint8_t)i, &reg_word);
		if (swap)
			printf("%04X ", swap_bytes16(reg_word));
		else
			printf("%04X ", reg_word);
	}
	printf("\n");
}
#endif	//DEBUG

static int rt5645_hw_params(rt5645Codec *codec)
{
	/* only support 16bits */
	if (rt5645_update_bits(codec, RT5645_I2S1_SDP,
			       RT5645_I2S_DL_MASK, 0)) {
		printf("%s: Error updating I2S1 Interface Ctrl reg!\n",
		       __func__);
		return 1;
	}

	return 0;
}

/* Sets rt5645 I2S format */
static int rt5645_set_fmt(rt5645Codec *codec)
{
	int ret = 0;

	/* Set format here: Assumes I2S, NB_NF, CBS_CFS */

	/* CBS_CFS (Codec Bit Slave/Codec Frame Slave) */
	ret = rt5645_update_bits(codec, RT5645_I2S1_SDP, RT5645_I2S_MS_MASK,
				 RT5645_I2S_MS_S);

	/* NB_NF (Normal Bit/Normal Frame) */
	ret |= rt5645_update_bits(codec, RT5645_I2S1_SDP, RT5645_I2S_BP_MASK,
				  RT5645_I2S_BP_NOR);

	/* I2S mode */
	ret |= rt5645_update_bits(codec, RT5645_I2S1_SDP, RT5645_I2S_DF_MASK,
				  RT5645_I2S_DF_I2S);

	if (ret) {
		printf("%s: Error updating I2S1 Interface Ctrl reg!\n",
		       __func__);
		return 1;
	}
	return 0;
}

/* Resets the audio codec */
static int rt5645_reset(rt5645Codec *codec)
{
	/* Reset the codec registers to their defaults */
	if (rt5645_i2c_writew(codec, RT5645_RESET, 0)) {
		printf("%s: Error resetting codec!\n", __func__);
		return 1;
	}

	mdelay(20);
	return 0;
}

/* Initialize rt5645 codec device */
int rt5645_device_init(rt5645Codec *codec)
{
	uint16_t reg;

	/* Reset the codec/regs */
	if (rt5645_reset(codec))
		return 1;

	if (rt5645_i2c_readw(codec, RT5645_VENDOR_ID2, &reg)) {
		printf("%s: Error reading vendor rev!\n", __func__);
		return 1;
	}

	/* init_list in kernel driver */
	rt5645_priv_writew(codec, RT5645_CHOP_DAC_ADC, 0x3600);
	rt5645_priv_writew(codec, RT5645_CLSD_INT_REG1, 0xFD20);
	rt5645_priv_writew(codec, 0x20, 0x611F);
	rt5645_priv_writew(codec, 0x21, 0x4040);
	rt5645_priv_writew(codec, 0x23, 0x0004);

	/* rt5650_init_list in kernel driver */
	if (reg == RT5650_DEVICE_ID)
		rt5645_i2c_writew(codec, 0xF6, 0x0100);

	/* rt5645_reg in kernel driver */
	rt5645_i2c_writew(codec, RT5645_AD_DA_MIXER, 0x8080);

	/* mixer control to enable SPK */
	rt5645_i2c_writew(codec, RT5645_SPK_L_MIXER, 0x003c);
	rt5645_i2c_writew(codec, RT5645_SPK_R_MIXER, 0x003c);
	rt5645_i2c_writew(codec, RT5645_SPK_VOL, 0x8888);
	rt5645_i2c_writew(codec, RT5645_SPO_MIXER, 0xD806);

	/* set pll and sysclk */
	rt5645_i2c_writew(codec, RT5645_PLL_CTRL1, 0x0F02);
	rt5645_i2c_writew(codec, RT5645_PLL_CTRL2, 0x0800);
	rt5645_i2c_writew(codec, RT5645_GLB_CLK, 0x4800);
	rt5645_i2c_writew(codec, RT5645_ADDA_CLK1, 0x1770);

	/* turn on */
	rt5645_i2c_writew(codec, RT5645_PWR_ANLG1, 0xA8D2);
	rt5645_i2c_writew(codec, RT5645_GEN_CTRL1, 0x3261);
	rt5645_i2c_writew(codec, RT5645_GEN_CTRL3, 0x0A00);
	rt5645_i2c_writew(codec, RT5645_PWR_ANLG2, 0x0200);
	rt5645_i2c_writew(codec, RT5645_PWR_DIG2, 0x0800);
	rt5645_i2c_writew(codec, RT5645_ASRC_1, 0x0800);
	rt5645_i2c_writew(codec, RT5645_PWR_MIXER, 0x3000);
	rt5645_i2c_writew(codec, RT5645_PWR_VOL, 0xC000);
	rt5645_i2c_writew(codec, RT5645_PWR_DIG1, 0x9B01);

	printf("%s completed, codec = %s\n", __func__,
	       (reg == RT5650_DEVICE_ID) ? "5650" : "5645");
	return 0;
}

static int rt5645_enable(SoundRouteComponentOps *me)
{
	int ret = 0;

	rt5645Codec *codec = container_of(me, rt5645Codec, component.ops);

	ret |= rt5645_device_init(codec) || rt5645_hw_params(codec) ||
	       rt5645_set_fmt(codec);

	/* unmute SPK */
	rt5645_update_bits(codec, RT5645_SPK_VOL,
			   RT5645_L_MUTE | RT5645_R_MUTE, 0);
#ifdef DEBUG
	/* Dump all regs after init is done */
	printf("%s: POST-INIT REGISTER DUMP!!!!!!!\n", __func__);
	debug_dump_5645_regs(codec, 0);

	printf("%s: exit w/ret code %d\n", __func__, ret);
#endif
	return ret;
}

rt5645Codec *new_rt5645_codec(I2cOps *i2c, uint8_t chip)
{
	printf("%s: chip = 0x%02X\n", __func__, chip);

	rt5645Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &rt5645_enable;

	codec->i2c = i2c;
	codec->chip = chip;

	return codec;
}
