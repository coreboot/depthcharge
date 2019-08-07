/*
 * rt1011.h -- RealTek ALC1011 ALSA SoC Audio driver
 *
 * Copyright 2017 Realtek Semiconductor Corp. All rights reserved.
 * Author: Shuming Fan <shumingf@realtek.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __DRIVERS_SOUND_RT1011_H__
#define __DRIVERS_SOUND_RT1011_H__

#include "drivers/bus/i2c/i2c.h"
#include "drivers/sound/route.h"
#include "drivers/sound/sound.h"

typedef struct {
	SoundRouteComponent component;
	SoundOps ops;
	I2cOps *i2c;
	uint8_t chip;
} rt1011Codec;

rt1011Codec *new_rt1011_codec(I2cOps *i2c, uint8_t chip);

#define AUD_RT1011_DEVICE_ADDR          0x38

/* rt1011 registers address */
#define RT1011_RESET			0x0000
#define RT1011_CLOCK2			0x0004
#define RT1011_PLL1			0x000A
#define RT1011_CLK_DETECT		0x0020
#define RT1011_RW1			0x00F3
#define RT1011_DAC2			0x0104
#define RT1011_ADC1			0x0108
#define RT1011_ADC                      0x0110
#define RT1011_TDM			0x0111
#define RT1011_TDM_TCON			0x0112
#define RT1011_TDM1			0x0114
#define RT1011_MIXER                    0x0300
#define RT1011_ANALOG_TC_SEQ1		0x0313
#define RT1011_POWER1                   0x0322
#define RT1011_POWER2			0x0324
#define RT1011_POWER3			0x0326
#define RT1011_POWER4			0x0328
#define RT1011_POWER7			0x032B
#define RT1011_POWER8			0x032C
#define RT1011_POWER9			0x032D
#define RT1011_BOOST_CTRL1		0x0330
#define RT1011_BEEP_TONE		0x050C
#define RT1011_SPK_DC_DETECT1		0x0519
#define RT1011_SPK_DC_DETECT2		0x051A
#define RT1011_SPK_DRIVER_STATUS	0x1300
#define RT1011_SINE_GEN_REG1		0x1500
#define RT1011_SINE_GEN_REG2		0x1502
#define RT1011_SINE_GEN_REG3		0x1504

/* rt1011 data values */
#define RT1011_RST			0x0000
#define RT1011_FS_RC_CLK		0xE000
#define RT1011_BYPASS_MODE		0x0802
#define RT1011_DISABLE_BCLK_DETECT	0x0000
#define RT1011_CLK_BOOST_OFF		0xA400
#define RT1011_EN_DAC_CHOPPER		0xA232
#define RT1011_ADC_DIGITAL_VOL		0x2925
#define RT1011_ADC_SRC_CTRL		0x0A20
#define RT1011_SLAVE_24BIT_I2S		0x0240
#define RT1011_BCLK_64FS_LRCLK_DIV_256	0x0440
#define RT1011_LOOPBACK_STEREO		0x0022
#define RT1011_UNMUTE			0x3F1D
#define RT1011_SEL_BSTCAP_FREQ_192KHZ	0x6054
#define RT1011_POW_LDO2_DAC_CLK12M	0xE0A0
#define RT1011_POW_BG_MBIAS_LV		0x0007
#define RT1011_POW_VBAT_MBIAS_LV	0x5FF7
#define RT1011_EN_SWR			0x16F2
#define RT1011_BOOST_SWR_CTRL		0x3E55
#define RT1011_BOOST_SWR_MOS_CTRL	0x0520
#define RT1011_SDB_MODE			0xAA00
#define RT1011_FORCE_BYPASS_BOOST_VOLT	0x8188
#define RT1011_BEEP_TONE_HIGH		0x8000
#define RT1011_BEEP_TONE_LOW		0x0000
#define RT1011_DC_DETECT1_ON		0xB00C
#define RT1011_DC_DETECT_THRESHOLD	0xCCCC
#define RT1011_SPK_DRIVER_READY		0x1701
#define RT1011_SINE_GEN_CTRL_EN		0xA436
#define RT1011_SINE_GEN_FREQ_VOL_SEL	0x00BF
#define RT1011_SINE_GEN_MANUAL_INDEX	0x0200

#endif /* __DRIVERS_SOUND_RT1011_H__ */
