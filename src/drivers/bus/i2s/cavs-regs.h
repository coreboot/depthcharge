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

#else /* CONFIG(INTEL_COMMON_I2S_CAVS_2_5) */

#define DSP_POWER_ON		0x00010f0f
#define DSP_POWERED_UP		0x01010f0f
#define AUDIO_DEV		0x1f
#define AUDIO_FUNC		3
/* BITS 8:19 (SCR) of  SSxCO reg is used to configure BCLK */
#define SERIAL_CLOCK_RATE	0x18
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

#endif /* CONFIG(INTEL_COMMON_I2S_CAVS_2_5) */

#endif /* __DRIVERS_BUS_I2S_CAVS_REGS_H__ */
