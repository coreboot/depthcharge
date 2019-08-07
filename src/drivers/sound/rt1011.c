/*
 * rt1011.c -- RealTek ALC1011 ALSA SoC Audio driver
 *
 * Copyright 2017 Realtek Semiconductor Corp. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "base/list.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/rt1011.h"

struct reg_config {
	uint16_t addr;
	uint16_t data;
};

static const struct reg_config config[] = {
	{RT1011_CLOCK2,			RT1011_FS_RC_CLK		},
	{RT1011_PLL1,			RT1011_BYPASS_MODE		},
	{RT1011_TDM,			RT1011_SLAVE_24BIT_I2S		},
	{RT1011_TDM_TCON,		RT1011_BCLK_64FS_LRCLK_DIV_256	},
	{RT1011_TDM1,			RT1011_LOOPBACK_STEREO		},
	{RT1011_POWER9,			RT1011_SDB_MODE			},
	{RT1011_ADC,			RT1011_ADC_SRC_CTRL		},
	{RT1011_MIXER,			RT1011_UNMUTE			},
	{RT1011_ADC1,			RT1011_ADC_DIGITAL_VOL		},
	{RT1011_SPK_DC_DETECT1,		RT1011_DC_DETECT1_ON		},
	{RT1011_SPK_DC_DETECT2,		RT1011_DC_DETECT_THRESHOLD	},
	{RT1011_POWER1,			RT1011_POW_LDO2_DAC_CLK12M	},
	{RT1011_POWER2,			RT1011_POW_BG_MBIAS_LV		},
	{RT1011_POWER3,			RT1011_POW_VBAT_MBIAS_LV	},
	{RT1011_DAC2,			RT1011_EN_DAC_CHOPPER		},
	{RT1011_RW1,			RT1011_CLK_BOOST_OFF		},
	{RT1011_ANALOG_TC_SEQ1,		RT1011_SEL_BSTCAP_FREQ_192KHZ	},
	{RT1011_POWER7,			RT1011_BOOST_SWR_CTRL		},
	{RT1011_POWER8,			RT1011_BOOST_SWR_MOS_CTRL	},
	{RT1011_BOOST_CTRL1,		RT1011_FORCE_BYPASS_BOOST_VOLT	},
	{RT1011_POWER4,			RT1011_EN_SWR			},
	{RT1011_SPK_DRIVER_STATUS,	RT1011_SPK_DRIVER_READY		},
	{RT1011_SINE_GEN_REG1,		RT1011_SINE_GEN_CTRL_EN		},
	{RT1011_SINE_GEN_REG2,		RT1011_SINE_GEN_FREQ_VOL_SEL	},
	{RT1011_SINE_GEN_REG3,		RT1011_SINE_GEN_MANUAL_INDEX	},
	{RT1011_CLK_DETECT,		RT1011_DISABLE_BCLK_DETECT	}
};

/* RT1011 has 16-bit register addresses, and 16-bit register data */
#ifdef DEBUG
static int rt1011_i2c_readw(rt1011Codec *codec, uint16_t reg, uint16_t *data)
{
	if (i2c_addrw_readw(codec->i2c, codec->chip, reg, data)) {
		printf("rt1011 i2c reg:%x read err\n", reg);
		return 1;
	}
	return 0;
}
#endif

static int rt1011_i2c_writew(rt1011Codec *codec, uint16_t reg, uint16_t data)
{
	if (i2c_addrw_writew(codec->i2c, codec->chip, reg, data)) {
		printf("rt1011 i2c reg:%x write err\n", reg);
		return 1;
	}
	return 0;
}

/* Resets the audio codec */
static int rt1011_reset(rt1011Codec *codec)
{
	/* Reset the codec registers to their defaults */
	if (rt1011_i2c_writew(codec, RT1011_RESET, RT1011_RST)) {
		printf("%s: Error resetting codec!\n", __func__);
		return 1;
	}
	return 0;
}

/* Initialize rt1011 codec device */
static int rt1011_device_init(rt1011Codec *codec)
{
	/* codec reset */
	if (rt1011_reset(codec))
		return 1;

	/* Regs config for internal tone generation */
	for (int i = 0; i < ARRAY_SIZE(config); i++) {
		if (rt1011_i2c_writew(codec, config[i].addr, config[i].data))
			return 1;
	}
	return 0;
}

static int rt1011_play_boot_beep(SoundOps *me, uint32_t msec,
				uint32_t frequency)
{
	uint16_t reg_data = RT1011_BEEP_TONE_HIGH;
	uint8_t pulse_cnt = 4;

	/* ignore frequency param since rt1011 generates internal tone */

	/* rt1011 does not require square wave generation with frequency */

	rt1011Codec *codec = container_of(me, rt1011Codec, ops);
	/* Internal Tone generation */
	for (uint8_t i = 0; i < pulse_cnt; i++) {
		if (rt1011_i2c_writew(codec, RT1011_BEEP_TONE, reg_data))
			return 1;

		/* Alter data between High and Low */
		if (reg_data == RT1011_BEEP_TONE_HIGH)
			reg_data = RT1011_BEEP_TONE_LOW;
		else
			reg_data = RT1011_BEEP_TONE_HIGH;

		mdelay(msec);
	}
	return 0;
}

static int rt1011_enable(SoundRouteComponentOps *me)
{
	rt1011Codec *codec = container_of(me, rt1011Codec, component.ops);

	return rt1011_device_init(codec);
}

static int rt1011_disable(SoundRouteComponentOps *me)
{
	rt1011Codec *codec = container_of(me, rt1011Codec, component.ops);

	return rt1011_reset(codec);
}

rt1011Codec *new_rt1011_codec(I2cOps *i2c, uint8_t chip)
{
	rt1011Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &rt1011_enable;
	codec->component.ops.disable = &rt1011_disable;

	codec->ops.play = &rt1011_play_boot_beep;
	codec->i2c = i2c;
	codec->chip = chip;

	return codec;
}
