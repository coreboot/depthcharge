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

static const struct reg_config config[] = {
	/* No Boost mode */
	{ RT1015_PWR4,			0x00B2 },
	{ RT1015_CLSD_INTERNAL8,	0x2008 },
	{ RT1015_CLSD_INTERNAL9,	0x0140 },
	{ RT1015_GAT_BOOST,		0x00FE },
	{ RT1015_PWR_STATE_CTRL,	0x000E },
	/* M/N for PLL_0 */
	{ RT1015_PLL1,			0x10BE },
	{ RT1015_TDM_MASTER,		0x0000 },
};

static int rt1015_enable(SoundRouteComponentOps *me)
{
	rt1015Codec *codec = container_of(me, rt1015Codec, component.ops);
	uint16_t val;

	if (i2c_addrw_readw(codec->i2c, codec->chip, RT1015_DEVICE_ID, &val)) {
		printf("%s: Error reading reg 0x%02x!\n", __func__, RT1015_DEVICE_ID);
		return 1;
	}
	if ((val != RT1015_DEVICE_ID_VAL) && (val != RT1015_DEVICE_ID_VAL2)) {
		printf("%s: Error reading device id!\n", __func__);
		return 1;
	}

	for (int i = 0; i < ARRAY_SIZE(config); i++) {
		if (i2c_addrw_writew(codec->i2c, codec->chip,
				     config[i].addr, config[i].data))
			return 1;
	}

	return 0;
}

static int rt1015_disable(SoundRouteComponentOps *me)
{
	rt1015Codec *codec = container_of(me, rt1015Codec, component.ops);

	/* Reset and put power state to shut-down mode */
	if (i2c_addrw_writew(codec->i2c, codec->chip, RT1015_RESET, 0)) {
		printf("%s: Error resetting codec!\n", __func__);
		return 1;
	}

	return 0;
}

rt1015Codec *new_rt1015_codec(I2cOps *i2c, uint8_t chip)
{
	rt1015Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &rt1015_enable;
	codec->component.ops.disable = &rt1015_disable;

	codec->i2c = i2c;
	codec->chip = chip;

	return codec;
}
