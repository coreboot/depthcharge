/*
 * Copyright 2013 Google Inc.  All rights reserved.
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

#ifndef __DRIVERS_BUS_I2S_TEGRA_H__
#define __DRIVERS_BUS_I2S_TEGRA_H__

#include "drivers/bus/i2s/i2s.h"
#include "drivers/common/fifo.h"

struct TegraI2sRegs;
typedef struct {
	I2sOps ops;

	struct TegraI2sRegs *regs;
	TxFifoOps *fifo;

	int initialized;
	int bits_per_sample;
	int channels;

	int id;
	uint32_t clock_freq;
	uint32_t sampling_rate;
} TegraI2s;

TegraI2s *new_tegra_i2s(uintptr_t regs, TxFifoOps *fifo, int id,
			int bits_per_sample, int channels, uint32_t clock_freq,
			uint32_t sampling_rate);

// Sets the client interface value when an I2S peripheral is integrated into
// audio hub.
int tegra_i2s_set_cif_tx_ctrl(TegraI2s *i2s, uint32_t value);

#endif /* __DRIVERS_BUS_I2S_TEGRA_H__ */
