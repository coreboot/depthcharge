// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * rt5682s.c -- RealTek ALC5682i-VS ALSA SoC Audio driver
 *
 * Copyright 2021 Realtek Semiconductor Corp. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdbool.h>
#include <libpayload.h>
#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/rt5682s.h"

/*
 * RT5682S Registers Definition
 */

#define RT5682S_REG_CNT 0x2fc

/* Info */
#define RT5682S_RESET 0x0000
#define RT5682S_VERSION_ID 0x00fd
#define RT5682S_VENDOR_ID 0x00fe
#define RT5682S_DEVICE_ID 0x00ff
/*  I/O - Output */
#define RT5682S_HP_CTRL_1 0x0002
#define RT5682S_HP_CTRL_2 0x0003
#define RT5682S_HPL_GAIN 0x0005
#define RT5682S_HPR_GAIN 0x0006

#define RT5682S_I2C_CTRL 0x0008

/* I/O - Input */
#define RT5682S_CBJ_BST_CTRL 0x000b
#define RT5682S_CBJ_DET_CTRL 0x000f
#define RT5682S_CBJ_CTRL_1 0x0010
#define RT5682S_CBJ_CTRL_2 0x0011
#define RT5682S_CBJ_CTRL_3 0x0012
#define RT5682S_CBJ_CTRL_4 0x0013
#define RT5682S_CBJ_CTRL_5 0x0014
#define RT5682S_CBJ_CTRL_6 0x0015
#define RT5682S_CBJ_CTRL_7 0x0016
#define RT5682S_CBJ_CTRL_8 0x0017
/* I/O - ADC/DAC/DMIC */
#define RT5682S_DAC1_DIG_VOL 0x0019
#define RT5682S_STO1_ADC_DIG_VOL 0x001c
#define RT5682S_STO1_ADC_BOOST 0x001f
#define RT5682S_HP_IMP_GAIN_1 0x0022
#define RT5682S_HP_IMP_GAIN_2 0x0023
/* Mixer - D-D */
#define RT5682S_SIDETONE_CTRL 0x0024
#define RT5682S_STO1_ADC_MIXER 0x0026
#define RT5682S_AD_DA_MIXER 0x0029
#define RT5682S_STO1_DAC_MIXER 0x002a
#define RT5682S_A_DAC1_MUX 0x002b
#define RT5682S_DIG_INF2_DATA 0x0030
/* Mixer - ADC */
#define RT5682S_REC_MIXER 0x003c
#define RT5682S_CAL_REC 0x0044
/* HP Analog Offset Control */
#define RT5682S_HP_ANA_OST_CTRL_1 0x004b
#define RT5682S_HP_ANA_OST_CTRL_2 0x004c
#define RT5682S_HP_ANA_OST_CTRL_3 0x004d
/* Power */
#define RT5682S_PWR_DIG_1 0x0061
#define RT5682S_PWR_DIG_2 0x0062
#define RT5682S_PWR_ANLG_1 0x0063
#define RT5682S_PWR_ANLG_2 0x0064
#define RT5682S_PWR_ANLG_3 0x0065
#define RT5682S_PWR_MIXER 0x0066

#define RT5682S_MB_CTRL 0x0067
#define RT5682S_CLK_GATE_TCON_1 0x0068
#define RT5682S_CLK_GATE_TCON_2 0x0069
#define RT5682S_CLK_GATE_TCON_3 0x006a
/* Clock Detect */
#define RT5682S_CLK_DET 0x006b
/* Filter Auto Reset */
#define RT5682S_RESET_LPF_CTRL 0x006c
#define RT5682S_RESET_HPF_CTRL 0x006d
/* DMIC */
#define RT5682S_DMIC_CTRL_1 0x006e
#define RT5682S_LPF_AD_DMIC 0x006f
/* Format - ADC/DAC */
#define RT5682S_I2S1_SDP 0x0070
#define RT5682S_I2S2_SDP 0x0071
#define RT5682S_ADDA_CLK_1 0x0073
#define RT5682S_ADDA_CLK_2 0x0074
#define RT5682S_I2S1_F_DIV_CTRL_1 0x0075
#define RT5682S_I2S1_F_DIV_CTRL_2 0x0076
/* Format - TDM Control */
#define RT5682S_TDM_CTRL 0x0079
#define RT5682S_TDM_ADDA_CTRL_1 0x007a
#define RT5682S_TDM_ADDA_CTRL_2 0x007b
#define RT5682S_DATA_SEL_CTRL_1 0x007c
#define RT5682S_TDM_TCON_CTRL_1 0x007e
#define RT5682S_TDM_TCON_CTRL_2 0x007f
/* Function - Analog */
#define RT5682S_GLB_CLK 0x0080
#define RT5682S_PLL_TRACK_1 0x0083
#define RT5682S_PLL_TRACK_2 0x0084
#define RT5682S_PLL_TRACK_3 0x0085
#define RT5682S_PLL_TRACK_4 0x0086
#define RT5682S_PLL_TRACK_5 0x0087
#define RT5682S_PLL_TRACK_6 0x0088
#define RT5682S_PLL_TRACK_11 0x008c
#define RT5682S_DEPOP_1 0x008e
#define RT5682S_HP_CHARGE_PUMP_1 0x008f
#define RT5682S_HP_CHARGE_PUMP_2 0x0091
#define RT5682S_HP_CHARGE_PUMP_3 0x0092
#define RT5682S_MICBIAS_1 0x0093
#define RT5682S_MICBIAS_2 0x0094
#define RT5682S_MICBIAS_3 0x0095

#define RT5682S_PLL_TRACK_12 0x0096
#define RT5682S_PLL_TRACK_14 0x0097
#define RT5682S_PLL_CTRL_1 0x0098
#define RT5682S_PLL_CTRL_2 0x0099
#define RT5682S_PLL_CTRL_3 0x009a
#define RT5682S_PLL_CTRL_4 0x009b
#define RT5682S_PLL_CTRL_5 0x009c
#define RT5682S_PLL_CTRL_6 0x009d
#define RT5682S_PLL_CTRL_7 0x009e

#define RT5682S_RC_CLK_CTRL 0x009f
#define RT5682S_I2S2_M_CLK_CTRL_1 0x00a0
#define RT5682S_I2S2_F_DIV_CTRL_1 0x00a3
#define RT5682S_I2S2_F_DIV_CTRL_2 0x00a4

#define RT5682S_IRQ_CTRL_1 0x00b6
#define RT5682S_IRQ_CTRL_2 0x00b7
#define RT5682S_IRQ_CTRL_3 0x00b8
#define RT5682S_IRQ_CTRL_4 0x00b9
#define RT5682S_INT_ST_1 0x00be
#define RT5682S_GPIO_CTRL_1 0x00c0
#define RT5682S_GPIO_CTRL_2 0x00c1
#define RT5682S_GPIO_ST 0x00c2
#define RT5682S_HP_AMP_DET_CTRL_1 0x00d0
#define RT5682S_MID_HP_AMP_DET 0x00d2
#define RT5682S_LOW_HP_AMP_DET 0x00d3
#define RT5682S_DELAY_BUF_CTRL 0x00d4
#define RT5682S_SV_ZCD_1 0x00d9
#define RT5682S_SV_ZCD_2 0x00da
#define RT5682S_IL_CMD_1 0x00db
#define RT5682S_IL_CMD_2 0x00dc
#define RT5682S_IL_CMD_3 0x00dd
#define RT5682S_IL_CMD_4 0x00de
#define RT5682S_IL_CMD_5 0x00df
#define RT5682S_IL_CMD_6 0x00e0
#define RT5682S_4BTN_IL_CMD_1 0x00e2
#define RT5682S_4BTN_IL_CMD_2 0x00e3
#define RT5682S_4BTN_IL_CMD_3 0x00e4
#define RT5682S_4BTN_IL_CMD_4 0x00e5
#define RT5682S_4BTN_IL_CMD_5 0x00e6
#define RT5682S_4BTN_IL_CMD_6 0x00e7
#define RT5682S_4BTN_IL_CMD_7 0x00e8

#define RT5682S_ADC_STO1_HP_CTRL_1 0x00ea
#define RT5682S_ADC_STO1_HP_CTRL_2 0x00eb
#define RT5682S_AJD1_CTRL 0x00f0
#define RT5682S_JD_CTRL_1 0x00f6
/* General Control */
#define RT5682S_DUMMY_1 0x00fa
#define RT5682S_DUMMY_2 0x00fb
#define RT5682S_DUMMY_3 0x00fc

#define RT5682S_DAC_ADC_DIG_VOL1 0x0100
#define RT5682S_BIAS_CUR_CTRL_2 0x010b
#define RT5682S_BIAS_CUR_CTRL_3 0x010c
#define RT5682S_BIAS_CUR_CTRL_4 0x010d
#define RT5682S_BIAS_CUR_CTRL_5 0x010e
#define RT5682S_BIAS_CUR_CTRL_6 0x010f
#define RT5682S_BIAS_CUR_CTRL_7 0x0110
#define RT5682S_BIAS_CUR_CTRL_8 0x0111
#define RT5682S_BIAS_CUR_CTRL_9 0x0112
#define RT5682S_BIAS_CUR_CTRL_10 0x0113
#define RT5682S_VREF_REC_OP_FB_CAP_CTRL_1 0x0117
#define RT5682S_VREF_REC_OP_FB_CAP_CTRL_2 0x0118
#define RT5682S_CHARGE_PUMP_1 0x0125
#define RT5682S_DIG_IN_CTRL_1 0x0132
#define RT5682S_PAD_DRIVING_CTRL 0x0136
#define RT5682S_CHOP_DAC_1 0x0139
#define RT5682S_CHOP_DAC_2 0x013a
#define RT5682S_CHOP_ADC 0x013b
#define RT5682S_CALIB_ADC_CTRL 0x013c
#define RT5682S_VOL_TEST 0x013f
#define RT5682S_SPKVDD_DET_ST 0x0142
#define RT5682S_TEST_MODE_CTRL_1 0x0145
#define RT5682S_TEST_MODE_CTRL_2 0x0146
#define RT5682S_TEST_MODE_CTRL_3 0x0147
#define RT5682S_TEST_MODE_CTRL_4 0x0148
#define RT5682S_PLL_INTERNAL_1 0x0156
#define RT5682S_PLL_INTERNAL_2 0x0157
#define RT5682S_PLL_INTERNAL_3 0x0158
#define RT5682S_PLL_INTERNAL_4 0x0159
#define RT5682S_STO_NG2_CTRL_1 0x0160
#define RT5682S_STO_NG2_CTRL_2 0x0161
#define RT5682S_STO_NG2_CTRL_3 0x0162
#define RT5682S_STO_NG2_CTRL_4 0x0163
#define RT5682S_STO_NG2_CTRL_5 0x0164
#define RT5682S_STO_NG2_CTRL_6 0x0165
#define RT5682S_STO_NG2_CTRL_7 0x0166
#define RT5682S_STO_NG2_CTRL_8 0x0167
#define RT5682S_STO_NG2_CTRL_9 0x0168
#define RT5682S_STO_NG2_CTRL_10 0x0169
#define RT5682S_STO1_DAC_SIL_DET 0x0190
#define RT5682S_SIL_PSV_CTRL1 0x0194
#define RT5682S_SIL_PSV_CTRL2 0x0195
#define RT5682S_SIL_PSV_CTRL3 0x0197
#define RT5682S_SIL_PSV_CTRL4 0x0198
#define RT5682S_SIL_PSV_CTRL5 0x0199
#define RT5682S_HP_IMP_SENS_CTRL_1 0x01ac
#define RT5682S_HP_IMP_SENS_CTRL_2 0x01ad
#define RT5682S_HP_IMP_SENS_CTRL_3 0x01ae
#define RT5682S_HP_IMP_SENS_CTRL_4 0x01af
#define RT5682S_HP_IMP_SENS_CTRL_5 0x01b0
#define RT5682S_HP_IMP_SENS_CTRL_6 0x01b1
#define RT5682S_HP_IMP_SENS_CTRL_7 0x01b2
#define RT5682S_HP_IMP_SENS_CTRL_8 0x01b3
#define RT5682S_HP_IMP_SENS_CTRL_9 0x01b4
#define RT5682S_HP_IMP_SENS_CTRL_10 0x01b5
#define RT5682S_HP_IMP_SENS_CTRL_11 0x01b6
#define RT5682S_HP_IMP_SENS_CTRL_12 0x01b7
#define RT5682S_HP_IMP_SENS_CTRL_13 0x01b8
#define RT5682S_HP_IMP_SENS_CTRL_14 0x01b9
#define RT5682S_HP_IMP_SENS_CTRL_15 0x01ba
#define RT5682S_HP_IMP_SENS_CTRL_16 0x01bb
#define RT5682S_HP_IMP_SENS_CTRL_17 0x01bc
#define RT5682S_HP_IMP_SENS_CTRL_18 0x01bd
#define RT5682S_HP_IMP_SENS_CTRL_19 0x01be
#define RT5682S_HP_IMP_SENS_CTRL_20 0x01bf
#define RT5682S_HP_IMP_SENS_CTRL_21 0x01c0
#define RT5682S_HP_IMP_SENS_CTRL_22 0x01c1
#define RT5682S_HP_IMP_SENS_CTRL_23 0x01c2
#define RT5682S_HP_IMP_SENS_CTRL_24 0x01c3
#define RT5682S_HP_IMP_SENS_CTRL_25 0x01c4
#define RT5682S_HP_IMP_SENS_CTRL_26 0x01c5
#define RT5682S_HP_IMP_SENS_CTRL_27 0x01c6
#define RT5682S_HP_IMP_SENS_CTRL_28 0x01c7
#define RT5682S_HP_IMP_SENS_CTRL_29 0x01c8
#define RT5682S_HP_IMP_SENS_CTRL_30 0x01c9
#define RT5682S_HP_IMP_SENS_CTRL_31 0x01ca
#define RT5682S_HP_IMP_SENS_CTRL_32 0x01cb
#define RT5682S_HP_IMP_SENS_CTRL_33 0x01cc
#define RT5682S_HP_IMP_SENS_CTRL_34 0x01cd
#define RT5682S_HP_IMP_SENS_CTRL_35 0x01ce
#define RT5682S_HP_IMP_SENS_CTRL_36 0x01cf
#define RT5682S_HP_IMP_SENS_CTRL_37 0x01d0
#define RT5682S_HP_IMP_SENS_CTRL_38 0x01d1
#define RT5682S_HP_IMP_SENS_CTRL_39 0x01d2
#define RT5682S_HP_IMP_SENS_CTRL_40 0x01d3
#define RT5682S_HP_IMP_SENS_CTRL_41 0x01d4
#define RT5682S_HP_IMP_SENS_CTRL_42 0x01d5
#define RT5682S_HP_IMP_SENS_CTRL_43 0x01d6
#define RT5682S_HP_IMP_SENS_CTRL_44 0x01d7
#define RT5682S_HP_IMP_SENS_CTRL_45 0x01d8
#define RT5682S_HP_IMP_SENS_CTRL_46 0x01d9
#define RT5682S_HP_LOGIC_CTRL_1 0x01da
#define RT5682S_HP_LOGIC_CTRL_2 0x01db
#define RT5682S_HP_LOGIC_CTRL_3 0x01dc
#define RT5682S_HP_CALIB_CTRL_1 0x01de
#define RT5682S_HP_CALIB_CTRL_2 0x01df
#define RT5682S_HP_CALIB_CTRL_3 0x01e0
#define RT5682S_HP_CALIB_CTRL_4 0x01e1
#define RT5682S_HP_CALIB_CTRL_5 0x01e2
#define RT5682S_HP_CALIB_CTRL_6 0x01e3
#define RT5682S_HP_CALIB_CTRL_7 0x01e4
#define RT5682S_HP_CALIB_CTRL_8 0x01e5
#define RT5682S_HP_CALIB_CTRL_9 0x01e6
#define RT5682S_HP_CALIB_CTRL_10 0x01e7
#define RT5682S_HP_CALIB_CTRL_11 0x01e8
#define RT5682S_HP_CALIB_ST_1 0x01ea
#define RT5682S_HP_CALIB_ST_2 0x01eb
#define RT5682S_HP_CALIB_ST_3 0x01ec
#define RT5682S_HP_CALIB_ST_4 0x01ed
#define RT5682S_HP_CALIB_ST_5 0x01ee
#define RT5682S_HP_CALIB_ST_6 0x01ef
#define RT5682S_HP_CALIB_ST_7 0x01f0
#define RT5682S_HP_CALIB_ST_8 0x01f1
#define RT5682S_HP_CALIB_ST_9 0x01f2
#define RT5682S_HP_CALIB_ST_10 0x01f3
#define RT5682S_HP_CALIB_ST_11 0x01f4
#define RT5682S_SAR_IL_CMD_1 0x0210
#define RT5682S_SAR_IL_CMD_2 0x0211
#define RT5682S_SAR_IL_CMD_3 0x0212
#define RT5682S_SAR_IL_CMD_4 0x0213
#define RT5682S_SAR_IL_CMD_5 0x0214
#define RT5682S_SAR_IL_CMD_6 0x0215
#define RT5682S_SAR_IL_CMD_7 0x0216
#define RT5682S_SAR_IL_CMD_8 0x0217
#define RT5682S_SAR_IL_CMD_9 0x0218
#define RT5682S_SAR_IL_CMD_10 0x0219
#define RT5682S_SAR_IL_CMD_11 0x021a
#define RT5682S_SAR_IL_CMD_12 0x021b
#define RT5682S_SAR_IL_CMD_13 0x021c
#define RT5682S_SAR_IL_CMD_14 0x021d
#define RT5682S_DUMMY_4 0x02fa
#define RT5682S_DUMMY_5 0x02fb
#define RT5682S_DUMMY_6 0x02fc

/* global definition */
#define RT5682S_L_MUTE (0x1 << 15)
#define RT5682S_L_MUTE_SFT 15
#define RT5682S_R_MUTE (0x1 << 7)
#define RT5682S_R_MUTE_SFT 7
#define RT5682S_L_VOL_SFT 8
#define RT5682S_R_VOL_SFT 0
#define RT5682S_CLK_SRC_MCLK (0x0)
#define RT5682S_CLK_SRC_PLL1 (0x1)
#define RT5682S_CLK_SRC_PLL2 (0x2)
#define RT5682S_CLK_SRC_RCCLK (0x4) /* 25M */

/* Power Management for Analog 3 (0x0065) */
#define RT5682S_PWR_LDO_PLLA (0x1 << 15)
#define RT5682S_PWR_LDO_PLLA_BIT 15
#define RT5682S_PWR_LDO_PLLB (0x1 << 14)
#define RT5682S_PWR_LDO_PLLB_BIT 14
#define RT5682S_PWR_BIAS_PLLA (0x1 << 13)
#define RT5682S_PWR_BIAS_PLLA_BIT 13
#define RT5682S_PWR_BIAS_PLLB (0x1 << 12)
#define RT5682S_PWR_BIAS_PLLB_BIT 12
#define RT5682S_PWR_CBJ (0x1 << 9)
#define RT5682S_PWR_CBJ_BIT 9
#define RT5682S_RSTB_PLLB (0x1 << 7)
#define RT5682S_RSTB_PLLB_BIT 7
#define RT5682S_RSTB_PLLA (0x1 << 6)
#define RT5682S_RSTB_PLLA_BIT 6
#define RT5682S_PWR_PLLB (0x1 << 5)
#define RT5682S_PWR_PLLB_BIT 5
#define RT5682S_PWR_PLLA (0x1 << 4)
#define RT5682S_PWR_PLLA_BIT 4
#define RT5682S_PWR_LDO_MB2 (0x1 << 2)
#define RT5682S_PWR_LDO_MB2_BIT 2
#define RT5682S_PWR_LDO_MB1 (0x1 << 1)
#define RT5682S_PWR_LDO_MB1_BIT 1
#define RT5682S_PWR_BGLDO (0x1 << 0)
#define RT5682S_PWR_BGLDO_BIT 0

/* Digital Microphone Control 1 (0x006e) */
#define RT5682S_DMIC_1_EN_MASK (0x1 << 15)
#define RT5682S_DMIC_1_EN_SFT 15
#define RT5682S_DMIC_1_DIS (0x0 << 15)
#define RT5682S_DMIC_1_EN (0x1 << 15)
#define RT5682S_FIFO_CLK_DIV_MASK (0x7 << 12)
#define RT5682S_FIFO_CLK_DIV_2 (0x1 << 12)
#define RT5682S_DMIC_1_DP_MASK (0x3 << 4)
#define RT5682S_DMIC_1_DP_SFT 4
#define RT5682S_DMIC_1_DP_GPIO2 (0x0 << 4)
#define RT5682S_DMIC_1_DP_GPIO5 (0x1 << 4)
#define RT5682S_DMIC_CLK_MASK (0xf << 0)
#define RT5682S_DMIC_CLK_SFT 0

/* ADC/DAC Clock Control 1 (0x0073) */
#define RT5682S_ADC_OSR_MASK (0xf << 12)
#define RT5682S_ADC_OSR_SFT 12
#define RT5682S_ADC_OSR_D_1 (0x0 << 12)
#define RT5682S_ADC_OSR_D_2 (0x1 << 12)
#define RT5682S_ADC_OSR_D_4 (0x2 << 12)
#define RT5682S_ADC_OSR_D_6 (0x3 << 12)
#define RT5682S_ADC_OSR_D_8 (0x4 << 12)
#define RT5682S_ADC_OSR_D_12 (0x5 << 12)
#define RT5682S_ADC_OSR_D_16 (0x6 << 12)
#define RT5682S_ADC_OSR_D_24 (0x7 << 12)
#define RT5682S_ADC_OSR_D_32 (0x8 << 12)
#define RT5682S_ADC_OSR_D_48 (0x9 << 12)
#define RT5682S_I2S_M_D_MASK (0xf << 8)
#define RT5682S_I2S_M_D_SFT 8
#define RT5682S_I2S_M_D_1 (0x0 << 8)
#define RT5682S_I2S_M_D_2 (0x1 << 8)
#define RT5682S_I2S_M_D_3 (0x2 << 8)
#define RT5682S_I2S_M_D_4 (0x3 << 8)
#define RT5682S_I2S_M_D_6 (0x4 << 8)
#define RT5682S_I2S_M_D_8 (0x5 << 8)
#define RT5682S_I2S_M_D_12 (0x6 << 8)
#define RT5682S_I2S_M_D_16 (0x7 << 8)
#define RT5682S_I2S_M_D_24 (0x8 << 8)
#define RT5682S_I2S_M_D_32 (0x9 << 8)
#define RT5682S_I2S_M_D_48 (0x10 << 8)
#define RT5682S_I2S_M_CLK_SRC_MASK (0x7 << 4)
#define RT5682S_I2S_M_CLK_SRC_SFT 4
#define RT5682S_DAC_OSR_MASK (0xf << 0)
#define RT5682S_DAC_OSR_SFT 0
#define RT5682S_DAC_OSR_D_1 (0x0 << 0)
#define RT5682S_DAC_OSR_D_2 (0x1 << 0)
#define RT5682S_DAC_OSR_D_4 (0x2 << 0)
#define RT5682S_DAC_OSR_D_6 (0x3 << 0)
#define RT5682S_DAC_OSR_D_8 (0x4 << 0)
#define RT5682S_DAC_OSR_D_12 (0x5 << 0)
#define RT5682S_DAC_OSR_D_16 (0x6 << 0)
#define RT5682S_DAC_OSR_D_24 (0x7 << 0)
#define RT5682S_DAC_OSR_D_32 (0x8 << 0)
#define RT5682S_DAC_OSR_D_48 (0x9 << 0)

/* PLL tracking mode 2 3 (0x0084)(0x0085)*/
#define RT5682S_FILTER_CLK_SEL_MASK (0x7 << 12)
#define RT5682S_FILTER_CLK_SEL_SFT 12
#define RT5682S_FILTER_CLK_DIV_MASK (0xf << 8)
#define RT5682S_FILTER_CLK_DIV_SFT 8

/* PLL M/N/K Code Control 1 (0x0098) */
#define RT5682S_PLLA_N_MASK (0x1ff << 0)

/* PLL M/N/K Code Control 2 (0x0099) */
#define RT5682S_PLLA_M_MASK (0x1f << 8)
#define RT5682S_PLLA_M_SFT 8
#define RT5682S_PLLA_K_MASK (0x1f << 0)

/* PLL M/N/K Code Control 3 (0x009a) */
#define RT5682S_PLLB_N_MASK (0x3ff << 0)

/* PLL M/N/K Code Control 4 (0x009b) */
#define RT5682S_PLLB_M_MASK (0x1f << 8)
#define RT5682S_PLLB_M_SFT 8
#define RT5682S_PLLB_K_MASK (0x1f << 0)

/* PLL M/N/K Code Control 6 (0x009d) */
#define RT5682S_PLLB_SEL_PS_MASK (0x1 << 13)
#define RT5682S_PLLB_SEL_PS_SFT 13
#define RT5682S_PLLB_BYP_PS_MASK (0x1 << 12)
#define RT5682S_PLLB_BYP_PS_SFT 12
#define RT5682S_PLLB_M_BP_MASK (0x1 << 11)
#define RT5682S_PLLB_M_BP_SFT 11
#define RT5682S_PLLB_K_BP_MASK (0x1 << 10)
#define RT5682S_PLLB_K_BP_SFT 10
#define RT5682S_PLLA_M_BP_MASK (0x1 << 7)
#define RT5682S_PLLA_M_BP_SFT 7
#define RT5682S_PLLA_K_BP_MASK (0x1 << 6)
#define RT5682S_PLLA_K_BP_SFT 6

/* PLL M/N/K Code Control 7 (0x009e) */
#define RT5682S_PLLB_SRC_MASK (0x1)
#define RT5682S_PLLB_SRC_DFIN (0x1)
#define RT5682S_PLLB_SRC_PLLA (0x0)

#define DEVICE_ID 0x6749

enum {
	USE_PLLA,
	USE_PLLB,
	USE_PLLAB,
};

struct pll_calc_map {
	unsigned int freq_in;
	unsigned int freq_out;
	int m;
	int n;
	int k;
	bool m_bp;
	bool k_bp;
	bool byp_ps;
	bool sel_ps;
};

static const struct pll_calc_map plla_table[] = {
	{2048000, 24576000, 0, 46, 2, true, false, false, false},
	{256000, 24576000, 0, 382, 2, true, false, false, false},
	{512000, 24576000, 0, 190, 2, true, false, false, false},
	{4096000, 24576000, 0, 22, 2, true, false, false, false},
	{4800000, 3840000, 0, 18, 3, true, false, false, false},
	{1024000, 24576000, 0, 94, 2, true, false, false, false},
	{11289600, 22579200, 1, 22, 2, false, false, false, false},
	{1411200, 22579200, 0, 62, 2, true, false, false, false},
	{2822400, 22579200, 0, 30, 2, true, false, false, false},
	{12288000, 24576000, 1, 22, 2, false, false, false, false},
	{1536000, 24576000, 0, 62, 2, true, false, false, false},
	{3072000, 24576000, 0, 30, 2, true, false, false, false},
	{24576000, 49152000, 4, 22, 0, false, false, false, false},
	{3072000, 49152000, 0, 30, 0, true, false, false, false},
	{6144000, 49152000, 0, 30, 0, false, false, false, false},
	{49152000, 98304000, 10, 22, 0, false, true, false, false},
	{6144000, 98304000, 0, 30, 0, false, true, false, false},
	{12288000, 98304000, 1, 22, 0, false, true, false, false},
	{48000000, 3840000, 10, 22, 23, false, false, false, false},
	{24000000, 3840000, 4, 22, 23, false, false, false, false},
	{19200000, 3840000, 3, 23, 23, false, false, false, false},
	{38400000, 3840000, 8, 23, 23, false, false, false, false},
};

static const struct pll_calc_map pllb_table[] = {
	{48000000, 24576000, 8, 6, 3, false, false, false, false},
	{48000000, 22579200, 23, 12, 3, false, false, false, true},
	{24000000, 24576000, 3, 6, 3, false, false, false, false},
	{24000000, 22579200, 23, 26, 3, false, false, false, true},
	{19200000, 24576000, 2, 6, 3, false, false, false, false},
	{19200000, 22579200, 3, 5, 3, false, false, false, true},
	{38400000, 24576000, 6, 6, 3, false, false, false, false},
	{38400000, 22579200, 8, 5, 3, false, false, false, true},
	{3840000, 49152000, 0, 6, 0, true, false, false, false},
	{3840000, 24576000, 2, 6, 3, false, false, false, false},
};

/* RT5682S uses 16-bit register addresses, and 16-bit register data */
static int rt5682s_i2c_readw(rt5682sCodec *codec, uint16_t reg, uint16_t *data)
{
	return i2c_addrw_readw(codec->i2c, codec->chip, reg, data);
}

static int rt5682s_i2c_writew(rt5682sCodec *codec, uint16_t reg, uint16_t data)
{
	return i2c_addrw_writew(codec->i2c, codec->chip, reg, data);
}

static int rt5682s_update_bits(rt5682sCodec *codec, uint16_t reg, uint16_t mask,
			       uint16_t value)
{
	uint16_t old, new_value;

	if (rt5682s_i2c_readw(codec, reg, &old))
		return 1;

	new_value = (old & ~mask) | (value & mask);

	if (old != new_value && rt5682s_i2c_writew(codec, reg, new_value))
		return 1;

	return 0;
}

#ifdef DEBUG
static void debug_dump_5682s_regs(rt5682sCodec *codec, int swap)
{
	uint16_t i, reg_word;

	// Show all 16-bit codec regs
	for (i = 0; i < RT5682S_REG_CNT; i++) {
		rt5682s_i2c_readw(codec, i, &reg_word);
		printf("%04X ", reg_word);
	}
	printf("\n");
}
#endif // DEBUG

static int find_pll_inter_combination(unsigned int f_in, unsigned int f_out,
				      struct pll_calc_map *a,
				      struct pll_calc_map *b)
{
	int i, j;

	/* Look at PLLA table */
	for (i = 0; i < ARRAY_SIZE(plla_table); i++) {
		if (plla_table[i].freq_in == f_in &&
		    plla_table[i].freq_out == f_out) {
			memcpy(a, plla_table + i, sizeof(*a));
			return USE_PLLA;
		}
	}

	/* Look at PLLB table */
	for (i = 0; i < ARRAY_SIZE(pllb_table); i++) {
		if (pllb_table[i].freq_in == f_in &&
		    pllb_table[i].freq_out == f_out) {
			memcpy(b, pllb_table + i, sizeof(*b));
			return USE_PLLB;
		}
	}

	/* Find a combination of PLLA & PLLB */
	for (i = ARRAY_SIZE(plla_table) - 1; i >= 0; i--) {
		if (plla_table[i].freq_in == f_in &&
		    plla_table[i].freq_out == 3840000) {
			for (j = ARRAY_SIZE(pllb_table) - 1; j >= 0; j--) {
				if (pllb_table[j].freq_in == 3840000 &&
				    pllb_table[j].freq_out == f_out) {
					memcpy(a, plla_table + i, sizeof(*a));
					memcpy(b, pllb_table + j, sizeof(*b));
					return USE_PLLAB;
				}
			}
		}
	}

	return -1;
}

static int rt5682s_set_clock(rt5682sCodec *codec)
{
	struct pll_calc_map a_map, b_map;
	int pll_comb;

	pll_comb = find_pll_inter_combination(codec->mclk, 24576000, &a_map,
					      &b_map);

	if (pll_comb < 0) {
		printf("Unsupported freq conversion for PLL:(%d->24576000): "
		       "%d\n",
		       codec->mclk, pll_comb);
		return 1;
	}

	if (pll_comb == USE_PLLA || pll_comb == USE_PLLAB) {
		printf("PLLA: fin=%d fout=%d m_bp=%d k_bp=%d m=%d n=%d k=%d\n",
		       a_map.freq_in, a_map.freq_out, a_map.m_bp, a_map.k_bp,
		       (a_map.m_bp ? 0 : a_map.m), a_map.n,
		       (a_map.k_bp ? 0 : a_map.k));
		rt5682s_update_bits(codec, RT5682S_PLL_CTRL_1,
				    RT5682S_PLLA_N_MASK, a_map.n);
		rt5682s_update_bits(codec, RT5682S_PLL_CTRL_2,
				    RT5682S_PLLA_M_MASK | RT5682S_PLLA_K_MASK,
				    a_map.m << RT5682S_PLLA_M_SFT | a_map.k);
		rt5682s_update_bits(
			codec, RT5682S_PLL_CTRL_6,
			RT5682S_PLLA_M_BP_MASK | RT5682S_PLLA_K_BP_MASK,
			a_map.m_bp << RT5682S_PLLA_M_BP_SFT |
				a_map.k_bp << RT5682S_PLLA_K_BP_SFT);
	}

	if (pll_comb == USE_PLLB || pll_comb == USE_PLLAB) {
		printf("PLLB: fin=%d fout=%d m_bp=%d k_bp=%d m=%d n=%d k=%d "
		       "byp_ps=%d sel_ps=%d\n",
		       b_map.freq_in, b_map.freq_out, b_map.m_bp, b_map.k_bp,
		       (b_map.m_bp ? 0 : b_map.m), b_map.n,
		       (b_map.k_bp ? 0 : b_map.k), b_map.byp_ps, b_map.sel_ps);
		rt5682s_update_bits(codec, RT5682S_PLL_CTRL_3,
				    RT5682S_PLLB_N_MASK, b_map.n);
		rt5682s_update_bits(codec, RT5682S_PLL_CTRL_4,
				    RT5682S_PLLB_M_MASK | RT5682S_PLLB_K_MASK,
				    b_map.m << RT5682S_PLLB_M_SFT | b_map.k);
		rt5682s_update_bits(
			codec, RT5682S_PLL_CTRL_6,
			RT5682S_PLLB_SEL_PS_MASK | RT5682S_PLLB_BYP_PS_MASK |
				RT5682S_PLLB_M_BP_MASK | RT5682S_PLLB_K_BP_MASK,
			b_map.sel_ps << RT5682S_PLLB_SEL_PS_SFT |
				b_map.byp_ps << RT5682S_PLLB_BYP_PS_SFT |
				b_map.m_bp << RT5682S_PLLB_M_BP_SFT |
				b_map.k_bp << RT5682S_PLLB_K_BP_SFT);
	}

	if (pll_comb == USE_PLLB)
		rt5682s_update_bits(codec, RT5682S_PLL_CTRL_7,
				    RT5682S_PLLB_SRC_MASK,
				    RT5682S_PLLB_SRC_DFIN);

	/* PLL reset */
	rt5682s_update_bits(codec, RT5682S_PWR_ANLG_3,
			    RT5682S_RSTB_PLLB | RT5682S_RSTB_PLLA,
			    RT5682S_RSTB_PLLB | RT5682S_RSTB_PLLA);

	/* Enable Clocks */
	rt5682s_i2c_writew(codec, RT5682S_GLB_CLK, 0x4000);
	rt5682s_i2c_writew(codec, RT5682S_PWR_DIG_1, 0x80c1);
	rt5682s_update_bits(
		codec, RT5682S_ADDA_CLK_1,
		RT5682S_ADC_OSR_MASK | RT5682S_I2S_M_D_MASK |
			RT5682S_I2S_M_CLK_SRC_MASK | RT5682S_DAC_OSR_MASK,
		RT5682S_ADC_OSR_D_2 | RT5682S_I2S_M_D_12 |
			(RT5682S_CLK_SRC_PLL2 << RT5682S_I2S_M_CLK_SRC_SFT) |
			RT5682S_DAC_OSR_D_2);
	rt5682s_i2c_writew(codec, RT5682S_TDM_TCON_CTRL_1, 0x0001);
	rt5682s_update_bits(codec, RT5682S_PLL_TRACK_2,
			    RT5682S_FILTER_CLK_DIV_MASK,
			    (1 << RT5682S_FILTER_CLK_DIV_SFT));
	rt5682s_update_bits(codec, RT5682S_DMIC_CTRL_1,
			    RT5682S_FIFO_CLK_DIV_MASK, RT5682S_FIFO_CLK_DIV_2);

	return 0;
}

/* Initialize rt5682 codec device */
static int rt5682s_device_init(rt5682sCodec *codec)
{
	uint16_t reg;

	if (rt5682s_i2c_readw(codec, RT5682S_DEVICE_ID, &reg)) {
		printf("%s: Error reading DEVICE ID!\n", __func__);
		return 1;
	}
	if (reg != DEVICE_ID) {
		printf("Device with ID register %x is not rt5682s\n", reg);
		return 1;
	}

	rt5682s_i2c_writew(codec, RT5682S_PWR_ANLG_3, 0xf030);
	rt5682s_i2c_writew(codec, RT5682S_PWR_ANLG_1, 0x2a80);
	mdelay(60);
	rt5682s_i2c_writew(codec, RT5682S_PWR_ANLG_1, 0x3a80);
	rt5682s_i2c_writew(codec, RT5682S_PWR_DIG_1, 0x0001);

	return 0;
}

static int rt5682s_enable(struct SoundRouteComponentOps *me)
{
	int ret;
	rt5682sCodec *codec = container_of(me, rt5682sCodec, component.ops);

	ret = rt5682s_device_init(codec);

#ifdef DEBUG
	/* Dump all regs after init is done */
	printf("%s: POST-INIT REGISTER DUMP!!!!!!!\n", __func__);
	debug_dump_5682s_regs(codec, 0);
	printf("%s: exit w/ret code %d\n", __func__, ret);
#endif

	if (ret)
		return ret;

	ret = rt5682s_set_clock(codec);

	return ret;
}

rt5682sCodec *new_rt5682s_codec(I2cOps *i2c, uint8_t chip, uint32_t mclk,
				uint32_t lrclk)
{
	printf("%s: chip = 0x%02X\n", __func__, chip);

	rt5682sCodec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &rt5682s_enable;

	codec->i2c = i2c;
	codec->chip = chip;
	codec->mclk = mclk;
	codec->lrclk = lrclk;

	return codec;
}
