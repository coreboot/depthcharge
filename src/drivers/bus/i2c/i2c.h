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
 */

#ifndef __DRIVERS_BUS_I2C_I2C_H__
#define __DRIVERS_BUS_I2C_I2C_H__

#include <stddef.h>
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
	uint8_t *buf;	// buffer for read or write
	int len;	// buffer size
	uint8_t chip;	// slave address
	uint8_t read;	// 0:write 1:read
} I2cSeg;

typedef struct I2cOps
{
	int scan_mode;
	int (*transfer)(struct I2cOps *me, I2cSeg *segments, int seg_count);
	void (*scan_mode_on_off)(struct I2cOps *me, int mode_on);
} I2cOps;

typedef struct I2cWriteVec
{
	uint8_t	reg;
	uint8_t	val;
} I2cWriteVec;

void scan_mode_on_off(struct I2cOps *me, int mode_on);

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

/**
 * Read a word by 2 segments in one frame
 * Read interface for 16-bit address and 16-bit data
 *
 * [start][slave addr][w][register addr high][register addr low]
 * [start][slave addr][r][data byte high][data byte low][stop]
 */
int i2c_addrw_readw(I2cOps *ops, uint8_t chip, uint16_t reg, uint16_t *data);

/**
 * Write a word by one segment in one frame
 * Write interface for 16-bit address and 16-bit data
 *
 * [start][slave addr][w][register addr high][register addr low]
 * [data byte high][data byte low][stop]
 */
int i2c_addrw_writew(I2cOps *ops, uint8_t chip, uint16_t reg, uint16_t data);

/**
 * Read a block by 2 segments in one frame
 *
 * [start][slave addr][w][register addr][start][slave addr][r][data][stop]
 */
int i2c_readblock(I2cOps *ops, uint8_t chip, uint8_t reg,
		   uint8_t *data, int len);

/**
 * Write a block by one segment in one frame.
 *
 * [start][slave addr][w][register addr][data][stop]
 */
int i2c_writeblock(I2cOps *ops, uint8_t chip, uint8_t reg,
		const uint8_t *data, int len);

/**
 * Read multiple registers as individual transactions.
 */
int i2c_read_regs(I2cOps *ops, uint8_t chip,
		  const uint8_t *regs, size_t count, uint8_t *data);

/**
 * Write multiple registers as individual transactions.
 */
int i2c_write_regs(I2cOps *ops, uint8_t chip,
		   const I2cWriteVec *cmds, size_t count);

/**
 * Clears bits in a register by doing a read/modify/write cycle.
 */
int i2c_clear_bits(I2cOps *bus, int chip, int reg, int mask_clr);

/**
 * Sets bits in a register by doing a read/modify/write cycle.
 */
int i2c_set_bits(I2cOps *bus, int chip, int reg, int mask_set);

/*
 * Add I2C node to the list of nodes known to the system.
 *
 * The list object includes the address of ops structure of the i2c node.
 * Format allows to assing the list object an arbitrary name.
 */
void add_i2c_controller_to_list(I2cOps *ops, const char *fmt, ...);

#endif /* __DRIVERS_BUS_I2C_I2C_H__ */
