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

#ifndef __DRIVERS_BUS_I2C_I2C_H__
#define __DRIVERS_BUS_I2C_I2C_H__

#include <stdint.h>

/*
 * transfer operation consumes I2cSeg struct one by one and finishes a
 * transaction by a stop bit at the end.
 *
 *   frame = [segment] ... [segment][stop]
 *   segment = [start][slave addr][r/w][ack][data][ack][data][ack] ...
 *
 * Two adjacent segments are connected by a repeated start bit.
 */
typedef struct I2cSeg
{
	int read;	// 0:write 1:read
	uint8_t chip;	// slave address
	uint8_t *buf;	// buffer for read or write
	int len;	// buffer size
} I2cSeg;

typedef struct I2cOps
{
	int (*transfer)(struct I2cOps *me, I2cSeg *segments, int seg_count);
} I2cOps;

/*
 * Read a raw chunk of data in one segment and one frame.
 *
 * [start][slave addr][r][data][stop]
 */
static inline int i2c_read_raw(I2cOps *ops, uint8_t chip, uint8_t *data,
			       int len)
{
	I2cSeg seg = { .read = 1, .chip = chip, .buf = data, .len = len };
	return ops->transfer(ops, &seg, 1);
}

/*
 * Write a raw chunk of data in one segment and one frame.
 *
 * [start][slave addr][w][data][stop]
 */
static inline int i2c_write_raw(I2cOps *ops, uint8_t chip, uint8_t *data,
			        int len)
{
	I2cSeg seg = { .read = 0, .chip = chip, .buf = data, .len = len };
	return ops->transfer(ops, &seg, 1);
}

/**
 * Read a byte by two segments in one frame
 *
 * [start][slave addr][w][register addr][start][slave addr][r][data][stop]
 */
int i2c_readb(I2cOps *ops, uint8_t chip, uint8_t reg, uint8_t *data);

/**
 * Write a byte by one segment in one frame.
 *
 * [start][slave addr][w][register addr][data][stop]
 */
int i2c_writeb(I2cOps *ops, uint8_t chip, uint8_t reg, uint8_t data);

/**
 * Read a word by 2 segments in one frame
 *
 * [start][slave addr][w][register addr][start][slave addr][r][data byte high][data byte low][stop]
 */
int i2c_readw(I2cOps *ops, uint8_t chip, uint8_t reg, uint16_t *data);

/**
 * Write a word by one segment in one frame.
 *
 * [start][slave addr][w][register addr][data byte high][data byte low][stop]
 */
int i2c_writew(I2cOps *ops, uint8_t chip, uint8_t reg, uint16_t data);

#endif /* __DRIVERS_BUS_I2C_I2C_H__ */
