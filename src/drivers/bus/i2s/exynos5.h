/*
 * This file is part of the depthcharge project.
 *
 * Copyright 2012 Google Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA, 02110-1301 USA
 */

#ifndef __DRIVERS_BUS_I2S_EXYNOS5_H__
#define __DRIVERS_BUS_I2S_EXYNOS5_H__

/*
 * DAI hardware audio formats.
 *
 * Describes the physical PCM data formating and clocking.
 */
#define SND_SOC_DAIFMT_I2S		1 // I2S mode
#define SND_SOC_DAIFMT_RIGHT_J		2 // Right Justified mode
#define SND_SOC_DAIFMT_LEFT_J		3 // Left Justified mode
#define SND_SOC_DAIFMT_DSP_A		4 // L data MSB after FRM LRC
#define SND_SOC_DAIFMT_DSP_B		5 // L data MSB during FRM LRC
#define SND_SOC_DAIFMT_AC97		6 // AC97
#define SND_SOC_DAIFMT_PDM		7 // Pulse density modulation

// Left and right justified also known as MSB and LSB respectively.
#define SND_SOC_DAIFMT_MSB		SND_SOC_DAIFMT_LEFT_J
#define SND_SOC_DAIFMT_LSB		SND_SOC_DAIFMT_RIGHT_J

/*
 * DAI hardware signal inversions.
 *
 * Specifies whether the DAI can also support inverted clocks for the
 * specified format.
 */
#define SND_SOC_DAIFMT_NB_NF	(1 << 8) // normal bit clock + frame
#define SND_SOC_DAIFMT_NB_IF	(2 << 8) // normal BCLK + inv FRM
#define SND_SOC_DAIFMT_IB_NF	(3 << 8) // invert BCLK + nor FRM
#define SND_SOC_DAIFMT_IB_IF	(4 << 8) // invert BCLK + FRM

/*
 * DAI hardware clock masters.
 *
 * This is wrt the codec, the inverse is true for the interface
 * i.e. if the codec is clk and FRM master then the interface is
 * clk and frame slave.
 */
#define SND_SOC_DAIFMT_CBM_CFM	(1 << 12) // codec clk & FRM master
#define SND_SOC_DAIFMT_CBS_CFM	(2 << 12) // codec clk slave & FRM master
#define SND_SOC_DAIFMT_CBM_CFS	(3 << 12) // codec clk master & frame slave
#define SND_SOC_DAIFMT_CBS_CFS	(4 << 12) // codec clk & FRM slave

#define SND_SOC_DAIFMT_FORMAT_MASK	0x000f
#define SND_SOC_DAIFMT_CLOCK_MASK	0x00f0
#define SND_SOC_DAIFMT_INV_MASK		0x0f00
#define SND_SOC_DAIFMT_MASTER_MASK	0xf000

// Master clock directions
#define SND_SOC_CLOCK_IN		0
#define SND_SOC_CLOCK_OUT		1

#endif /* __DRIVERS_BUS_I2S_EXYNOS5_H__ */
