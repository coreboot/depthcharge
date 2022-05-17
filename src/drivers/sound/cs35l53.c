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
	/* To set EDMYSTOP bit in SSPSP - number of SCLK cycles after data */
	.ssp_psp_T4 = 0,
	/* To set SFRMWDTH bit in SSPSP - frame width */
	.ssp_psp_T6 = 0x20,
	/* To set TTSA bit n SSTSA - data transmit timeslot */
	.ssp_active_tx_slots_map = 3,
	/* To set RTSA bit n SSRSA - data receive timeslot */
	.ssp_active_rx_slots_map = 3,
};

typedef struct reg_config {
	uint32_t addr;
	uint32_t data;
} reg_config_t;

reg_config_t enable[] = {
	{CS35L53_AMP_CTRL, CS35L53_AMP_HPF_PCM_DIS },
	{CS35L53_DAC_MSM_CONFIG, CS35L53_AMP_DIAG_SEL },
	{CS35L53_DAC_MSM_CONFIG, CS35L53_AMP_DIAG_SEL|CS35L53_AMP_DIAG_EN },
	{CS35L53_ASP_DACPCM1_INPUT, CS35L53_ASP_DACPCM1_SRC_DIAG_GEN },
	{CS35L53_REFCLK_INPUT, CS35L53_PLL_OPEN_LOOP|CS35L53_PLL_REFCLK_EN },
	{CS35L53_ASP_CONTROL2, CS35L53_ASP_TX_RX_WIDTH2 },
	{CS35L53_PLL_CONFIG1, CS35L53_PLL_FRC_FREQ_LOCK_EN },
	{CS35L53_BLOCK_ENABLES, CS35L53_IMON_EN|CS35L53_VMON_EN|
		CS35L53_TEMPMON_EN|CS35L53_VBSTMON_EN|CS35L53_VPMON_EN|
		CS35L53_BST_EN|CS35L53_AMP_EN },
	{CS35L53_GLOBAL_ENABLES, CS35L53_GLOBAL_EN }
};

reg_config_t disable[] = {
	{CS35L53_GLOBAL_ENABLES, CS35L53_GLOBAL_DIS }
};

static int cs35l53_i2c_writedw(cs35l53Codec *codec, uint32_t reg, uint32_t data)
{
	if (i2c_addrl_writel(codec->i2c, codec->chip, reg, data)) {
		printf("cs35l53 i2c reg:%x write err\n", reg);
		return -1;
	}
	return 0;
}

static int cs35l53_enable(SoundRouteComponentOps *me)
{
	cs35l53Codec *codec = container_of(me, cs35l53Codec, component.ops);

	for (int i = 0; i < ARRAY_SIZE(enable); i++) {
		if (cs35l53_i2c_writedw(codec, enable[i].addr, enable[i].data) == -1)
			return 1;
	}
	return 0;
}

static int cs35l53_disable(SoundRouteComponentOps *me)
{
	cs35l53Codec *codec = container_of(me, cs35l53Codec, component.ops);

	for (int i = 0; i < ARRAY_SIZE(disable); i++) {
		if (cs35l53_i2c_writedw(codec, disable[i].addr, disable[i].data) == -1)
			return 1;
	}
	return 0;
}

cs35l53Codec *new_cs35l53_codec(I2cOps *i2c, uint8_t chip)
{
	cs35l53Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &cs35l53_enable;
	codec->component.ops.disable = &cs35l53_disable;

	codec->i2c = i2c;
	codec->chip = chip;

	return codec;
}
