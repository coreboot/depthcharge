/*
 * Copyright 2014 Rockchip Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but without any warranty; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */


#ifndef __DRIVERS_BUS_I2S_ROCKCHIP_H__
#define __DRIVERS_BUS_I2S_ROCKCHIP_H__

#include "drivers/bus/i2s/i2s.h"
#include "drivers/common/fifo.h"

typedef struct {
	I2sOps ops;
	void *regs;
	int initialized;
	int bits_per_sample;
	int channels;
	int lr_frame_size;
} RockchipI2s;

RockchipI2s *new_rockchip_i2s(uintptr_t regs, int bits_per_sample,
	int channels, int lr_frame_size);

#endif /* __DRIVERS_BUS_I2S_ROCKCHIP_H__ */
