/*
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

#include "drivers/bus/i2s/i2s.h"

typedef struct
{
	I2sOps ops;

	void *regs;

	int initialized;

	int bits_per_sample;
	int channels;
	int lr_frame_size;
} Exynos5I2s;

Exynos5I2s *new_exynos5_i2s(uintptr_t regs, int bits_per_sample,
			    int channels, int lr_frame_size);

Exynos5I2s *new_exynos5_i2s_multi(uintptr_t regs, int bits_per_sample,
				  int channels, int lr_frame_size);

#endif /* __DRIVERS_BUS_I2S_EXYNOS5_H__ */
