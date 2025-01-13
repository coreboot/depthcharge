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

#ifndef __DRIVERS_BUS_SPI_EXYNOS5_H__
#define __DRIVERS_BUS_SPI_EXYNOS5_H__

#include "drivers/bus/spi/spi.h"

typedef struct Exynos5Spi
{
	SpiOps ops;
	void *reg_addr;
	int initialized;
	int started;
} Exynos5Spi;

Exynos5Spi *new_exynos5_spi(uintptr_t reg_addr);

#endif /* __DRIVERS_BUS_SPI_EXYNOS5_H__ */
