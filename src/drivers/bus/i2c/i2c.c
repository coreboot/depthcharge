/*
 * Copyright 2014 Google Inc.  All rights reserved.
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

#include <libpayload.h>
#include "drivers/bus/i2c/i2c.h"

int i2c_readb(I2cOps *ops, uint8_t chip, uint8_t reg, uint8_t *data)
{
	I2cSeg seg[2];

	seg[0].read = 0;
	seg[0].chip = chip;
	seg[0].buf = &reg;
	seg[0].len = 1;
	seg[1].read = 1;
	seg[1].chip = chip;
	seg[1].buf = data;
	seg[1].len = 1;

	return ops->transfer(ops, seg, ARRAY_SIZE(seg));
}

int i2c_writeb(I2cOps *ops, uint8_t chip, uint8_t reg, uint8_t data)
{
	I2cSeg seg;
	uint8_t buf[] = {reg, data};

	seg.read = 0;
	seg.chip = chip;
	seg.buf = buf;
	seg.len = ARRAY_SIZE(buf);

	return ops->transfer(ops, &seg, 1);
}

int i2c_readw(I2cOps *ops, uint8_t chip, uint8_t reg, uint16_t *data)
{
	I2cSeg seg[2];
	int ret;
	uint8_t bytes[2];

	seg[0].read = 0;
	seg[0].chip = chip;
	seg[0].buf = &reg;
	seg[0].len = 1;
	seg[1].read = 1;
	seg[1].chip = chip;
	seg[1].buf = &bytes[0];
	seg[1].len = 2;

	ret = ops->transfer(ops, seg, ARRAY_SIZE(seg));
	*data = (uint16_t)(bytes[0] << 8) + bytes[1];
	return ret;
}

int i2c_writew(I2cOps *ops, uint8_t chip, uint8_t reg, uint16_t data)
{
	I2cSeg seg;
	uint8_t buf[3] = {reg, data >> 8, data & 0xFF};

	seg.read = 0;
	seg.chip = chip;
	seg.buf = buf;
	seg.len = ARRAY_SIZE(buf);

	return ops->transfer(ops, &seg, 1);
}
