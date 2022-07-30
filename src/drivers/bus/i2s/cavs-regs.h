/* SPDX-License-Identifier: GPL-2.0 */
/*
 * Copyright 2020, Intel Corporation.
 * Copyright 2020 Google LLC
 *
 * Common SoC driver for Intel SOC.
 */

#ifndef __DRIVERS_BUS_I2S_CAVS_REGS_H__
#define __DRIVERS_BUS_I2S_CAVS_REGS_H__

/* Applies to all CAVS versions */
#define DSP_POWER_OFFSET	0x04
#define POWER_OFFSET		0x8
#define BAR_OFFSET		0x804
#define ENABLE_ADSP_BAR		0x40000000

#if CONFIG(INTEL_COMMON_I2S_CAVS_1_5)

#define DSP_POWER_ON		0x00030303
#define DSP_POWERED_UP		0x03030303
#define AUDIO_DEV		0x0e
#define AUDIO_FUNC		0
#define SERIAL_CLOCK_RATE	0x7
/* I2S1 register start address. */
#define SSP_I2S1_START_ADDRESS	0x5000
/* I2S5 register start address */
#define SSP_I2S5_START_ADDRESS	0x3000
/* Clock gating register definitions */
#define CLOCK_GATING_OFFSET	0x378
#define DISABLE_CLOCK_GATING	0x0FFC0000
#define DISABLED_CLOCK_GATING	0x0DFC0000

#elif CONFIG(INTEL_COMMON_I2S_CAVS_1_8)

#define DSP_POWER_ON		0x000f0f0f
#define DSP_POWERED_UP		0x0f0f0f0f
#define AUDIO_DEV		0x1f
#define AUDIO_FUNC		3
#define SERIAL_CLOCK_RATE	0x9
/* I2S1 register start address. */
#define	SSP_I2S1_START_ADDRESS	0x11000

#elif CONFIG(INTEL_COMMON_I2S_CAVS_2_0)

#define DSP_POWER_ON		0x00010303
#define DSP_POWERED_UP		0x01010303
#define AUDIO_DEV		0x1f
#define AUDIO_FUNC		3
#define SERIAL_CLOCK_RATE	0x18
/* I2S0 register start address */
#define SSP_I2S0_START_ADDRESS	0x10000
/* I2S1 register start address */
#define SSP_I2S1_START_ADDRESS	0x11000
/* MNCSS - M/N clock synthesizer reg block */
#define MNCSS_REG_BLOCK_START	0x0800
/* Index of respective I2S port to which amp is connected */
#define AMP_SSP_PORT_INDEX	1
/* M register offset address */
#define MDIV_M_VAL(port_id)	(0x100 + 0x8 * (port_id) + 0x0)
/* N register offset address */
#define MDIV_N_VAL(port_id)	(0x100 + 0x8 * (port_id) + 0x4)

#elif CONFIG(INTEL_COMMON_I2S_CAVS_2_5)

#define DSP_POWER_ON		0x00010f0f
#define DSP_POWERED_UP		0x01010f0f
#define AUDIO_DEV		0x1f
#define AUDIO_FUNC		3
/* BITS 8:19 (SCR) of  SSxCO reg is used to configure BCLK */
#if !CONFIG(DRIVER_SOUND_CS35L53)
#define SERIAL_CLOCK_RATE	0x18
#else
#define SERIAL_CLOCK_RATE	0x7
#endif
/* I2S0 register start address */
#define SSP_I2S0_START_ADDRESS	0x10000
/* I2S1 register start address */
#define SSP_I2S1_START_ADDRESS	0x11000
/* I2S2 register start address */
#define SSP_I2S2_START_ADDRESS	0x12000
/* MNCSS - M/N clock synthesizer reg block */
#define MNCSS_REG_BLOCK_START	0x0800
/* I2S Link Control register */
#define I2SLCTL			0x71c04
/* Index of respective I2S port to which amp is connected */
#define AMP_SSP_PORT_INDEX	1
/* M register offset address */
#define MDIV_M_VAL(port_id)	(0x100 + 0x8 * (port_id) + 0x0)
/* N register offset address */
#define MDIV_N_VAL(port_id)	(0x100 + 0x8 * (port_id) + 0x4)

#else /* CONFIG(INTEL_COMMON_I2S_ACE_1_x) */

#define DSP_POWER_ON		0x0
#define DSP_POWERED_UP		0x0
#define AUDIO_DEV		0x1f
#define AUDIO_FUNC		3

/*
 * 38400000 / 16 * 2 * 48000 = 25
 * Serial bit rate = SSP clock / (SCR+1),
 */
#define SERIAL_CLOCK_RATE		0x18
#define SSP_I2Sx_OFFSET(x)		(0x28000 + 0x1000 * (x))
#define SSP_I2S0_START_ADDRESS	SSP_I2Sx_OFFSET(0)
#define SSP_I2S1_START_ADDRESS	SSP_I2Sx_OFFSET(1)
#define SSP_I2S2_START_ADDRESS	SSP_I2Sx_OFFSET(2)
/* 0x00028804[+=0x0] I2S0LCTL[1] default XTAL Oscillator clock 38.4MHz */
#define I2SLCTL			0x28804

#endif /* CONFIG(INTEL_COMMON_I2S_ACE_1_x) */

/* ACE-specific, registers access mem with BAR4 */
#define HFDSSCS			0x1000
#define HFDSSCS_SPA_MASK		BIT(16)
#define HFDSSCS_CPA_MASK		BIT(24)
#define HFPWRCTL			0x1d18
#define HFPWRSTS			0x1d1c
#define HFPWRCTL_WPDSPHPxPG(x)	BIT(x)
#define DSP2C0_CTL			0x178d04
#define DSP2C0_OSEL_SHIFT		24
#define DSP2C0_OSEL_MASK		GENMASK(25, 24)
#define DSP2C0_OSEL_HOST(x)		\
	(((x) & ~DSP2C0_OSEL_MASK) | 0x2 << DSP2C0_OSEL_SHIFT)
#define DSP2C0_CTL_SPA_MASK		BIT(0)
#define DSP2C0_CTL_CPA_MASK		BIT(8)

#endif /* __DRIVERS_BUS_I2S_CAVS_REGS_H__ */
