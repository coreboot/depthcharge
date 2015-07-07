/*
 * rt5645.h -- RealTek ALC5645 ALSA SoC Audio driver
 *
 * Copyright 2013 Realtek Semiconductor Corp.
 * Author: Oder Chiou <oder_chiou@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DRIVERS_SOUND_RT5645_H__
#define __DRIVERS_SOUND_RT5645_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/route.h"

typedef struct
{
	SoundRouteComponent component;

	I2cOps *i2c;
	uint8_t chip;
} rt5645Codec;

rt5645Codec *new_rt5645_codec(I2cOps *i2c, uint8_t chip);

/*
 * RT5645 Registers Definition
 */

/* Info */
#define RT5645_RESET				0x00
#define RT5645_VENDOR_ID			0xfd
#define RT5645_VENDOR_ID1			0xfe
#define RT5645_VENDOR_ID2			0xff

#define RT5645_REG_CNT				(RT5645_VENDOR_ID2+1)
#define RT5645_PR_REG_CNT			255

#define RT5645_DEVICE_ID			0x6308
#define RT5650_DEVICE_ID			0x6419

/*  I/O - Output */
#define RT5645_SPK_VOL				0x01
#define RT5645_HP_VOL				0x02
#define RT5645_LOUT1				0x03
#define RT5645_LOUT_CTRL			0x05
/* I/O - Input */
#define RT5645_IN1_CTRL1			0x0a
#define RT5645_IN1_CTRL2			0x0b
#define RT5645_IN1_CTRL3			0x0c
#define RT5645_IN2_CTRL				0x0d
#define RT5645_INL1_INR1_VOL			0x0f
#define RT5645_SPK_FUNC_LIM			0x14
#define RT5645_ADJ_HPF_CTRL			0x16
/* I/O - ADC/DAC/DMIC */
#define RT5645_DAC1_DIG_VOL			0x19
#define RT5645_DAC2_DIG_VOL			0x1a
#define RT5645_DAC_CTRL				0x1b
#define RT5645_STO1_ADC_DIG_VOL			0x1c
#define RT5645_MONO_ADC_DIG_VOL			0x1d
#define RT5645_ADC_BST_VOL1			0x1e
/* Mixer - D-D */
#define RT5645_ADC_BST_VOL2			0x20
#define RT5645_STO1_ADC_MIXER			0x27
#define RT5645_MONO_ADC_MIXER			0x28
#define RT5645_AD_DA_MIXER			0x29
#define RT5645_STO_DAC_MIXER			0x2a
#define RT5645_MONO_DAC_MIXER			0x2b
#define RT5645_DIG_MIXER			0x2c
#define RT5650_A_DAC_SOUR			0x2d
#define RT5645_DIG_INF1_DATA			0x2f
/* Mixer - PDM */
#define RT5645_PDM_OUT_CTRL			0x31
/* Mixer - ADC */
#define RT5645_REC_L1_MIXER			0x3b
#define RT5645_REC_L2_MIXER			0x3c
#define RT5645_REC_R1_MIXER			0x3d
#define RT5645_REC_R2_MIXER			0x3e
/* Mixer - DAC */
#define RT5645_HPMIXL_CTRL			0x3f
#define RT5645_HPOMIXL_CTRL			0x40
#define RT5645_HPMIXR_CTRL			0x41
#define RT5645_HPOMIXR_CTRL			0x42
#define RT5645_HPO_MIXER			0x45
#define RT5645_SPK_L_MIXER			0x46
#define RT5645_SPK_R_MIXER			0x47
#define RT5645_SPO_MIXER			0x48
#define RT5645_SPO_CLSD_RATIO			0x4a
#define RT5645_OUT_L_GAIN1			0x4d
#define RT5645_OUT_L_GAIN2			0x4e
#define RT5645_OUT_L1_MIXER			0x4f
#define RT5645_OUT_R_GAIN1			0x50
#define RT5645_OUT_R_GAIN2			0x51
#define RT5645_OUT_R1_MIXER			0x52
#define RT5645_LOUT_MIXER			0x53
/* Haptic */
#define RT5645_HAPTIC_CTRL1			0x56
#define RT5645_HAPTIC_CTRL2			0x57
#define RT5645_HAPTIC_CTRL3			0x58
#define RT5645_HAPTIC_CTRL4			0x59
#define RT5645_HAPTIC_CTRL5			0x5a
#define RT5645_HAPTIC_CTRL6			0x5b
#define RT5645_HAPTIC_CTRL7			0x5c
#define RT5645_HAPTIC_CTRL8			0x5d
#define RT5645_HAPTIC_CTRL9			0x5e
#define RT5645_HAPTIC_CTRL10			0x5f
/* Power */
#define RT5645_PWR_DIG1				0x61
#define RT5645_PWR_DIG2				0x62
#define RT5645_PWR_ANLG1			0x63
#define RT5645_PWR_ANLG2			0x64
#define RT5645_PWR_MIXER			0x65
#define RT5645_PWR_VOL				0x66
/* Private Register Control */
#define RT5645_PRIV_INDEX			0x6a
#define RT5645_PRIV_DATA			0x6c
/* Format - ADC/DAC */
#define RT5645_I2S1_SDP				0x70
#define RT5645_I2S2_SDP				0x71
#define RT5645_ADDA_CLK1			0x73
#define RT5645_ADDA_CLK2			0x74
#define RT5645_DMIC_CTRL1			0x75
#define RT5645_DMIC_CTRL2			0x76
/* Format - TDM Control */
#define RT5645_TDM_CTRL_1			0x77
#define RT5645_TDM_CTRL_2			0x78
#define RT5645_TDM_CTRL_3			0x79
#define RT5650_TDM_CTRL_4			0x7a

/* Function - Analog */
#define RT5645_GLB_CLK				0x80
#define RT5645_PLL_CTRL1			0x81
#define RT5645_PLL_CTRL2			0x82
#define RT5645_ASRC_1				0x83
#define RT5645_ASRC_2				0x84
#define RT5645_ASRC_3				0x85
#define RT5645_ASRC_4				0x8a
#define RT5645_DEPOP_M1				0x8e
#define RT5645_DEPOP_M2				0x8f
#define RT5645_DEPOP_M3				0x90
#define RT5645_CHARGE_PUMP			0x91
#define RT5645_MICBIAS				0x93
#define RT5645_A_JD_CTRL1			0x94
#define RT5645_VAD_CTRL4			0x9d
#define RT5645_CLSD_OUT_CTRL			0xa0
/* Function - Digital */
#define RT5645_ADC_EQ_CTRL1			0xae
#define RT5645_ADC_EQ_CTRL2			0xaf
#define RT5645_EQ_CTRL1				0xb0
#define RT5645_EQ_CTRL2				0xb1
#define RT5645_ALC_CTRL_1			0xb3
#define RT5645_ALC_CTRL_2			0xb4
#define RT5645_ALC_CTRL_3			0xb5
#define RT5645_ALC_CTRL_4			0xb6
#define RT5645_ALC_CTRL_5			0xb7
#define RT5645_JD_CTRL				0xbb
#define RT5645_IRQ_CTRL1			0xbc
#define RT5645_IRQ_CTRL2			0xbd
#define RT5645_IRQ_CTRL3			0xbe
#define RT5645_INT_IRQ_ST			0xbf
#define RT5645_GPIO_CTRL1			0xc0
#define RT5645_GPIO_CTRL2			0xc1
#define RT5645_GPIO_CTRL3			0xc2
#define RT5645_BASS_BACK			0xcf
#define RT5645_MP3_PLUS1			0xd0
#define RT5645_MP3_PLUS2			0xd1
#define RT5645_ADJ_HPF1				0xd3
#define RT5645_ADJ_HPF2				0xd4
#define RT5645_HP_CALIB_AMP_DET			0xd6
#define RT5645_SV_ZCD1				0xd9
#define RT5645_SV_ZCD2				0xda
#define RT5645_IL_CMD				0xdb
#define RT5645_IL_CMD2				0xdc
#define RT5645_IL_CMD3				0xdd
#define RT5650_4BTN_IL_CMD1			0xdf
#define RT5650_4BTN_IL_CMD2			0xe0
#define RT5645_DRC1_HL_CTRL1			0xe7
#define RT5645_DRC2_HL_CTRL1			0xe9
#define RT5645_MUTI_DRC_CTRL1			0xea
#define RT5645_ADC_MONO_HP_CTRL1		0xec
#define RT5645_ADC_MONO_HP_CTRL2		0xed
#define RT5645_DRC2_CTRL1			0xf0
#define RT5645_DRC2_CTRL2			0xf1
#define RT5645_DRC2_CTRL3			0xf2
#define RT5645_DRC2_CTRL4			0xf3
#define RT5645_DRC2_CTRL5			0xf4
#define RT5645_JD_CTRL3				0xf8
#define RT5645_JD_CTRL4				0xf9
/* General Control */
#define RT5645_GEN_CTRL1			0xfa
#define RT5645_GEN_CTRL2			0xfb
#define RT5645_GEN_CTRL3			0xfc

/* Index of Codec Private Register definition */
#define RT5645_DIG_VOL				0x00
#define RT5645_PR_ALC_CTRL_1			0x01
#define RT5645_PR_ALC_CTRL_2			0x02
#define RT5645_PR_ALC_CTRL_3			0x03
#define RT5645_PR_ALC_CTRL_4			0x04
#define RT5645_PR_ALC_CTRL_5			0x05
#define RT5645_PR_ALC_CTRL_6			0x06
#define RT5645_BIAS_CUR1			0x12
#define RT5645_BIAS_CUR3			0x14
#define RT5645_CLSD_INT_REG1			0x1c
#define RT5645_MAMP_INT_REG2			0x37
#define RT5645_CHOP_DAC_ADC			0x3d
#define RT5645_MIXER_INT_REG			0x3f
#define RT5645_3D_SPK				0x63
#define RT5645_WND_1				0x6c
#define RT5645_WND_2				0x6d
#define RT5645_WND_3				0x6e
#define RT5645_WND_4				0x6f
#define RT5645_WND_5				0x70
#define RT5645_WND_8				0x73
#define RT5645_DIP_SPK_INF			0x75
#define RT5645_HP_DCC_INT1			0x77
#define RT5645_EQ_BW_LOP			0xa0
#define RT5645_EQ_GN_LOP			0xa1
#define RT5645_EQ_FC_BP1			0xa2
#define RT5645_EQ_BW_BP1			0xa3
#define RT5645_EQ_GN_BP1			0xa4
#define RT5645_EQ_FC_BP2			0xa5
#define RT5645_EQ_BW_BP2			0xa6
#define RT5645_EQ_GN_BP2			0xa7
#define RT5645_EQ_FC_BP3			0xa8
#define RT5645_EQ_BW_BP3			0xa9
#define RT5645_EQ_GN_BP3			0xaa
#define RT5645_EQ_FC_BP4			0xab
#define RT5645_EQ_BW_BP4			0xac
#define RT5645_EQ_GN_BP4			0xad
#define RT5645_EQ_FC_HIP1			0xae
#define RT5645_EQ_GN_HIP1			0xaf
#define RT5645_EQ_FC_HIP2			0xb0
#define RT5645_EQ_BW_HIP2			0xb1
#define RT5645_EQ_GN_HIP2			0xb2
#define RT5645_EQ_PRE_VOL			0xb3
#define RT5645_EQ_PST_VOL			0xb4

/* global definition */
#define RT5645_L_MUTE				(0x1 << 15)
#define RT5645_L_MUTE_SFT			15
#define RT5645_VOL_L_MUTE			(0x1 << 14)
#define RT5645_VOL_L_SFT			14
#define RT5645_R_MUTE				(0x1 << 7)
#define RT5645_R_MUTE_SFT			7
#define RT5645_VOL_R_MUTE			(0x1 << 6)
#define RT5645_VOL_R_SFT			6
#define RT5645_L_VOL_MASK			(0x3f << 8)
#define RT5645_L_VOL_SFT			8
#define RT5645_R_VOL_MASK			(0x3f)
#define RT5645_R_VOL_SFT			0

/* I2S1/2 Audio Serial Data Port Control (0x70 0x71) */
#define RT5645_I2S_MS_MASK			(0x1 << 15)
#define RT5645_I2S_MS_SFT			15
#define RT5645_I2S_MS_M				(0x0 << 15)
#define RT5645_I2S_MS_S				(0x1 << 15)
#define RT5645_I2S_O_CP_MASK			(0x3 << 10)
#define RT5645_I2S_O_CP_SFT			10
#define RT5645_I2S_O_CP_OFF			(0x0 << 10)
#define RT5645_I2S_O_CP_U_LAW			(0x1 << 10)
#define RT5645_I2S_O_CP_A_LAW			(0x2 << 10)
#define RT5645_I2S_I_CP_MASK			(0x3 << 8)
#define RT5645_I2S_I_CP_SFT			8
#define RT5645_I2S_I_CP_OFF			(0x0 << 8)
#define RT5645_I2S_I_CP_U_LAW			(0x1 << 8)
#define RT5645_I2S_I_CP_A_LAW			(0x2 << 8)
#define RT5645_I2S_BP_MASK			(0x1 << 7)
#define RT5645_I2S_BP_SFT			7
#define RT5645_I2S_BP_NOR			(0x0 << 7)
#define RT5645_I2S_BP_INV			(0x1 << 7)
#define RT5645_I2S_DL_MASK			(0x3 << 2)
#define RT5645_I2S_DL_SFT			2
#define RT5645_I2S_DL_16			(0x0 << 2)
#define RT5645_I2S_DL_20			(0x1 << 2)
#define RT5645_I2S_DL_24			(0x2 << 2)
#define RT5645_I2S_DL_8				(0x3 << 2)
#define RT5645_I2S_DF_MASK			(0x3)
#define RT5645_I2S_DF_SFT			0
#define RT5645_I2S_DF_I2S			(0x0)
#define RT5645_I2S_DF_LEFT			(0x1)
#define RT5645_I2S_DF_PCM_A			(0x2)
#define RT5645_I2S_DF_PCM_B			(0x3)

#endif /* __DRIVERS_SOUND_RT5645_H__ */
