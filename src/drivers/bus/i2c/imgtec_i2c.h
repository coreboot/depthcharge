/*
 * Copyright 2012-2014 Imagination Technologies Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#ifndef __DRIVERS_BUS_I2C_IMG_H__
#define __DRIVERS_BUS_I2C_IMG_H__

#include "drivers/bus/i2c/i2c.h"

typedef struct ImgI2c
{
	I2cOps ops;
	uint32_t base;
	uint32_t bitrate;
	uint32_t clkrate;
	uint32_t speed;
	uint32_t int_enable;
	uint32_t current_len;
	uint8_t *current_buf;
	uint8_t local_status;
	uint8_t initialized;
	uint8_t scan_mode;
} ImgI2c;

#define min_t(type, x, y) ({				\
	type __min1 = (x);					\
	type __min2 = (y);					\
	__min1 < __min2 ? __min1 : __min2; })

#define max_t(type, x, y) ({				\
	type __max1 = (x);					\
	type __max2 = (y);					\
	__max1 > __max2 ? __max1 : __max2; })

#define clamp_t(type, val, lo, hi) min_t(type, max_t(type, val, lo), hi)

ImgI2c *new_imgtec_i2c(unsigned port, unsigned bitrate, unsigned clkrate);

#endif /* __DRIVERS_BUS_I2C_IMG_H__ */
