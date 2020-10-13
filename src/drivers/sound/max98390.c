/*
 * max98390.c -- Maxim Integrated 98390
 *
 * Copyright 2020 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <libpayload.h>

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/max98390.h"

// MAX98390 uses 16-bit register addresses
typedef enum {
	MAX98390_SOFTWARE_RESET = 0x2000,
	MAX98390_CLK_MON = 0x2012,
	MAX98390_DAT_MON = 0x2014,
	MAX98390_PCM_RX_EN_A = 0x201b,
	MAX98390_PCM_TX_EN_A = 0x201d,
	MAX98390_PCM_TX_HIZ_CTRL_A = 0x201f,
	MAX98390_PCM_CH_SRC_1 = 0x2021,
	MAX98390_PCM_CH_SRC_2 = 0x2022,
	MAX98390_PCM_MODE_CFG = 0x2024,
	MAX98390_PCM_CLK_SETUP = 0x2026,
	MAX98390_R203A_AMP_EN = 0x203a,
	MAX98390_MEAS_EN = 0x203f,
	MAX98390_PWR_GATE_CTL =  0x2050,
	MAX98390_ENV_TRACK_VOUT_HEADROOM = 0x2076,
	MAX98390_BOOST_BYPASS1 = 0x207c,
	MAX98390_FET_SCALING3 = 0x2081,
	MAX98390_DSM_VOL_CTRL = 0x23ba,
	MAX98390_R23E1_DSP_GLOBAL_EN = 0x23e1,
	MAX98390_R23FF_GLOBAL_EN = 0x23FF,
} Max98390_Reg;

typedef struct {
	Max98390_Reg offset;
	uint8_t data;
} Max98390_RegDataMap;

static int max98390_write(Max98390Codec *codec, Max98390_Reg reg, uint8_t data)
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

static int max98390_reg_init(Max98390Codec *codec)
{
	int index;

	/* AMP on sequence for BIOS beep sound */
	static const Max98390_RegDataMap initList[] = {
		{MAX98390_CLK_MON, 0x6f},
		{MAX98390_PCM_RX_EN_A, 0x01},
		{MAX98390_PCM_TX_EN_A, 0x01},
		{MAX98390_PCM_TX_HIZ_CTRL_A, 0xfe},
		{MAX98390_PCM_CH_SRC_1, 0x00},
		{MAX98390_PCM_CH_SRC_2, 0x10},
		{MAX98390_PCM_MODE_CFG, 0x40},
		{MAX98390_PCM_CLK_SETUP, 0x33},
		{MAX98390_MEAS_EN, 0x03},
		{MAX98390_PWR_GATE_CTL, 0x2f},
		{MAX98390_ENV_TRACK_VOUT_HEADROOM, 0x0e},
		{MAX98390_BOOST_BYPASS1, 0x46},
		{MAX98390_FET_SCALING3, 0x03},
		{MAX98390_DSM_VOL_CTRL, 0x8a},
		{MAX98390_R23E1_DSP_GLOBAL_EN, 0x00},
		{MAX98390_R203A_AMP_EN, 0x81},
		{MAX98390_R23FF_GLOBAL_EN, 0x01},
	};

	for (index = 0; index < ARRAY_SIZE(initList); index ++) {
		if (max98390_write(codec, initList[index].offset, initList[index].data))
			return 1;
	}

	return 0;
}

static int max98390_enable(SoundRouteComponentOps *me)
{
	Max98390Codec *codec = container_of(me, Max98390Codec, component.ops);

	return max98390_reg_init(codec);
}

Max98390Codec *new_max98390_codec(I2cOps *i2c, uint8_t chip)
{
	Max98390Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &max98390_enable;

	codec->i2c = i2c;
	codec->chip = chip;

	return codec;
}
