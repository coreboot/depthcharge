/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 * Copyright 2013 Google Inc.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * Portions Copyright (C) 2012 Samsung Electronics
 * R. Chandrasekar <rcsekar@samsung.com>
 *
 * This is the Tegra30/Tegra1x4 I2S driver.
 * I2S on Tegra30+ connects to the AHUB (Audio Hub) on one end, and the
 * audio codec (RealTek, Maxim, Wolfson, etc.) on the other.
 * The I2S driver will send an audio stream (PCM, 16-bit stereo) to the codec.
 */

#include <libpayload.h>

#include "base/container_of.h"
#include "drivers/bus/i2s/i2s.h"
#include "drivers/bus/i2s/tegra.h"
#include "drivers/bus/i2s/tegra-regs.h"

static void tegra_i2s_transmit_enable(TegraI2sRegs *regs, int on)
{
	uint32_t ctrl = readl(&regs->ctrl);
	if (on)
		ctrl |= I2S_CTRL_XFER_EN_TX;
	else
		ctrl &= ~I2S_CTRL_XFER_EN_TX;
	writel(ctrl, &regs->ctrl);
}

static int tegra_i2s_init(TegraI2s *bus)
{
	uint32_t audio_bits = (bus->bits_per_sample >> 2) - 1;
	uint32_t ctrl = readl(&bus->regs->ctrl);

	// Set format to LRCK / Left Low.
	ctrl &= ~(I2S_CTRL_FRAME_FORMAT_MASK | I2S_CTRL_LRCK_MASK);
	ctrl |= I2S_CTRL_FRAME_FORMAT_LRCK;
	ctrl |= I2S_CTRL_LRCK_L_LOW;

	// Disable all transmission until we are ready to transfer.
	ctrl &= ~(I2S_CTRL_XFER_EN_TX | I2S_CTRL_XFER_EN_RX);

	// Serve as master.
	ctrl |= I2S_CTRL_MASTER_ENABLE;

	// Configure audio bits size.
	ctrl &= ~I2S_CTRL_BIT_SIZE_MASK;
	ctrl |= audio_bits << I2S_CTRL_BIT_SIZE_SHIFT;
	writel(ctrl, &bus->regs->ctrl);

	// Timing in LRCK mode:
	writel(bus->bits_per_sample, &bus->regs->timing);

	// I2S mode has [TX/RX]_DATA_OFFSET both set to 1.
	writel(((1 << I2S_OFFSET_RX_DATA_OFFSET_SHIFT) |
		(1 << I2S_OFFSET_TX_DATA_OFFSET_SHIFT)), &bus->regs->offset);

	// FSYNC_WIDTH = 2 clocks wide, TOTAL_SLOTS = 2 slots per fsync.
	writel((2 - 1) << I2S_CH_CTRL_FSYNC_WIDTH_SHIFT, &bus->regs->ch_ctrl);
	writel((2 - 1), &bus->regs->slot_ctrl);
	return 0;
}

static int tegra_i2s_send(I2sOps *me, uint32_t *data, unsigned int length)
{
	TegraI2s *bus = container_of(me, TegraI2s, ops);

	if (!bus->initialized) {
		if (tegra_i2s_init(bus))
			return -1;
		else
			bus->initialized = 1;
	}

	// Note the FIFO on Tegra 1x4 (provided by APBIF inside AHUB) has its
	// own flow control to start FIFO only when the accumulated data has
	// reached a threshold, so we don't need to explicitly prefill it or
	// check its fullness.
	tegra_i2s_transmit_enable(bus->regs, 1);
	bus->fifo->send(bus->fifo, data, length * sizeof(*data));
	tegra_i2s_transmit_enable(bus->regs, 0);

	return 0;
}

int tegra_i2s_set_cif_tx_ctrl(TegraI2s *i2s, uint32_t value)
{
	// The CIF is not really part of I2S -- it's for Audio Hub to control
	// the interface between I2S and Audio Hub.  However since it's put in
	// I2S registers domain instead of Audio Hub, we need to export this as
	// a function.
	writel(value, &i2s->regs->cif_tx_ctrl);
	return 0;
}

TegraI2s *new_tegra_i2s(uintptr_t regs, TxFifoOps *fifo, int id,
			int bits_per_sample, int channels, uint32_t clock_freq,
			uint32_t sampling_rate)
{
	TegraI2s *bus = xzalloc(sizeof(*bus));
	bus->ops.send = &tegra_i2s_send;
	bus->regs = (TegraI2sRegs *)regs;
	bus->fifo = fifo;
	bus->id = id;
	bus->bits_per_sample = bits_per_sample;
	bus->channels = channels;
	bus->clock_freq = clock_freq;
	bus->sampling_rate = sampling_rate;
	return bus;
}
