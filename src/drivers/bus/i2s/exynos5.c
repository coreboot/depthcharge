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

#include "base/container_of.h"
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

typedef struct __attribute__((packed)) Exynos5I2sMultiRegs {
	uint32_t control;
	uint32_t mode;
	uint32_t fifo_control;
	uint32_t prescaler;
	uint32_t tx_data;
	uint32_t rx_data;
	uint32_t fifo_control_s;
	uint32_t tx_data_s;
	uint32_t ahb_dma_control;
	uint32_t ahb_dma_start_0;
	uint32_t ahb_dma_size;
	uint32_t ahb_dma_count;
	uint32_t ahb_dma_int_0;
	uint32_t ahb_dma_int_1;
	uint32_t ahb_dma_int_2;
	uint32_t ahb_dma_int_3;
	uint32_t ahb_dma_start_1;
	uint32_t version;
	uint32_t tx_data_3_count;
	uint32_t tdm_mode_control;
} Exynos5I2sMultiRegs;

static void i2s_transmit_enable(uint32_t *control_ptr, int on)
{
	uint32_t control = readl(control_ptr);
	if (on) {
		control |= CON_ACTIVE;
		control &= ~CON_TXCH_PAUSE;
	} else {
		control |=  CON_TXCH_PAUSE;
		control &= ~CON_ACTIVE;
	}
	writel(control, control_ptr);
}

static void i2s_flush_tx_fifo(uint32_t *fifo_control_ptr)
{
	uint32_t fifo_control = readl(fifo_control_ptr);
	writel(fifo_control | FIC_TXFLUSH, fifo_control_ptr);
	writel(fifo_control & ~FIC_TXFLUSH, fifo_control_ptr);
}

static int i2s_send_generic(uint32_t *data, unsigned int length,
			    uint32_t *tx_data, uint32_t *control)
{
	// Prefill the tx fifo
	for (int i = 0; i < MIN(64, length); i++)
		writel(*data++, tx_data);

	length -= MIN(64, length);
	i2s_transmit_enable(control, 1);

	while (length) {
		if (!(readl(control) & CON_TXFIFO_FULL)) {
			writel(*data++, tx_data);
			length--;
		}
	}
	i2s_transmit_enable(control, 0);

	return 0;
}



static int i2s_init(Exynos5I2s *bus)
{
	Exynos5I2sRegs *regs = bus->regs;

	uint32_t mode = readl(&regs->mode);

	// Set transmit only mode.
	mode &= ~MOD_MASK;

	mode &= ~(MOD_SDF_MASK | MOD_LR_RLOW | MOD_SLAVE);
	mode |= MOD_SDF_IIS;

	// Sets the frame size for I2S LR clock.
	mode &= ~MOD_MULTI_RCLK_MASK;
	switch (bus->lr_frame_size) {
	case 768:
		mode |= MOD_MULTI_RCLK_768FS;
		break;
	case 512:
		mode |= MOD_MULTI_RCLK_512FS;
		break;
	case 384:
		mode |= MOD_MULTI_RCLK_384FS;
		break;
	case 256:
		mode |= MOD_MULTI_RCLK_256FS;
		break;
	default:
		printf("%s: Unrecognized frame size %d.\n", __func__,
			bus->lr_frame_size);
		return 1;
	}

	mode &= ~MOD_BLC_MASK;
	switch (bus->bits_per_sample) {
	case 8:
		mode |= MOD_BLC_8BIT;
		break;
	case 16:
		mode |= MOD_BLC_16BIT;
		break;
	case 24:
		mode |= MOD_BLC_24BIT;
		break;
	default:
		printf("%s: Invalid sample size input %d\n",
			__func__, bus->bits_per_sample);
		return 1;
	}

	// Set the bit clock frame size (in multiples of LRCLK)
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
		printf("%s: Unrecognignized clock frame size %d.\n",
			__func__, bus->bits_per_sample * bus->channels);
		return 1;
	}

	writel(mode, &regs->mode);

	i2s_transmit_enable(&regs->control, 0);
	i2s_flush_tx_fifo(&regs->fifo_control);

	return 0;
}

static int i2s_send(I2sOps *me, uint32_t *data, unsigned int length)
{
	Exynos5I2s *bus = container_of(me, Exynos5I2s, ops);
	Exynos5I2sRegs *regs = bus->regs;

	if (!bus->initialized) {
		if (i2s_init(bus))
			return -1;
		else
			bus->initialized = 1;
	}

	i2s_send_generic(data, length, &regs->tx_data, &regs->control);

	return 0;
}

Exynos5I2s *new_exynos5_i2s(uintptr_t regs, int bits_per_sample,
			    int channels, int lr_frame_size)
{
	Exynos5I2s *bus = xzalloc(sizeof(*bus));
	bus->ops.send = &i2s_send;
	bus->regs = (void *)regs;
	bus->bits_per_sample = bits_per_sample;
	bus->channels = channels;
	bus->lr_frame_size = lr_frame_size;
	return bus;
}



static int i2s_init_multi(Exynos5I2s *bus)
{
	Exynos5I2sMultiRegs *regs = bus->regs;

	// Reset the bus.
	writel(0, &regs->control);
	writel(CON_MULTI_RSTCLR, &regs->control);

	uint32_t mode = MOD_MULTI_OP_CLK_AUDIO | MOD_MULTI_RCLKSRC;

	unsigned int magic_prescaler = 0x3;
	writel(PSR_MULTI_PSREN | (magic_prescaler << 8), &regs->prescaler);

	mode |= MOD_MULTI_SDF_IIS;

	// Sets the frame size for I2S LR clock.
	mode &= ~MOD_MULTI_RCLK_MASK;
	switch (bus->lr_frame_size) {
	case 768:
		mode |= MOD_MULTI_RCLK_768FS;
		break;
	case 512:
		mode |= MOD_MULTI_RCLK_512FS;
		break;
	case 384:
		mode |= MOD_MULTI_RCLK_384FS;
		break;
	case 256:
		mode |= MOD_MULTI_RCLK_256FS;
		break;
	default:
		printf("%s: Unrecognized frame size %d.\n", __func__,
			bus->lr_frame_size);
		return 1;
	}

	switch (bus->bits_per_sample) {
	case 8:
		mode |= MOD_MULTI_BLCP_8BIT;
		mode |= MOD_MULTI_BLC_8BIT;
		break;
	case 16:
		mode |= MOD_MULTI_BLCP_16BIT;
		mode |= MOD_MULTI_BLC_16BIT;
		break;
	case 24:
		mode |= MOD_MULTI_BLCP_24BIT;
		mode |= MOD_MULTI_BLC_24BIT;
		break;
	default:
		printf("%s: Invalid sample size input %d\n",
			__func__, bus->bits_per_sample);
		return 1;
	}

	// Set the bit clock frame size (in multiples of LRCLK)
	switch (bus->bits_per_sample * bus->channels) {
	case 48:
		mode |= MOD_MULTI_BCLK_48FS;
		break;
	case 32:
		mode |= MOD_MULTI_BCLK_32FS;
		break;
	case 24:
		mode |= MOD_MULTI_BCLK_24FS;
		break;
	case 16:
		mode |= MOD_MULTI_BCLK_16FS;
		break;
	default:
		printf("%s: Unrecognignized clock frame size %d.\n",
			__func__, bus->bits_per_sample * bus->channels);
		return 1;
	}

	writel(mode, &regs->mode);

	i2s_transmit_enable(&regs->control, 0);
	i2s_flush_tx_fifo(&regs->fifo_control);

	return 0;
}

static int i2s_send_multi(I2sOps *me, uint32_t *data, unsigned int length)
{
	Exynos5I2s *bus = container_of(me, Exynos5I2s, ops);
	Exynos5I2sMultiRegs *regs = bus->regs;

	if (!bus->initialized) {
		if (i2s_init_multi(bus))
			return -1;
		else
			bus->initialized = 1;
	}

	i2s_send_generic(data, length, &regs->tx_data, &regs->control);

	return 0;
}

Exynos5I2s *new_exynos5_i2s_multi(uintptr_t regs, int bits_per_sample,
				  int channels, int lr_frame_size)
{
	Exynos5I2s *bus = xzalloc(sizeof(*bus));
	bus->ops.send = &i2s_send_multi;
	bus->regs = (void *)regs;
	bus->bits_per_sample = bits_per_sample;
	bus->channels = channels;
	bus->lr_frame_size = lr_frame_size;
	return bus;
}
