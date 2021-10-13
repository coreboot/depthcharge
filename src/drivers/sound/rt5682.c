/*
 * rt5682.c -- RealTek ALC5682 ALSA SoC Audio driver
 *
 * Copyright 2020 Realtek Semiconductor Corp. All rights reserved.
 * Copyright 2020 Advanced Micro Devices, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <stdbool.h>
#include <libpayload.h>
#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/rt5682.h"

/*
 * RT5682 Registers Definition
 */

#define RT5682_REG_CNT				0x3f3

/* Info */
#define RT5682_RESET				0x0000
#define RT5682_VERSION_ID			0x00fd
#define RT5682_VENDOR_ID			0x00fe
#define RT5682_DEVICE_ID			0x00ff
/*  I/O - Output */
#define RT5682_HP_CTRL_1			0x0002
#define RT5682_HP_CTRL_2			0x0003
#define RT5682_HPL_GAIN				0x0005
#define RT5682_HPR_GAIN				0x0006

#define RT5682_I2C_CTRL				0x0008

/* I/O - Input */
#define RT5682_CBJ_BST_CTRL			0x000b
#define RT5682_CBJ_CTRL_1			0x0010
#define RT5682_CBJ_CTRL_2			0x0011
#define RT5682_CBJ_CTRL_3			0x0012
#define RT5682_CBJ_CTRL_4			0x0013
#define RT5682_CBJ_CTRL_5			0x0014
#define RT5682_CBJ_CTRL_6			0x0015
#define RT5682_CBJ_CTRL_7			0x0016
/* I/O - ADC/DAC/DMIC */
#define RT5682_DAC1_DIG_VOL			0x0019
#define RT5682_STO1_ADC_DIG_VOL			0x001c
#define RT5682_STO1_ADC_BOOST			0x001f
#define RT5682_HP_IMP_GAIN_1			0x0022
#define RT5682_HP_IMP_GAIN_2			0x0023
/* Mixer - D-D */
#define RT5682_SIDETONE_CTRL			0x0024
#define RT5682_STO1_ADC_MIXER			0x0026
#define RT5682_AD_DA_MIXER			0x0029
#define RT5682_STO1_DAC_MIXER			0x002a
#define RT5682_A_DAC1_MUX			0x002b
#define RT5682_DIG_INF2_DATA			0x0030
/* Mixer - ADC */
#define RT5682_REC_MIXER			0x003c
#define RT5682_CAL_REC				0x0044
#define RT5682_ALC_BACK_GAIN			0x0049
/* Power */
#define RT5682_PWR_DIG_1			0x0061
#define RT5682_PWR_DIG_2			0x0062
#define RT5682_PWR_ANLG_1			0x0063
#define RT5682_PWR_ANLG_2			0x0064
#define RT5682_PWR_ANLG_3			0x0065
#define RT5682_PWR_MIXER			0x0066
#define RT5682_PWR_VOL				0x0067
/* Clock Detect */
#define RT5682_CLK_DET				0x006b
/* Filter Auto Reset */
#define RT5682_RESET_LPF_CTRL			0x006c
#define RT5682_RESET_HPF_CTRL			0x006d
/* DMIC */
#define RT5682_DMIC_CTRL_1			0x006e
/* Format - ADC/DAC */
#define RT5682_I2S1_SDP				0x0070
#define RT5682_I2S2_SDP				0x0071
#define RT5682_ADDA_CLK_1			0x0073
#define RT5682_ADDA_CLK_2			0x0074
#define RT5682_I2S1_F_DIV_CTRL_1		0x0075
#define RT5682_I2S1_F_DIV_CTRL_2		0x0076
/* Format - TDM Control */
#define RT5682_TDM_CTRL				0x0079
#define RT5682_TDM_ADDA_CTRL_1			0x007a
#define RT5682_TDM_ADDA_CTRL_2			0x007b
#define RT5682_DATA_SEL_CTRL_1			0x007c
#define RT5682_TDM_TCON_CTRL			0x007e
/* Function - Analog */
#define RT5682_GLB_CLK				0x0080
#define RT5682_PLL_CTRL_1			0x0081
#define RT5682_PLL_CTRL_2			0x0082
#define RT5682_PLL_TRACK_1			0x0083
#define RT5682_PLL_TRACK_2			0x0084
#define RT5682_PLL_TRACK_3			0x0085
#define RT5682_PLL_TRACK_4			0x0086
#define RT5682_PLL_TRACK_5			0x0087
#define RT5682_PLL_TRACK_6			0x0088
#define RT5682_PLL_TRACK_11			0x008c
#define RT5682_SDW_REF_CLK			0x008d
#define RT5682_DEPOP_1				0x008e
#define RT5682_DEPOP_2				0x008f
#define RT5682_HP_CHARGE_PUMP_1			0x0091
#define RT5682_HP_CHARGE_PUMP_2			0x0092
#define RT5682_MICBIAS_1			0x0093
#define RT5682_MICBIAS_2			0x0094
#define RT5682_PLL_TRACK_12			0x0098
#define RT5682_PLL_TRACK_14			0x009a
#define RT5682_PLL2_CTRL_1			0x009b
#define RT5682_PLL2_CTRL_2			0x009c
#define RT5682_PLL2_CTRL_3			0x009d
#define RT5682_PLL2_CTRL_4			0x009e
#define RT5682_RC_CLK_CTRL			0x009f
#define RT5682_I2S_M_CLK_CTRL_1			0x00a0
#define RT5682_I2S2_F_DIV_CTRL_1		0x00a3
#define RT5682_I2S2_F_DIV_CTRL_2		0x00a4
/* Function - Digital */
#define RT5682_EQ_CTRL_1			0x00ae
#define RT5682_EQ_CTRL_2			0x00af
#define RT5682_IRQ_CTRL_1			0x00b6
#define RT5682_IRQ_CTRL_2			0x00b7
#define RT5682_IRQ_CTRL_3			0x00b8
#define RT5682_IRQ_CTRL_4			0x00b9
#define RT5682_INT_ST_1				0x00be
#define RT5682_GPIO_CTRL_1			0x00c0
#define RT5682_GPIO_CTRL_2			0x00c1
#define RT5682_GPIO_CTRL_3			0x00c2
#define RT5682_HP_AMP_DET_CTRL_1		0x00d0
#define RT5682_HP_AMP_DET_CTRL_2		0x00d1
#define RT5682_MID_HP_AMP_DET			0x00d2
#define RT5682_LOW_HP_AMP_DET			0x00d3
#define RT5682_DELAY_BUF_CTRL			0x00d4
#define RT5682_SV_ZCD_1				0x00d9
#define RT5682_SV_ZCD_2				0x00da
#define RT5682_IL_CMD_1				0x00db
#define RT5682_IL_CMD_2				0x00dc
#define RT5682_IL_CMD_3				0x00dd
#define RT5682_IL_CMD_4				0x00de
#define RT5682_IL_CMD_5				0x00df
#define RT5682_IL_CMD_6				0x00e0
#define RT5682_4BTN_IL_CMD_1			0x00e2
#define RT5682_4BTN_IL_CMD_2			0x00e3
#define RT5682_4BTN_IL_CMD_3			0x00e4
#define RT5682_4BTN_IL_CMD_4			0x00e5
#define RT5682_4BTN_IL_CMD_5			0x00e6
#define RT5682_4BTN_IL_CMD_6			0x00e7
#define RT5682_4BTN_IL_CMD_7			0x00e8

#define RT5682_ADC_STO1_HP_CTRL_1		0x00ea
#define RT5682_ADC_STO1_HP_CTRL_2		0x00eb
#define RT5682_AJD1_CTRL			0x00f0
#define RT5682_JD1_THD				0x00f1
#define RT5682_JD2_THD				0x00f2
#define RT5682_JD_CTRL_1			0x00f6
/* General Control */
#define RT5682_DUMMY_1				0x00fa
#define RT5682_DUMMY_2				0x00fb
#define RT5682_DUMMY_3				0x00fc

#define RT5682_DAC_ADC_DIG_VOL1			0x0100
#define RT5682_BIAS_CUR_CTRL_2			0x010b
#define RT5682_BIAS_CUR_CTRL_3			0x010c
#define RT5682_BIAS_CUR_CTRL_4			0x010d
#define RT5682_BIAS_CUR_CTRL_5			0x010e
#define RT5682_BIAS_CUR_CTRL_6			0x010f
#define RT5682_BIAS_CUR_CTRL_7			0x0110
#define RT5682_BIAS_CUR_CTRL_8			0x0111
#define RT5682_BIAS_CUR_CTRL_9			0x0112
#define RT5682_BIAS_CUR_CTRL_10			0x0113
#define RT5682_VREF_REC_OP_FB_CAP_CTRL		0x0117
#define RT5682_CHARGE_PUMP_1			0x0125
#define RT5682_DIG_IN_CTRL_1			0x0132
#define RT5682_PAD_DRIVING_CTRL			0x0136
#define RT5682_SOFT_RAMP_DEPOP			0x0138
#define RT5682_CHOP_DAC				0x013a
#define RT5682_CHOP_ADC				0x013b
#define RT5682_CALIB_ADC_CTRL			0x013c
#define RT5682_VOL_TEST				0x013f
#define RT5682_SPKVDD_DET_STA			0x0142
#define RT5682_TEST_MODE_CTRL_1			0x0145
#define RT5682_TEST_MODE_CTRL_2			0x0146
#define RT5682_TEST_MODE_CTRL_3			0x0147
#define RT5682_TEST_MODE_CTRL_4			0x0148
#define RT5682_TEST_MODE_CTRL_5			0x0149
#define RT5682_PLL1_INTERNAL			0x0150
#define RT5682_PLL2_INTERNAL			0x0156
#define RT5682_STO_NG2_CTRL_1			0x0160
#define RT5682_STO_NG2_CTRL_2			0x0161
#define RT5682_STO_NG2_CTRL_3			0x0162
#define RT5682_STO_NG2_CTRL_4			0x0163
#define RT5682_STO_NG2_CTRL_5			0x0164
#define RT5682_STO_NG2_CTRL_6			0x0165
#define RT5682_STO_NG2_CTRL_7			0x0166
#define RT5682_STO_NG2_CTRL_8			0x0167
#define RT5682_STO_NG2_CTRL_9			0x0168
#define RT5682_STO_NG2_CTRL_10			0x0169
#define RT5682_STO1_DAC_SIL_DET			0x0190
#define RT5682_SIL_PSV_CTRL1			0x0194
#define RT5682_SIL_PSV_CTRL2			0x0195
#define RT5682_SIL_PSV_CTRL3			0x0197
#define RT5682_SIL_PSV_CTRL4			0x0198
#define RT5682_SIL_PSV_CTRL5			0x0199
#define RT5682_HP_IMP_SENS_CTRL_01		0x01af
#define RT5682_HP_IMP_SENS_CTRL_02		0x01b0
#define RT5682_HP_IMP_SENS_CTRL_03		0x01b1
#define RT5682_HP_IMP_SENS_CTRL_04		0x01b2
#define RT5682_HP_IMP_SENS_CTRL_05		0x01b3
#define RT5682_HP_IMP_SENS_CTRL_06		0x01b4
#define RT5682_HP_IMP_SENS_CTRL_07		0x01b5
#define RT5682_HP_IMP_SENS_CTRL_08		0x01b6
#define RT5682_HP_IMP_SENS_CTRL_09		0x01b7
#define RT5682_HP_IMP_SENS_CTRL_10		0x01b8
#define RT5682_HP_IMP_SENS_CTRL_11		0x01b9
#define RT5682_HP_IMP_SENS_CTRL_12		0x01ba
#define RT5682_HP_IMP_SENS_CTRL_13		0x01bb
#define RT5682_HP_IMP_SENS_CTRL_14		0x01bc
#define RT5682_HP_IMP_SENS_CTRL_15		0x01bd
#define RT5682_HP_IMP_SENS_CTRL_16		0x01be
#define RT5682_HP_IMP_SENS_CTRL_17		0x01bf
#define RT5682_HP_IMP_SENS_CTRL_18		0x01c0
#define RT5682_HP_IMP_SENS_CTRL_19		0x01c1
#define RT5682_HP_IMP_SENS_CTRL_20		0x01c2
#define RT5682_HP_IMP_SENS_CTRL_21		0x01c3
#define RT5682_HP_IMP_SENS_CTRL_22		0x01c4
#define RT5682_HP_IMP_SENS_CTRL_23		0x01c5
#define RT5682_HP_IMP_SENS_CTRL_24		0x01c6
#define RT5682_HP_IMP_SENS_CTRL_25		0x01c7
#define RT5682_HP_IMP_SENS_CTRL_26		0x01c8
#define RT5682_HP_IMP_SENS_CTRL_27		0x01c9
#define RT5682_HP_IMP_SENS_CTRL_28		0x01ca
#define RT5682_HP_IMP_SENS_CTRL_29		0x01cb
#define RT5682_HP_IMP_SENS_CTRL_30		0x01cc
#define RT5682_HP_IMP_SENS_CTRL_31		0x01cd
#define RT5682_HP_IMP_SENS_CTRL_32		0x01ce
#define RT5682_HP_IMP_SENS_CTRL_33		0x01cf
#define RT5682_HP_IMP_SENS_CTRL_34		0x01d0
#define RT5682_HP_IMP_SENS_CTRL_35		0x01d1
#define RT5682_HP_IMP_SENS_CTRL_36		0x01d2
#define RT5682_HP_IMP_SENS_CTRL_37		0x01d3
#define RT5682_HP_IMP_SENS_CTRL_38		0x01d4
#define RT5682_HP_IMP_SENS_CTRL_39		0x01d5
#define RT5682_HP_IMP_SENS_CTRL_40		0x01d6
#define RT5682_HP_IMP_SENS_CTRL_41		0x01d7
#define RT5682_HP_IMP_SENS_CTRL_42		0x01d8
#define RT5682_HP_IMP_SENS_CTRL_43		0x01d9
#define RT5682_HP_LOGIC_CTRL_1			0x01da
#define RT5682_HP_LOGIC_CTRL_2			0x01db
#define RT5682_HP_LOGIC_CTRL_3			0x01dc
#define RT5682_HP_CALIB_CTRL_1			0x01de
#define RT5682_HP_CALIB_CTRL_2			0x01df
#define RT5682_HP_CALIB_CTRL_3			0x01e0
#define RT5682_HP_CALIB_CTRL_4			0x01e1
#define RT5682_HP_CALIB_CTRL_5			0x01e2
#define RT5682_HP_CALIB_CTRL_6			0x01e3
#define RT5682_HP_CALIB_CTRL_7			0x01e4
#define RT5682_HP_CALIB_CTRL_9			0x01e6
#define RT5682_HP_CALIB_CTRL_10			0x01e7
#define RT5682_HP_CALIB_CTRL_11			0x01e8
#define RT5682_HP_CALIB_STA_1			0x01ea
#define RT5682_HP_CALIB_STA_2			0x01eb
#define RT5682_HP_CALIB_STA_3			0x01ec
#define RT5682_HP_CALIB_STA_4			0x01ed
#define RT5682_HP_CALIB_STA_5			0x01ee
#define RT5682_HP_CALIB_STA_6			0x01ef
#define RT5682_HP_CALIB_STA_7			0x01f0
#define RT5682_HP_CALIB_STA_8			0x01f1
#define RT5682_HP_CALIB_STA_9			0x01f2
#define RT5682_HP_CALIB_STA_10			0x01f3
#define RT5682_HP_CALIB_STA_11			0x01f4
#define RT5682_SAR_IL_CMD_1			0x0210
#define RT5682_SAR_IL_CMD_2			0x0211
#define RT5682_SAR_IL_CMD_3			0x0212
#define RT5682_SAR_IL_CMD_4			0x0213
#define RT5682_SAR_IL_CMD_5			0x0214
#define RT5682_SAR_IL_CMD_6			0x0215
#define RT5682_SAR_IL_CMD_7			0x0216
#define RT5682_SAR_IL_CMD_8			0x0217
#define RT5682_SAR_IL_CMD_9			0x0218
#define RT5682_SAR_IL_CMD_10			0x0219
#define RT5682_SAR_IL_CMD_11			0x021a
#define RT5682_SAR_IL_CMD_12			0x021b
#define RT5682_SAR_IL_CMD_13			0x021c
#define RT5682_EFUSE_CTRL_1			0x0250
#define RT5682_EFUSE_CTRL_2			0x0251
#define RT5682_EFUSE_CTRL_3			0x0252
#define RT5682_EFUSE_CTRL_4			0x0253
#define RT5682_EFUSE_CTRL_5			0x0254
#define RT5682_EFUSE_CTRL_6			0x0255
#define RT5682_EFUSE_CTRL_7			0x0256
#define RT5682_EFUSE_CTRL_8			0x0257
#define RT5682_EFUSE_CTRL_9			0x0258
#define RT5682_EFUSE_CTRL_10			0x0259
#define RT5682_EFUSE_CTRL_11			0x025a
#define RT5682_JD_TOP_VC_VTRL			0x0270
#define RT5682_DRC1_CTRL_0			0x02ff
#define RT5682_DRC1_CTRL_1			0x0300
#define RT5682_DRC1_CTRL_2			0x0301
#define RT5682_DRC1_CTRL_3			0x0302
#define RT5682_DRC1_CTRL_4			0x0303
#define RT5682_DRC1_CTRL_5			0x0304
#define RT5682_DRC1_CTRL_6			0x0305
#define RT5682_DRC1_HARD_LMT_CTRL_1		0x0306
#define RT5682_DRC1_HARD_LMT_CTRL_2		0x0307
#define RT5682_DRC1_PRIV_1			0x0310
#define RT5682_DRC1_PRIV_2			0x0311
#define RT5682_DRC1_PRIV_3			0x0312
#define RT5682_DRC1_PRIV_4			0x0313
#define RT5682_DRC1_PRIV_5			0x0314
#define RT5682_DRC1_PRIV_6			0x0315
#define RT5682_DRC1_PRIV_7			0x0316
#define RT5682_DRC1_PRIV_8			0x0317
#define RT5682_EQ_AUTO_RCV_CTRL1		0x03c0
#define RT5682_EQ_AUTO_RCV_CTRL2		0x03c1
#define RT5682_EQ_AUTO_RCV_CTRL3		0x03c2
#define RT5682_EQ_AUTO_RCV_CTRL4		0x03c3
#define RT5682_EQ_AUTO_RCV_CTRL5		0x03c4
#define RT5682_EQ_AUTO_RCV_CTRL6		0x03c5
#define RT5682_EQ_AUTO_RCV_CTRL7		0x03c6
#define RT5682_EQ_AUTO_RCV_CTRL8		0x03c7
#define RT5682_EQ_AUTO_RCV_CTRL9		0x03c8
#define RT5682_EQ_AUTO_RCV_CTRL10		0x03c9
#define RT5682_EQ_AUTO_RCV_CTRL11		0x03ca
#define RT5682_EQ_AUTO_RCV_CTRL12		0x03cb
#define RT5682_EQ_AUTO_RCV_CTRL13		0x03cc
#define RT5682_ADC_L_EQ_LPF1_A1			0x03d0
#define RT5682_R_EQ_LPF1_A1			0x03d1
#define RT5682_L_EQ_LPF1_H0			0x03d2
#define RT5682_R_EQ_LPF1_H0			0x03d3
#define RT5682_L_EQ_BPF1_A1			0x03d4
#define RT5682_R_EQ_BPF1_A1			0x03d5
#define RT5682_L_EQ_BPF1_A2			0x03d6
#define RT5682_R_EQ_BPF1_A2			0x03d7
#define RT5682_L_EQ_BPF1_H0			0x03d8
#define RT5682_R_EQ_BPF1_H0			0x03d9
#define RT5682_L_EQ_BPF2_A1			0x03da
#define RT5682_R_EQ_BPF2_A1			0x03db
#define RT5682_L_EQ_BPF2_A2			0x03dc
#define RT5682_R_EQ_BPF2_A2			0x03dd
#define RT5682_L_EQ_BPF2_H0			0x03de
#define RT5682_R_EQ_BPF2_H0			0x03df
#define RT5682_L_EQ_BPF3_A1			0x03e0
#define RT5682_R_EQ_BPF3_A1			0x03e1
#define RT5682_L_EQ_BPF3_A2			0x03e2
#define RT5682_R_EQ_BPF3_A2			0x03e3
#define RT5682_L_EQ_BPF3_H0			0x03e4
#define RT5682_R_EQ_BPF3_H0			0x03e5
#define RT5682_L_EQ_BPF4_A1			0x03e6
#define RT5682_R_EQ_BPF4_A1			0x03e7
#define RT5682_L_EQ_BPF4_A2			0x03e8
#define RT5682_R_EQ_BPF4_A2			0x03e9
#define RT5682_L_EQ_BPF4_H0			0x03ea
#define RT5682_R_EQ_BPF4_H0			0x03eb
#define RT5682_L_EQ_HPF1_A1			0x03ec
#define RT5682_R_EQ_HPF1_A1			0x03ed
#define RT5682_L_EQ_HPF1_H0			0x03ee
#define RT5682_R_EQ_HPF1_H0			0x03ef
#define RT5682_L_EQ_PRE_VOL			0x03f0
#define RT5682_R_EQ_PRE_VOL			0x03f1
#define RT5682_L_EQ_POST_VOL			0x03f2
#define RT5682_R_EQ_POST_VOL			0x03f3
#define RT5682_I2C_MODE				0xffff

/* ADC/DAC Clock Control 1 (0x0073) */
#define RT5682_I2S_M_DIV_MASK                  (0xf << 8)
#define RT5682_I2S_M_DIV_SFT                   8
#define RT5682_I2S_M_D_1                       (0x0 << 8)
#define RT5682_I2S_M_D_2                       (0x1 << 8)
#define RT5682_I2S_M_D_3                       (0x2 << 8)
#define RT5682_I2S_M_D_4                       (0x3 << 8)
#define RT5682_I2S_M_D_6                       (0x4 << 8)
#define RT5682_I2S_M_D_8                       (0x5 << 8)
#define RT5682_I2S_M_D_12                      (0x6 << 8)
#define RT5682_I2S_M_D_16                      (0x7 << 8)
#define RT5682_I2S_M_D_24                      (0x8 << 8)
#define RT5682_I2S_M_D_32                      (0x9 << 8)
#define RT5682_I2S_M_D_48                      (0x10 << 8)
#define RT5682_I2S_CLK_SRC_MASK                (0x7 << 4)
#define RT5682_I2S_CLK_SRC_SFT                 4
#define RT5682_I2S_CLK_SRC_MCLK                (0x0 << 4)
#define RT5682_I2S_CLK_SRC_PLL1                (0x1 << 4)
#define RT5682_I2S_CLK_SRC_PLL2                (0x2 << 4)
#define RT5682_I2S_CLK_SRC_SDW                 (0x3 << 4)
#define RT5682_I2S_CLK_SRC_RCCLK               (0x4 << 4) /* 25M */
#define RT5682_DAC_OSR_MASK                    (0xf << 0)
#define RT5682_DAC_OSR_SFT                     0
#define RT5682_DAC_OSR_D_1                     (0x0 << 0)
#define RT5682_DAC_OSR_D_2                     (0x1 << 0)
#define RT5682_DAC_OSR_D_4                     (0x2 << 0)
#define RT5682_DAC_OSR_D_6                     (0x3 << 0)
#define RT5682_DAC_OSR_D_8                     (0x4 << 0)
#define RT5682_DAC_OSR_D_12                    (0x5 << 0)
#define RT5682_DAC_OSR_D_16                    (0x6 << 0)
#define RT5682_DAC_OSR_D_24                    (0x7 << 0)
#define RT5682_DAC_OSR_D_32                    (0x8 << 0)
#define RT5682_DAC_OSR_D_48                    (0x9 << 0)

/* PLL2 M/N/K Code Control 1 (0x009b) */
#define RT5682_PLL2F_K_MASK			(0x1f << 8)
#define RT5682_PLL2F_K_SFT			8
#define RT5682_PLL2B_K_MASK			(0xf << 4)
#define RT5682_PLL2B_K_SFT			4
#define RT5682_PLL2B_M_MASK			(0xf << 0)

/* PLL2 M/N/K Code Control 2 (0x009c) */
#define RT5682_PLL2F_M_MASK			(0x3f << 8)
#define RT5682_PLL2F_M_SFT			8
#define RT5682_PLL2B_N_MASK			(0x3f << 0)

/* PLL2 M/N/K Code Control 3 (0x009d) */
#define RT5682_PLL2F_N_MASK			(0x7f << 8)
#define RT5682_PLL2F_N_SFT			8

/* PLL2 M/N/K Code Control 4 (0x009e) */
#define RT5682_PLL2B_M_BP_MASK			(0x1 << 11)
#define RT5682_PLL2B_M_BP_SFT			11
#define RT5682_PLL2F_M_BP_MASK			(0x1 << 7)
#define RT5682_PLL2F_M_BP_SFT			7

#define DEVICE_ID 0x6530

struct rt5682_pll_code {
	unsigned int pll_in;
	unsigned int pll_out;
	int k;
	int n;
	int m;
	bool m_bp; /* Indicates bypass m code or not. */
};

static const struct rt5682_pll_code pll_preset_table[] = {
	{48000000,  3840000,  23,  2, 0, false},
	{19200000,  4096000,  23, 14, 1, false},
	{19200000,  24576000,  3, 30, 3, false},
	{3840000,   24576000,  3, 30, 0, true},
	{3840000,   8192000,   3, 30, 1, false},
	{3840000,   4096000,   8, 30, 1, false},
};

/* RT5682 has 256 16-bit register addresses, and 16-bit register data */
static int rt5682_i2c_readw(rt5682Codec *codec, uint16_t reg, uint16_t *data)
{
	return i2c_addrw_readw(codec->i2c, codec->chip, reg, data);
}

static int rt5682_i2c_writew(rt5682Codec *codec, uint16_t reg, uint16_t data)
{
	return i2c_addrw_writew(codec->i2c, codec->chip, reg, data);
}

static int rt5682_update_bits(rt5682Codec *codec, uint16_t reg,
			      uint16_t mask, uint16_t value)
{
	uint16_t old, new_value;

	if (rt5682_i2c_readw(codec, reg, &old))
		return 1;

	new_value = (old & ~mask) | (value & mask);

	if (old != new_value && rt5682_i2c_writew(codec, reg, new_value))
		return 1;

	return 0;
}

#ifdef DEBUG
static void debug_dump_5682_regs(rt5682Codec *codec, int swap)
{
	uint16_t i, reg_word;

	// Show all 16-bit codec regs
	for (i = 0; i < RT5682_REG_CNT; i++) {
		rt5682_i2c_readw(codec, i, &reg_word);
		printf("%04X ", reg_word);
	}
	printf("\n");

}
#endif	//DEBUG

static const struct rt5682_pll_code *rt5682_pll_calc(uint32_t freq_in,
						     uint32_t freq_out)
{
	for (int i = 0; i < ARRAY_SIZE(pll_preset_table); i++) {
		if (freq_in == pll_preset_table[i].pll_in &&
		    freq_out == pll_preset_table[i].pll_out) {
			return &pll_preset_table[i];
                }
        }
	return NULL;
}

static int rt5682_set_clock(rt5682Codec *codec)
{
	const struct rt5682_pll_code *pll2f_code, *pll2b_code;
	uint16_t i2s1_master_div_val;

	/* PLL2 comprises of 2 PLLs, we keep front PLL output at 3.84MHz */
	pll2f_code = rt5682_pll_calc(codec->mclk, 3840000);
	if (!pll2f_code) {
		printf("%s: Error! mclk=%d not supported\n", __func__,
		       codec->mclk);
		return 1;
	}
	pll2b_code = rt5682_pll_calc(3840000, 24576000);
	if (!pll2b_code) {
		printf("%s: Error! sysclk=24576000 not supported\n", __func__);
		return 1;
	}

	switch(codec->lrclk) {
	case 8000:
		i2s1_master_div_val = RT5682_I2S_M_D_12;
		break;
	case 16000:
		i2s1_master_div_val = RT5682_I2S_M_D_6;
		break;
	case 48000:
	default:
		i2s1_master_div_val = RT5682_I2S_M_D_2;
		break;
	}

	/* Configure PLL2 */
	rt5682_i2c_writew(codec, RT5682_PLL2_CTRL_1,
		pll2f_code->k << RT5682_PLL2F_K_SFT |
		pll2b_code->k << RT5682_PLL2B_K_SFT |
		pll2b_code->m);
	rt5682_i2c_writew(codec, RT5682_PLL2_CTRL_2,
		pll2f_code->m << RT5682_PLL2F_M_SFT |
		pll2b_code->n);
	rt5682_i2c_writew(codec, RT5682_PLL2_CTRL_3,
		pll2f_code->n << RT5682_PLL2F_N_SFT);
	rt5682_update_bits(codec, RT5682_PLL2_CTRL_4,
			RT5682_PLL2B_M_BP_MASK | RT5682_PLL2F_M_BP_MASK | 0xf,
			(pll2b_code->m_bp ? 1 : 0) << RT5682_PLL2B_M_BP_SFT |
			(pll2f_code->m_bp ? 1 : 0) << RT5682_PLL2F_M_BP_SFT |
			0xf);

	/* Enable Clocks */
	rt5682_i2c_writew(codec, RT5682_GLB_CLK, 0x4000);
	rt5682_update_bits(codec, RT5682_ADDA_CLK_1,
			RT5682_I2S_M_DIV_MASK | RT5682_I2S_CLK_SRC_MASK |
			RT5682_DAC_OSR_MASK,
			i2s1_master_div_val | RT5682_I2S_CLK_SRC_PLL2);
	rt5682_i2c_writew(codec, RT5682_TDM_TCON_CTRL, 0x0001);
	rt5682_update_bits(codec, RT5682_ADDA_CLK_1,
			RT5682_DAC_OSR_MASK, RT5682_DAC_OSR_D_2);
	rt5682_i2c_writew(codec, RT5682_PLL_TRACK_2, 0x0100);
	rt5682_i2c_writew(codec, RT5682_DMIC_CTRL_1, 0x1A10);

	return 0;
}

/* Initialize rt5682 codec device */
static int rt5682_device_init(rt5682Codec *codec)
{
	uint16_t reg;


	if (rt5682_i2c_writew(codec, RT5682_I2C_MODE, 0x01)) {
		printf("%s: Error writing I2C mode!\n", __func__);
		return 1;
	}

	udelay(15000);

	if (rt5682_i2c_readw(codec, RT5682_DEVICE_ID, &reg)) {
		printf("%s: Error reading DEVICE ID!\n", __func__);
		return 1;
	}
	if (reg != DEVICE_ID) {
		printf("Device with ID register %x is not rt5682\n", reg);
		return 1;
	}

	rt5682_i2c_writew(codec, RT5682_PLL2_INTERNAL, 0x8266);
	rt5682_i2c_writew(codec, RT5682_PWR_ANLG_3, 0x0030);
	rt5682_i2c_writew(codec, RT5682_PWR_ANLG_1, 0x22ac);
	mdelay(60);
	rt5682_i2c_writew(codec, RT5682_PWR_ANLG_1, 0x32ac);
	rt5682_i2c_writew(codec, RT5682_PWR_DIG_1, 0x8001);

	return 0;
}

static int rt5682_enable(struct SoundRouteComponentOps *me)
{
	int ret;
	rt5682Codec *codec = container_of(me, rt5682Codec, component.ops);

	ret = rt5682_device_init(codec);

#ifdef DEBUG
	/* Dump all regs after init is done */
	printf("%s: POST-INIT REGISTER DUMP!!!!!!!\n", __func__);
	debug_dump_5682_regs(codec, 0);

	printf("%s: exit w/ret code %d\n", __func__, ret);
#endif

	if (ret)
		return ret;

	ret = rt5682_set_clock(codec);

	return ret;
}

rt5682Codec *new_rt5682_codec(I2cOps *i2c, uint8_t chip, uint32_t mclk,
			      uint32_t lrclk)
{
	printf("%s: chip = 0x%02X\n", __func__, chip);

	rt5682Codec *codec = xzalloc(sizeof(*codec));

	codec->component.ops.enable = &rt5682_enable;

	codec->i2c = i2c;
	codec->chip = chip;
	codec->mclk = mclk;
	codec->lrclk = lrclk;

	return codec;
}
