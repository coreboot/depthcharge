/*
 * cs35l53.h -- Cirrus CS35L53 Audio driver
 *
 * Copyright 2021 American Megatrends Corp. All rights reserved.
 * Author: Eddy Lu <eddylu@ami.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <libpayload.h>
#include "base/container_of.h"
#include "base/list.h"
#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/cs35l53.h"

/* I2S controller settings for Cs35l53 codec */
const I2sSettings cs35l53_settings = {
	/* To set MOD bit in SSC0 - defining as network/normal mode */
	.mode = SSP_IN_NETWORK_MODE,
	/* To set FRDC bit in SSC0 - timeslot per frame in network mode */
	.frame_rate_divider_ctrl = FRAME_RATE_CONTROL_STEREO,
	.fsync_rate = 48000,
	.bclk_rate = 3072000,
	/* To set TTSA bit n SSTSA - data transmit timeslot */
	.ssp_active_tx_slots_map = 3,
	/* To set RTSA bit n SSRSA - data receive timeslot */
	.ssp_active_rx_slots_map = 3,
};

typedef struct reg_config {
	uint32_t addr;
	uint32_t data;
	uint32_t read_check;
	int	 delay;
} reg_config_t;

reg_config_t config[] = {
	{CS35L53_RST, CS35L53_SFT_RST, 0x00000000, 0 },
	{CS35L53_REFCLK_INPUT, CS35L53_PLL_REFCLK_FREQ, 0x00000000, 0 },
	{CS35L53_REFCLK_INPUT, CS35L53_PLL_REFCLK_FREQ|CS35L53_PLL_REFCLK_EN, 0x00000000, 0 },
	{CS35L53_GLOBAL_SAMPLE_RATE, CS35L53_GLOBAL_FS, 0x00000000, 0 },
	{CS35L53_ASP_ENABLES1, CS35L53_ASP_RX1_EN, 0x00000000, 0 },
	{CS35L53_ASP_CONTROL1, CS35L53_ASP_BCLK_FREQ, 0x00000000, 0 },
	{CS35L53_ASP_CONTROL2, CS35L53_ASP_TX_RX_WIDTH1, 0x00000000, 0 },
	{CS35L53_ASP_FRAME_CONTROL5, CS35L53_ASP_RX1_SLOT, 0x00000000, 0 },
	{CS35L53_ASP_DATA_CONTROL5, CS35L53_ASP_RX_WL, 0x00000000, 0 },
	{CS35L53_ASP_DACPCM1_INPUT, CS35L53_ASP_DACPCM1_SRC_ASPRX1, 0x00000000, 0 },
	{CS35L53_AMP_CTRL, CS35L53_AMP_VOL_PCM, 0x00000000, 0 },
	{CS35L53_AMP_GAIN, CS35L53_AMP_GAIN_PCM_PDM, 0x00000000, 0 },
	{CS35L53_BLOCK_ENABLES, CS35L53_AMP_EN, 0x00000000, 0 }
};

/* play beep */
reg_config_t play[] = {
	{CS35L53_TEST_KEY_CTRL, CS35L53_TEST_KEY_ENABLE_1, 0x00000000, 0 },
	{CS35L53_TEST_KEY_CTRL, CS35L53_TEST_KEY_ENABLE_2, 0x00000000, 0 },
	{CS35L53_SPK_MSM_TST_3, CS35L53_SPK_MSM_TST_3_ENABLE, 0x00000000, 0 },
	{CS35L53_TST_SPK_2, CS35L53_TST_SPK_2_ENABLE, 0x00000000, 0 },
	{CS35L53_SPK_FORCE_TST_2, CS35L53_SPK_FORCE_TST_2_ENABLE_1, 0x00000000, 0 },
	{CS35L53_GPIO_PAD_CTRL, CS35L53_GP1_CTRL_GPIO, 0x00000000, 0 },
	{CS35L53_GPIO1_CTRL1, CS35L53_GP1_LVL_H, 0x00000000, 0 },
	{CS35L53_SPK_FORCE_TST_2, CS35L53_SPK_FORCE_TST_2_SET_1, 0x00000000, 0 },
	{CS35L53_SPK_FORCE_TST_2, CS35L53_SPK_FORCE_TST_2_SET_2, 0x00000000, 0 },
	{CS35L53_SPK_MSM_TST_3, CS35L53_SPK_MSM_TST_3_ENABLE, 0x00000000, 0 },
	{CS35L53_GLOBAL_ENABLES, CS35L53_GLOBAL_EN, 0x00000000, 0 },
	{CS35L53_IRQ1_STS_1, 0x00000000, CS35L53_MSM_PUP_DONE_STS1, 0 },
	{CS35L53_SPK_FORCE_TST_2, CS35L53_SPK_FORCE_TST_2_ENABLE_2, 0x00000000, 0 },
	{CS35L53_SPK_MSM_TST_3, CS35L53_SPK_MSM_TST_3_DISABLE, 0x00000000, 0 },
	{CS35L53_TEST_KEY_CTRL, CS35L53_TEST_KEY_DISABLE_1, 0x00000000, 0 },
	{CS35L53_TEST_KEY_CTRL, CS35L53_TEST_KEY_DISABLE_2, 0x00000000, 0 }
};

/* stop beep */
reg_config_t stop[] = {
	{CS35L53_TEST_KEY_CTRL, CS35L53_TEST_KEY_ENABLE_1, 0x00000000, 0 },
	{CS35L53_TEST_KEY_CTRL, CS35L53_TEST_KEY_ENABLE_2, 0x00000000, 0 },
	{CS35L53_SPK_MSM_TST_3, CS35L53_SPK_MSM_TST_3_ENABLE, 0x00000000, 0 },
	{CS35L53_GLOBAL_ENABLES, CS35L53_GLOBAL_DIS, 0x00000000, 0 },
	{CS35L53_IRQ1_STS_1, 0x00000000, CS35L53_MSM_PDN_DONE_STS1, 0 },
	{CS35L53_SPK_FORCE_TST_2, CS35L53_SPK_FORCE_TST_2_ENABLE_1, 0x00000000, 0 },
	{CS35L53_SPK_MSM_TST_3, CS35L53_SPK_MSM_TST_3_DISABLE, 0x00000000, 0 },
	{CS35L53_GPIO1_CTRL1, CS35L53_GP1_LVL_L, 0x00000000, 0 },
	{CS35L53_GPIO_PAD_CTRL, CS35L53_GP1_CTRL_HI, 0x00000000, 0 },
	{CS35L53_BOOST_TEST_MANUAL, CS35L53_BOOST_TEST_MANUAL_ENABLE, 0x00000000, 5 },
	{CS35L53_BOOST_TEST_MANUAL, CS35L53_BOOST_TEST_MANUAL_DISABLE, CS35L53_BOOST_TEST_MANUAL_ENABLE, 0 },
	{CS35L53_TST_SPK_2, CS35L53_TST_SPK_2_DISABLE, 0x00000000, 0 },
	{CS35L53_SPK_FORCE_TST_2, CS35L53_SPK_FORCE_TST_2_DISABLE, 0x00000000, 0 },
	{CS35L53_TEST_KEY_CTRL, CS35L53_TEST_KEY_DISABLE_1, 0x00000000, 0 },
	{CS35L53_TEST_KEY_CTRL, CS35L53_TEST_KEY_DISABLE_2, 0x00000000, 0 }
};

static int cs35l53_i2c_readdw(cs35l53Codec *codec, uint32_t reg, uint32_t *data)
{
	if (i2c_addrl_readl(codec->i2c, codec->chip, reg, data)) {
		printf("cs35l53 i2c reg:%x read err\n", reg);
		return -1;
	}
	return 0;
}

static int cs35l53_i2c_writedw(cs35l53Codec *codec, uint32_t reg, uint32_t data)
{
	if (i2c_addrl_writel(codec->i2c, codec->chip, reg, data)) {
		printf("cs35l53 i2c reg:%x write err\n", reg);
		return -1;
	}
	return 0;
}

/* Righ ch:0 Left ch:1 fail:-1 */
static int cs35l53_ch_check(cs35l53Codec *codec)
{
	switch (codec->chip) {
		case AUD_CS35L53_DEVICE_ADDR1:
			__attribute__((fallthrough));
		case AUD_CS35L53_DEVICE_ADDR2:
			return 0;
		case AUD_CS35L53_DEVICE_ADDR3:
			__attribute__((fallthrough));
		case AUD_CS35L53_DEVICE_ADDR4:
			return 1;
		default:
			return -1;
	}
}

/* Initialize cs35l53 codec device */
static int cs35l53_device_init(cs35l53Codec *codec)
{
	uint32_t check;
	int tries = 5;
	int ch;

	ch = cs35l53_ch_check(codec);

	/* If Right ch(addr: 0x40 0x41) set [0x00004820, 0x00000001] */
	if (ch == 0)
		config[7].data = 0x00000001;
	else if (ch == 1)
		config[7].data = 0x00000000;
	else
		return 1;

	for (int i = 0; i < ARRAY_SIZE(config); i++) {
		if (config[i].read_check && !cs35l53_i2c_readdw(codec, config[i].addr, &check)) {
			while ((!(check & config[i].read_check)) && (tries--)) {
				mdelay(10);
				cs35l53_i2c_readdw(codec, config[i].addr, &check);
			}
		} else if (cs35l53_i2c_writedw(codec, config[i].addr, config[i].data) == -1)
			return 1;

		if (config[i].delay)
			mdelay(config[i].delay);
	}

	return 0;
}

static int cs35l53_play_boot_beep(SoundRouteComponentOps *me)
{
	uint32_t check;
	int tries = 5;
	cs35l53Codec *codec = container_of(me, cs35l53Codec, component.ops);

	for (int i = 0; i < ARRAY_SIZE(play); i++) {
		if (play[i].read_check && !cs35l53_i2c_readdw(codec, play[i].addr, &check)) {
			while ((!(check & play[i].read_check)) && (tries--)) {
				mdelay(10);
				cs35l53_i2c_readdw(codec, play[i].addr, &check);
			}
		} else if (cs35l53_i2c_writedw(codec, play[i].addr, play[i].data) == -1)
			return 1;

		if (play[i].delay)
			mdelay(play[i].delay);
	}

	return 0;
}

static int cs35l53_enable(SoundRouteComponentOps *me)
{
	cs35l53Codec *codec = container_of(me, cs35l53Codec, component.ops);
	cs35l53_device_init(codec);

	return 0;
}

static int cs35l53_disable(SoundRouteComponentOps *me)
{
	uint32_t check;
	int tries = 5;
	cs35l53Codec *codec = container_of(me, cs35l53Codec, component.ops);

	for (int i = 0; i < ARRAY_SIZE(stop); i++) {
		if (stop[i].read_check && !cs35l53_i2c_readdw(codec, stop[i].addr, &check)) {
			while ((!(check & stop[i].read_check)) && (tries--)) {
				mdelay(10);
				cs35l53_i2c_readdw(codec, stop[i].addr, &check);
			}
		} else if (cs35l53_i2c_writedw(codec, stop[i].addr, stop[i].data) == -1)
			return 1;

		if (stop[i].delay)
			mdelay(stop[i].delay);
	}

	return 0;
}

cs35l53Codec *new_cs35l53_codec(I2cOps *i2c, uint8_t chip)
{
	cs35l53Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &cs35l53_enable;
	codec->component.ops.disable = &cs35l53_disable;
	codec->component.ops.play = &cs35l53_play_boot_beep;

	codec->i2c = i2c;
	codec->chip = chip;

	return codec;
}
