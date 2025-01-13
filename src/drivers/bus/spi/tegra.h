/*
 * Copyright 2013 Google LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __DRIVERS_BUS_SPI_TEGRA_H__
#define __DRIVERS_BUS_SPI_TEGRA_H__

#include "drivers/bus/spi/spi.h"

struct TegraApbDmaController;
typedef struct TegraApbDmaController TegraApbDmaController;

typedef struct {
	SpiOps ops;
	void *reg_addr;
	int started;
	int initialized;
	uint32_t dma_slave_id;
	TegraApbDmaController *dma_controller;
} TegraSpi;

TegraSpi *new_tegra_spi(uintptr_t reg_addr,
			TegraApbDmaController *dma_controller,
			uint32_t dma_slave_id);

#endif /* __DRIVERS_BUS_SPI_TEGRA_H__ */
