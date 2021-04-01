// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * RT1015 ALSA SoC audio amplifier driver
 *
 * Copyright 2020 Intel Corporation.
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/sound/rt1015.h"

struct reg_config {
	uint16_t addr;
	uint16_t data;
};

static const struct reg_config config_no_boost[] = {
	/* No Boost mode */
	{ RT1015_PWR4,			0x00B2 },
	{ RT1015_CLSD_INTERNAL8,	0x2008 },
	{ RT1015_CLSD_INTERNAL9,	0x0140 },
	{ RT1015_GAT_BOOST,		0x0EFE },
	{ RT1015_PWR_STATE_CTRL,	0x000E },
	/* M/N for PLL_0 */
	{ RT1015_PLL1,			0x10BE },
	/* Disable Zero detection */
	{ RT1015_DAC2,                  0x2000 },
	{ RT1015_TDM_MASTER,		0x0000 },
};

static const struct reg_config config_boost[] = {
	/* Boost mode 48K, 50fs, BCLK=2.4M */
	{ RT1015_RESET, 0x0000 },
	{ RT1015_PLL1,  0x30FE },
	{ RT1015_PLL2,  0x0008 },
	{ RT1015_DAC2,  0x2000 },
};

static int rt1015_enable(SoundRouteComponentOps *me)
{
	rt1015Codec *codec = container_of(me, rt1015Codec, component.ops);
	uint16_t val;

	const struct reg_config *config;
	size_t config_size;

	if (codec->boost) {
		config = config_boost;
		config_size = ARRAY_SIZE(config_boost);
	} else {
		config = config_no_boost;
		config_size = ARRAY_SIZE(config_no_boost);
	}

	if (i2c_addrw_readw(codec->i2c, codec->chip, RT1015_DEVICE_ID, &val)) {
		printf("%s: Error reading reg 0x%02x!\n", __func__, RT1015_DEVICE_ID);
		return 1;
	}
	if ((val != RT1015_DEVICE_ID_VAL) && (val != RT1015_DEVICE_ID_VAL2)) {
		printf("%s: Error reading device id!\n", __func__);
		return 1;
	}

	for (int i = 0; i < config_size; i++) {
		if (i2c_addrw_writew(codec->i2c, codec->chip,
				     config[i].addr, config[i].data))
			return 1;
	}

	return 0;
}

static int rt1015_disable(SoundRouteComponentOps *me)
{
	rt1015Codec *codec = container_of(me, rt1015Codec, component.ops);

	mdelay(10);
	/* Reset and put power state to shut-down mode */
	if (i2c_addrw_writew(codec->i2c, codec->chip, RT1015_RESET, 0)) {
		printf("%s: Error resetting codec!\n", __func__);
		return 1;
	}

	return 0;
}

rt1015Codec *new_rt1015_codec(I2cOps *i2c, uint8_t chip, uint8_t boost_mode)
{
	rt1015Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &rt1015_enable;
	codec->component.ops.disable = &rt1015_disable;

	codec->i2c = i2c;
	codec->chip = chip;
	codec->boost = boost_mode;

	return codec;
}
