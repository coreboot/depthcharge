/*
 * SPDX-License-Identifier: GPL-2.0
 * Copyright (c) 2020, Intel Corporation.
 * Copyright 2020 Google LLC.
 */

#ifndef __DRIVERS_BUS_I2S_CAVS_2_5_REGS_H__
#define __DRIVERS_BUS_I2S_CAVS_2_5_REGS_H__

#define DSP_POWER_OFFSET	0x04
#define DSP_POWER_ON		0x00010f0f
#define DSP_POWERED_UP		0x01010f0f
#define AUDIO_DEV		0x1f
#define AUDIO_FUNC		3
#define POWER_OFFSET		0x8
#define BAR_OFFSET		0x804
#define ENABLE_ADSP_BAR		0x40000000

/* BITS 8:19 (SCR) of  SSxCO reg is used to configure BCLK */
#define SERIAL_CLOCK_RATE	0x18

/* I2S1 register start address */
#define SSP_I2S1_START_ADDRESS	0x11000

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

#endif /* __DRIVERS_BUS_I2S_CAVS_2_5_REGS_H__ */
