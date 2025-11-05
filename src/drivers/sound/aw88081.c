// SPDX-License-Identifier: GPL-2.0-only
/*
 * aw88081.c -- AW88081 ALSA SoC Audio driver
 *
 * Copyright 2024 awinic Technology CO., LTD
 * Copyright 2025 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <assert.h>
#include <libpayload.h>
#include "drivers/bus/i2c/i2c.h"
#include "drivers/gpio/gpio.h"
#include "drivers/sound/aw88081.h"

struct reg_data {
	uint8_t reg;
	uint16_t data;
};

static const struct reg_data init_register_tab[] = {
	{0x03, 0x61ff},
	{0x04, 0x2345},
	{0x05, 0x8000},
	{0x06, 0x0c06},
	{0x07, 0x0010},
	{0x08, 0x0016},
	{0x09, 0x5f6a},
	{0x0a, 0x000f},
	{0x0b, 0x01e0},
	{0x0c, 0x1c1e},
	{0x0d, 0x00d8},
	{0x0e, 0x94b5},
	{0x11, 0x025f},
	{0x13, 0x1b00},
	{0x14, 0x2180},
	{0x15, 0x8398},
	{0x16, 0x02be},
	{0x17, 0x441a},
	{0x18, 0x4001},
	{0x19, 0x0000},
	{0x30, 0x460c},
	{0x31, 0xc989},
	{0x32, 0x3541},
	{0x33, 0x4db8},
	{0x34, 0xd6f9},
	{0x35, 0x7ace},
	{0x36, 0xec7c},
	{0x37, 0x000c},
	{0x38, 0x0400},
	{0x40, 0x2250},
	{0x41, 0x9d0b},
	{0x42, 0x1a60},
	{0x54, 0x2768},
	{0x55, 0x0202},
	{0x56, 0x3800},
	{0x57, 0x8c88},
	{0x58, 0x8909},
	{0x59, 0x28a4},
	{0x5a, 0x0000},
	{0x5b, 0x0000},
	{0x5c, 0x0140},
	{0x6f, 0x0002},
	{0x70, 0x0000},
	{0x6e, 0xdd21},
	{0x71, 0x0043},
	{0x6e, 0x0000},
};

static int aw88081_write(Aw88081Codec *codec, uint8_t reg, uint16_t data)
{
	uint8_t buf[] = {reg, data >> 8, data & 0xFF};
	I2cSeg seg = {0};

	seg.read = 0;
	seg.chip = codec->chip;
	seg.buf = buf;
	seg.len = ARRAY_SIZE(buf);

	return codec->i2c->transfer(codec->i2c, &seg, 1);
}

static int aw88081_read(Aw88081Codec *codec, uint8_t reg, uint16_t *data)
{
	uint8_t bytes[2] = {0};
	I2cSeg seg[2] = {0};
	int ret;

	seg[0].read = 0;
	seg[0].chip = codec->chip;
	seg[0].buf = &reg;
	seg[0].len = sizeof(reg);

	seg[1].read = 1;
	seg[1].chip = codec->chip;
	seg[1].buf = &bytes[0];
	seg[1].len = sizeof(bytes);

	ret = codec->i2c->transfer(codec->i2c, seg, ARRAY_SIZE(seg));
	/* dealing endian issue */
	*data = (uint16_t)(bytes[0] << 8) | bytes[1];

	return ret;
}

static int aw88081_write_bits(Aw88081Codec *codec,  uint16_t reg,
			      uint16_t mask, uint16_t value)
{
	uint16_t old;
	uint16_t new;

	if (aw88081_read(codec, reg, &old))
		return 1;

	assert((value & mask) == value);
	new = (old & ~mask) | value;

	if (aw88081_write(codec, reg, new))
		return 1;

	return 0;
}

static int aw88081_reg_update(Aw88081Codec *codec)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(init_register_tab); i++) {
		if (aw88081_write(codec, init_register_tab[i].reg, init_register_tab[i].data))
			return 1;
	}

	return 0;
}

static int aw88081_soft_reset(Aw88081Codec *codec)
{
	if (aw88081_write(codec, AW88081_SOFT_RESET_REG, AW88081_SOFT_RESET_VALUE))
		return 1;

	return 0;
}

static int aw88081_dev_pwd(Aw88081Codec *codec, bool pwd)
{
	if (aw88081_write_bits(codec, AW88081_SYSCTRL_REG, AW88081_PWDN_MASK,
			       pwd ? AW88081_PWDN_WORKING : AW88081_PWDN_POWER_DOWN))
		return 1;

	return 0;
}

static int aw88081_dev_amppd(Aw88081Codec *codec, bool amppd)
{
	if (aw88081_write_bits(codec, AW88081_SYSCTRL_REG, AW88081_AMPPD_MASK,
			       amppd ? AW88081_AMPPD_WORKING : AW88081_AMPPD_POWER_DOWN))
		return 1;

	return 0;
}

static int aw88081_dev_mute(Aw88081Codec *codec, bool mute)
{
	if (aw88081_write_bits(codec, AW88081_SYSCTRL_REG, AW88081_HMUTE_MASK,
			       mute ? AW88081_HMUTE_ENABLE : AW88081_HMUTE_DISABLE))
		return 1;

	return 0;
}

static int aw88081_uls_hmute(Aw88081Codec *codec, bool mute)
{
	if (aw88081_write_bits(codec, AW88081_SYSCTRL_REG, AW88081_ULS_HMUTE_MASK,
			       mute ? AW88081_ULS_HMUTE_ENABLE : AW88081_ULS_HMUTE_DISABLE))
		return 1;

	return 0;
}

static int aw88081_tx_enable(Aw88081Codec *codec, bool txen)
{
	if (aw88081_write_bits(codec, AW88081_I2SCTRL3_REG, AW88081_I2STXEN_MASK,
			       txen ? AW88081_I2STXEN_ENABLE : AW88081_I2STXEN_DISABLE))
		return 1;

	return 0;
}

static int aw88081_device_start(Aw88081Codec *codec)
{
	/* pwdn on */
	if (aw88081_dev_pwd(codec, true))
		return 1;
	mdelay(2);

	/* amppd on */
	if (aw88081_dev_amppd(codec, true))
		return 1;
	mdelay(1);

	if (aw88081_tx_enable(codec, true))
		return 1;

	if (aw88081_uls_hmute(codec, false))
		return 1;
	if (aw88081_dev_mute(codec, false))
		return 1;

	return 0;
}

static int aw88081_device_stop(Aw88081Codec *codec)
{
	if (aw88081_uls_hmute(codec, true))
		return 1;
	if (aw88081_dev_mute(codec, true))
		return 1;

	if (aw88081_tx_enable(codec, false))
		return 1;
	mdelay(5);

	/* amppd off */
	if (aw88081_dev_amppd(codec, false))
		return 1;

	/* pwdn off  */
	if (aw88081_dev_pwd(codec, false))
		return 1;

	return 0;
}

static int aw88081_init(Aw88081Codec *codec)
{
	if (aw88081_soft_reset(codec))
		return 1;

	if (aw88081_reg_update(codec))
		return 1;

	if (aw88081_device_stop(codec))
		return 1;

	return 0;
}

static int aw88081_enable(SoundRouteComponentOps *me)
{
	Aw88081Codec *codec = container_of(me, Aw88081Codec, component.ops);

	if (aw88081_init(codec))
		return 1;

	if (aw88081_device_start(codec))
		return 1;

	return 0;
}

static int aw88081_disable(SoundRouteComponentOps *me)
{
	Aw88081Codec *codec = container_of(me, Aw88081Codec, component.ops);

	if (aw88081_device_stop(codec))
		return 1;

	return 0;
}

Aw88081Codec *new_aw88081_codec(I2cOps *i2c, uint8_t chip)
{
	Aw88081Codec *codec = xzalloc(sizeof(Aw88081Codec));

	codec->component.ops.enable = &aw88081_enable;
	codec->component.ops.disable = &aw88081_disable;
	codec->i2c = i2c;
	codec->chip = chip;
	return codec;
}
