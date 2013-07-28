/*
 * Copyright 2013 Google Inc.  All rights reserved.
 * Copyright (C) 2012 Samsung Electronics
 * R. Chandrasekar <rcsekar@samsung.com>
 *
 * See file CREDITS for list of people who contributed to this
 * project.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "base/list.h"
#include "drivers/bus/i2s/exynos5.h"
#include "drivers/bus/i2s/exynos5-regs.h"
#include "drivers/bus/i2s/i2s.h"

typedef struct __attribute__((packed)) Exynos5I2sRegs {
	uint32_t control;
	uint32_t mode;
	uint32_t fifo_control;
	uint32_t reserved;
	uint32_t tx_data;
	uint32_t rx_data;
} Exynos5I2sRegs;

// Sets the frame size for I2S LR clock.
static void i2s_set_lr_frame_size(Exynos5I2s *bus)
{
	Exynos5I2sRegs *regs = bus->regs;
	uint32_t mode = readl(&regs->mode);

	mode &= ~MOD_RCLK_MASK;

	switch (bus->lr_frame_size) {
	case 768:
		mode |= MOD_RCLK_768FS;
		break;
	case 512:
		mode |= MOD_RCLK_512FS;
		break;
	case 384:
		mode |= MOD_RCLK_384FS;
		break;
	default:
		mode |= MOD_RCLK_256FS;
		break;
	}

	writel(mode, &regs->mode);
}

static void i2s_transmit_enable(Exynos5I2sRegs *regs, int on)
{
	// Set transmit only mode.
	writel(readl(&regs->mode) & ~MOD_MASK, &regs->mode);

	uint32_t control = readl(&regs->control);
	if (on) {
		control |= CON_ACTIVE;
		control &= ~CON_TXCH_PAUSE;
	} else {
		control |=  CON_TXCH_PAUSE;
		control &= ~CON_ACTIVE;
	}
	writel(control, &regs->control);
}

// Set the bit clock frame size (in multiples of LRCLK)
static void i2s_set_bit_clock_frame_size(Exynos5I2s *bus)
{
	Exynos5I2sRegs *regs = bus->regs;
	uint32_t mode = readl(&regs->mode);

	mode &= ~MOD_BCLK_MASK;
	switch (bus->bits_per_sample * bus->channels) {
	case 48:
		mode |= MOD_BCLK_48FS;
		break;
	case 32:
		mode |= MOD_BCLK_32FS;
		break;
	case 24:
		mode |= MOD_BCLK_24FS;
		break;
	case 16:
		mode |= MOD_BCLK_16FS;
		break;
	default:
		return;
	}

	writel(mode, &regs->mode);
}

static void i2s_flush_tx_fifo(Exynos5I2sRegs *regs)
{
	uint32_t fifo_control = readl(&regs->fifo_control);
	writel(fifo_control | FIC_TXFLUSH, &regs->fifo_control);
	writel(fifo_control & ~FIC_TXFLUSH, &regs->fifo_control);
}

static void i2s_set_system_clock_dir(Exynos5I2sRegs *regs, int dir)
{
	uint32_t mode = readl(&regs->mode);

	if (dir == SND_SOC_CLOCK_IN)
		mode |= MOD_CDCLKCON;
	else
		mode &= ~MOD_CDCLKCON;

	writel(mode, &regs->mode);
}

static int i2s_set_clock_format(Exynos5I2sRegs *regs, unsigned int format)
{
	uint32_t temp = 0;

	/* Format is priority */
	switch (format & SND_SOC_DAIFMT_FORMAT_MASK) {
	case SND_SOC_DAIFMT_RIGHT_J:
		temp |= (MOD_LR_RLOW | MOD_SDF_MSB);
		break;
	case SND_SOC_DAIFMT_LEFT_J:
		temp |= (MOD_LR_RLOW | MOD_SDF_LSB);
		break;
	case SND_SOC_DAIFMT_I2S:
		temp |= MOD_SDF_IIS;
		break;
	default:
		printf("%s: Invalid format priority [0x%x]\n", __func__,
			(format & SND_SOC_DAIFMT_FORMAT_MASK));
		return 1;
	}

	// INV flag is relative to the FORMAT flag - if set it simply
	// flips the polarity specified by the standard.
	switch (format & SND_SOC_DAIFMT_INV_MASK) {
	case SND_SOC_DAIFMT_NB_NF:
		break;
	case SND_SOC_DAIFMT_NB_IF:
		temp ^= MOD_LR_RLOW;
		break;
	default:
		printf("%s: Invalid clock polarity input [0x%x]\n", __func__,
			(format & SND_SOC_DAIFMT_INV_MASK));
		return 1;
	}

	switch (format & SND_SOC_DAIFMT_MASTER_MASK) {
	case SND_SOC_DAIFMT_CBS_CFS:
		temp |= MOD_SLAVE;
		break;
	case SND_SOC_DAIFMT_CBM_CFM:
		// Set default source clock in Master mode.
		i2s_set_system_clock_dir(regs, SND_SOC_CLOCK_OUT);
		break;
	default:
		printf("%s: Invalid master selection [0x%x]\n", __func__,
			(format & SND_SOC_DAIFMT_MASTER_MASK));
		return 1;
	}

	uint32_t mode = readl(&regs->mode);
	mode &= ~(MOD_SDF_MASK | MOD_LR_RLOW | MOD_SLAVE);
	writel(mode | temp, &regs->mode);

	return 0;
}

static int i2s_set_samplesize(Exynos5I2s *bus)
{
	Exynos5I2sRegs *regs = bus->regs;
	uint32_t mode = readl(&regs->mode);

	mode &= ~(MOD_BLCP_MASK | MOD_BLC_MASK);
	switch (bus->bits_per_sample) {
	case 8:
		mode |= MOD_BLCP_8BIT;
		mode |= MOD_BLC_8BIT;
		break;
	case 16:
		mode |= MOD_BLCP_16BIT;
		mode |= MOD_BLC_16BIT;
		break;
	case 24:
		mode |= MOD_BLCP_24BIT;
		mode |= MOD_BLC_24BIT;
		break;
	default:
		printf("%s: Invalid sample size input [0x%x]\n",
			__func__, bus->bits_per_sample);
		return 1;
	}

	writel(mode, &regs->mode);
	return 0;
}

static int i2s_transfer_init(Exynos5I2s *bus)
{
	Exynos5I2sRegs *regs = bus->regs;

	// Configure I2s format.
	if (i2s_set_clock_format(regs, SND_SOC_DAIFMT_I2S |
				       SND_SOC_DAIFMT_NB_NF |
				       SND_SOC_DAIFMT_CBM_CFM)) {
		printf("%s: failed.\n", __func__);
		return 1;
	}

	i2s_set_lr_frame_size(bus);
	if (i2s_set_samplesize(bus)) {
		printf("%s:set sample rate failed\n", __func__);
		return 1;
	}

	i2s_set_bit_clock_frame_size(bus);

	i2s_transmit_enable(regs, 0);
	i2s_flush_tx_fifo(regs);

	return 0;
}

static int i2s_send(I2sOps *me, uint32_t *data, unsigned int length)
{
	Exynos5I2s *bus = container_of(me, Exynos5I2s, ops);
	Exynos5I2sRegs *regs = bus->regs;

	if (!bus->initialized) {
		if (i2s_transfer_init(bus))
			return -1;
		else
			bus->initialized = 1;
	}

	// Prefill the tx fifo
	for (int i = 0; i < MIN(64, length); i++)
		writel(*data++, &regs->tx_data);

	length -= MIN(64, length);
	i2s_transmit_enable(regs, 1);

	while (length) {
		if (!(readl(&regs->control) & CON_TXFIFO_FULL)) {
			writel(*data++, &regs->tx_data);
			length--;
		}
	}
	i2s_transmit_enable(regs, 0);

	return 0;
}

Exynos5I2s *new_exynos5_i2s(void *regs, int bits_per_sample, int channels,
			    int lr_frame_size)
{
	Exynos5I2s *bus = malloc(sizeof(*bus));
	if (!bus) {
		printf("Failed to allocate Exynos5 I2S structure.\n");
		return NULL;
	}
	memset(bus, 0, sizeof(*bus));
	bus->ops.send = &i2s_send;
	bus->regs = regs;
	bus->bits_per_sample = bits_per_sample;
	bus->channels = channels;
	bus->lr_frame_size = lr_frame_size;
	return bus;
}
