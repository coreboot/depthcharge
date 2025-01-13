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

#ifndef __DRIVERS_BUS_SPI_SPI_H__
#define __DRIVERS_BUS_SPI_SPI_H__

#include <stdint.h>

typedef struct SpiOps
{
	int (*start)(struct SpiOps *me);
	int (*transfer)(struct SpiOps *me, void *in, const void *out,
			uint32_t size);
	int (*stop)(struct SpiOps *me);
} SpiOps;

#endif /* __DRIVERS_BUS_SPI_SPI_H__ */
