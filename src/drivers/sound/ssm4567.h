/*
 * Copyright (C) 2015 Google Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_SOUND_SSM4567_H__
#define __DRIVERS_SOUND_SSM4567_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/route.h"

typedef enum {
	SSM4567_MODE_PDM,
	SSM4567_MODE_TDM_PCM,
	SSM4567_MODE_I2S,
	SSM4567_MODE_I2S_LEFT,
} ssm4567CodecMode;

typedef struct ssm4567Codec
{
	SoundRouteComponent component;

	I2cOps *i2c;

	uint8_t chip;
	ssm4567CodecMode mode;

	int (*read)(struct ssm4567Codec *codec, uint8_t reg, uint8_t *data);
	int (*write)(struct ssm4567Codec *codec, uint8_t reg, uint8_t data);
	int (*update)(struct ssm4567Codec *codec, uint8_t reg,
		      uint8_t mask, uint8_t value);
} ssm4567Codec;

ssm4567Codec *new_ssm4567_codec(I2cOps *i2c, uint8_t chip,
				ssm4567CodecMode mode);

#define SSM4567_POWER_CTRL				0x00
#define  SSM4567_POWER_CTRL_APWDN_EN			(1 << 7)
#define  SSM4567_POWER_CTRL_BSNS_PWDN			(1 << 6)
#define  SSM4567_POWER_CTRL_VSNS_PWDN			(1 << 5)
#define  SSM4567_POWER_CTRL_ISNS_PWDN			(1 << 4)
#define  SSM4567_POWER_CTRL_BOOST_PWDN			(1 << 3)
#define  SSM4567_POWER_CTRL_AMP_PWDN			(1 << 2)
#define  SSM4567_POWER_CTRL_VBAT_ONLY			(1 << 1)
#define  SSM4567_POWER_CTRL_SPWDN			(1 << 0)

#define SSM4567_AMP_SNS_CTRL				0x01
#define  SSM4567_AMP_SNS_CTRL_SNS_FS			(3 << 4)
#define  SSM4567_AMP_SNS_CTRL_SNS_HPF			(1 << 3)
#define  SSM4567_AMP_SNS_CTRL_EDGES			(1 << 2)
#define  SSM4567_AMP_SNS_CTRL_ANA_GAIN			(1 << 0)

#define SSM4567_DAC_CTRL				0x02
#define  SSM4567_DAC_CTRL_DAC_HV			(1 << 7)
#define  SSM4567_DAC_CTRL_DAC_MUTE			(1 << 6)
#define  SSM4567_DAC_CTRL_DAC_HPF			(1 << 5)
#define  SSM4567_DAC_CTRL_DAC_LPM			(1 << 4)
#define  SSM4567_DAC_CTRL_DAC_FS			(7 << 0)

#define SSM4567_DAC_VOLUME				0x03

#define SSM4567_SAI_CTRL_1				0x04
#define  SSM4567_SAI_CTRL_1_SAI_DRV			(1 << 7)
#define  SSM4567_SAI_CTRL_1_BCLK_POL			(1 << 6)
#define  SSM4567_SAI_CTRL_1_TDM_BCLKS			(3 << 4)
#define  SSM4567_SAI_CTRL_1_FSYNC_MODE			(1 << 3)
#define  SSM4567_SAI_CTRL_1_SDATA_FMT			(1 << 2)
#define   SSM4567_SAI_CTRL_1_SDATA_FMT_LEFT		(1 << 2)
#define   SSM4567_SAI_CTRL_1_SDATA_FMT_I2S		(0 << 2)
#define  SSM4567_SAI_CTRL_1_SAI_MODE			(1 << 1)
#define   SSM4567_SAI_CTRL_1_SAI_MODE_TDM_PCM		(1 << 1)
#define   SSM4567_SAI_CTRL_1_SAI_MODE_I2S		(0 << 1)
#define  SSM4567_SAI_CTRL_1_PDM_MODE			(1 << 0)
#define   SSM4567_SAI_CTRL_1_PDM_MODE_PDM		(1 << 0)
#define   SSM4567_SAI_CTRL_1_PDM_MODE_SERIAL		(0 << 0)

#define SSM4567_SAI_CTRL_2				0x05
#define  SSM4567_SAI_CTRL_2_PAD_DRV			(1 << 6)
#define  SSM4567_SAI_CTRL_2_AUTO_SAI			(1 << 5)
#define  SSM4567_SAI_CTRL_2_MC_I2S			(1 << 4)
#define  SSM4567_SAI_CTRL_2_AUTO_SLOT			(1 << 3)
#define  SSM4567_SAI_CTRL_2_TDM_SLOT			(7 << 0)

#define SSM4567_SAI_P1					0x06
#define  SSM4567_SAI_P1_DAC				(3 << 4)
#define  SSM4567_SAI_P1_SNS				(7 << 0)

#define SSM4567_SAI_P2					0x07
#define  SSM4567_SAI_P2_DAC				(3 << 4)
#define  SSM4567_SAI_P2_SNS				(7 << 0)

#define SSM4567_SAI_P3					0x08
#define  SSM4567_SAI_P3_DAC				(3 << 4)
#define  SSM4567_SAI_P3_SNS				(7 << 0)

#define SSM4567_SAI_P4					0x09
#define  SSM4567_SAI_P4_DAC				(3 << 4)
#define  SSM4567_SAI_P4_SNS				(7 << 0)

#define SSM4567_SAI_P5					0x0a
#define  SSM4567_SAI_P5_SNS				(7 << 0)

#define SSM4567_SAI_P6					0x0b
#define  SSM4567_SAI_P6_SNS				(7 << 0)

#define SSM4567_BATTERY_VOUT				0x0c

#define SSM4567_LIMITER_CTRL_1				0x0d
#define  SSM4567_LIMITER_CTRL_1_SLOPE			(3 << 6)
#define  SSM4567_LIMITER_CTRL_1_VBAT_INF		(7 << 3)
#define  SSM4567_LIMITER_CTRL_1_VBAT_TRACK		(1 << 2)
#define  SSM4567_LIMITER_CTRL_1_LIM_EN			(3 << 0)

#define SSM4567_LIMITER_CTRL_2				0x0e
#define  SSM4567_LIMITER_CTRL_2_LIM_RRT			(3 << 6)
#define  SSM4567_LIMITER_CTRL_2_LIM_ATR			(3 << 4)
#define  SSM4567_LIMITER_CTRL_2_LIM_THRES		(0xf << 0)

#define SSM4567_LIMITER_CTRL_3				0x0f
#define  SSM4567_LIMITER_CTRL_3_TAV			(1 << 3)
#define  SSM4567_LIMITER_CTRL_3_VBAT_HYST		(7 << 0)

#define SSM4567_STATUS_1				0x10
#define  SSM4567_STATUS_1_BST_FLT			(1 << 7)
#define  SSM4567_STATUS_1_LIM_EG			(1 << 5)
#define  SSM4567_STATUS_1_CLIP				(1 << 4)
#define  SSM4567_STATUS_1_UVLO				(1 << 3)
#define  SSM4567_STATUS_1_AMP_OC			(1 << 2)
#define  SSM4567_STATUS_1_OTF				(1 << 1)
#define  SSM4567_STATUS_1_BAT_WARN			(1 << 0)

#define SSM4567_STATUS_2				0x11
#define  SSM4567_STATUS_2_OTW				(1 << 0)

#define SSM4567_FAULT_CTRL				0x12
#define  SSM4567_FAULT_CTRL_OTW_GAIN			(3 << 6)
#define  SSM4567_FAULT_CTRL_MAX_AR			(3 << 4)
#define  SSM4567_FAULT_CTRL_MRCV			(1 << 3)
#define  SSM4567_FAULT_CTRL_ARCV_UV			(1 << 2)
#define  SSM4567_FAULT_CTRL_ARCV_OT			(1 << 1)
#define  SSM4567_FAULT_CTRL_ARCV_OC			(1 << 0)

#define SSM4567_PDM_CTRL				0x13
#define  SSM4567_PDM_CTRL_PDM_LR_SEL			(1 << 7)
#define  SSM4567_PDM_CTRL_PAT_CTRL_EN			(1 << 6)
#define  SSM4567_PDM_CTRL_I2C_ADDR_SET			(1 << 4)
#define  SSM4567_PDM_CTRL_LOW_LATENCY			(3 << 2)
#define  SSM4567_PDM_CTRL_SHARED_CLOCK			(1 << 1)
#define  SSM4567_PDM_CTRL_SEL_VBAT			(1 << 0)

#define SSM4567_MCLK_RATIO				0x14
#define  SSM4567_MCLK_RATIO_AMCS			(1 << 4)
#define  SSM4567_MCLK_RATIO_MCS				(0xf << 0)

#define SSM4567_BOOST_CTRL_1				0x15
#define  SSM4567_BOOST_CTRL_1_ADJ_PGATE			(3 << 6)
#define  SSM4567_BOOST_CTRL_1_EN_DSCGB			(1 << 2)
#define  SSM4567_BOOST_CTRL_1_FPWMB			(1 << 1)
#define  SSM4567_BOOST_CTRL_1_SEL_FREQ			(1 << 0)

#define SSM4567_BOOST_CTRL_2				0x16
#define  SSM4567_BOOST_CTRL_2_ARCV_BST			(1 << 3)
#define  SSM4567_BOOST_CTRL_2_SEL_GM			(3 << 0)

#define SSM4567_SOFT_RESET				0xff
#define  SSM4567_SOFT_RESET_VALUE			0x00

#define SSM4567_REG_COUNT				SSM4567_BOOST_CTRL_2

#endif /* __DRIVERS_SOUND_SSM4567_H__ */
