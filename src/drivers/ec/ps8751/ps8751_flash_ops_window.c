// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2021 Google LLC.
 */

#include "drivers/ec/ps8751/ps8751_priv.h"

/* stay under I2C tunnel message size limit */
#define PS_FW_I2C_IO_CHUNK	240

/**
 * read a block of contiguous bytes from a SPI target
 *
 * @param me	device context
 * @param page	i2c device address
 * @param reg	starting i2c register on target device
 * @param data	pointer to return data buffer
 * @param count	number of bytes to read
 * @return 0 if ok, -1 on error
 */

static int __must_check read_block(Ps8751 *me,
				   uint8_t page, uint8_t reg,
				   uint8_t *data, size_t count)
{
	return i2c_readblock(&me->bus->ops, page, reg, data, count);
}

/**
 * write a block of contiguous bytes to a SPI target
 *
 * @param me	device context
 * @param page	i2c device address
 * @param reg	starting i2c register on target device
 * @param data	pointer to data buffer
 * @param count	number of bytes to write
 * @return 0 if ok, -1 on error
 */

static int __must_check write_block(Ps8751 *me,
				    uint8_t page, uint8_t reg,
				    const uint8_t *data, size_t count)
{
	return i2c_writeblock(&me->bus->ops, page, reg, data, count);
}

/**
 * Initialize device context state for subsequent flash window
 * operations.
 */

static void ps8751_flash_window_start(Ps8751 *me)
{
	me->last_a16_a23 = -1;
	me->last_a8_a15 = -1;
}

_Static_assert(PS_FW_I2C_WINDOW_SIZE == 256,
	       "PS8xxx flash access window size "
	       "over I2C is expected to be 256 bytes.");

/**
 * Read data from flash using the I2C mapped window. The actual number
 * of bytes read may be less than requested if the window size is
 * exceeded.
 */

static ssize_t __must_check ps8751_flash_window_read(Ps8751 *me,
						uint8_t * const data,
						const size_t count,
						const uint32_t a24)
{
	ssize_t chunk;

	const uint32_t a16_a23 = (a24 >> 16) & 0xff;
	const uint32_t a8_a15 = (a24 >> 8) & 0xff;
	const uint32_t a0_a7 = a24 & 0xff;

	/* clip at EC message size limit */
	chunk = MIN(count, PS_FW_I2C_IO_CHUNK);

	/* clip at I2C window boundary */
	chunk = MIN(chunk, PS_FW_I2C_WINDOW_SIZE - a0_a7);

	if (a16_a23 != me->last_a16_a23) {
		if (ps8751_write_reg(me,
				     PAGE_2, P2_FLASH_A16_A23, a16_a23) != 0)
			return -1;
		me->last_a16_a23 = a16_a23;
	}

	if (a8_a15 != me->last_a8_a15) {
		if (ps8751_write_reg(me, PAGE_2, P2_FLASH_A8_A15, a8_a15) != 0)
			return -1;
		me->last_a8_a15 = a8_a15;
	}

	if (read_block(me, PAGE_7, a0_a7, data, chunk) != 0)
		return -1;

	return chunk;
}

/**
 * Write data to flash using the I2C mapped window. The actual number of
 * bytes written may be less than requested if the window size is
 * exceeded.
 */

static ssize_t __must_check ps8751_flash_window_write(Ps8751 *me,
						const uint8_t * const data,
						const size_t count,
						const uint32_t a24)
{
	ssize_t chunk;

	const uint32_t a16_a23 = (a24 >> 16) & 0xff;
	const uint32_t a8_a15 = (a24 >> 8) & 0xff;
	const uint32_t a0_a7 = a24 & 0xff;

	/* clip at EC message size limit */
	chunk = MIN(count, PS_FW_I2C_IO_CHUNK);

	/* clip at I2C window boundary */
	chunk = MIN(chunk, PS_FW_I2C_WINDOW_SIZE - a0_a7);

	if (a16_a23 != me->last_a16_a23) {
		if (ps8751_write_reg(me,
				     PAGE_2, P2_FLASH_A16_A23, a16_a23) != 0)
			return -1;
		me->last_a16_a23 = a16_a23;
	}

	if (a8_a15 != me->last_a8_a15) {
		if (ps8751_write_reg(me, PAGE_2, P2_FLASH_A8_A15, a8_a15) != 0)
			return -1;
		me->last_a8_a15 = a8_a15;
	}

	if (write_block(me, PAGE_7, a0_a7, data, chunk) != 0)
		return -1;

	return chunk;
}

/**
 * Enable writes to flash using the I2C PAGE_7 window. This only unlocks
 * the window, not the flash.
 */

static int ps8751_flash_window_write_enable(Ps8751 *me)
{
	uint8_t wr_status;
	uint64_t t0_us;

	t0_us = timer_us(0);
	do {
		static const I2cWriteVec wr[] = {
			{ P2_PROG_WIN_UNLOCK, 0xaa },
			{ P2_PROG_WIN_UNLOCK, 0x55 },
			{ P2_PROG_WIN_UNLOCK, 0x50 }, /* P */
			{ P2_PROG_WIN_UNLOCK, 0x41 }, /* A */
			{ P2_PROG_WIN_UNLOCK, 0x52 }, /* R */
			{ P2_PROG_WIN_UNLOCK, 0x44 }  /* D */
		};

		if (timer_us(t0_us) >= PS_SPI_TIMEOUT_US) {
			printf("%s: window write enable timeout after %ums\n",
			       me->chip_name, USEC_TO_MSEC(PS_SPI_TIMEOUT_US));
			return -1;
		}

		if (ps8751_write_regs(me, PAGE_2, wr, ARRAY_SIZE(wr)) != 0)
			return -1;

		if (ps8751_read_reg(me, PAGE_2,
				    P2_PROG_WIN_UNLOCK, &wr_status) != 0)
			return -1;

	} while (wr_status != P2_PROG_WIN_UNLOCK_ACK);

	return 0;
}

/**
 * Disable writes to flash using the I2C PAGE_7 window. This only locks
 * the window, not the flash.
 */

static int ps8751_flash_window_write_disable(Ps8751 *me)
{
	if (ps8751_write_reg(me, PAGE_2,
		      P2_PROG_WIN_UNLOCK, P2_PROG_WIN_UNLOCK_LOCK) != 0)
		return -1;

	return 0;
}

void ps8751_init_flash_window_ops(Ps8751 *me)
{
	me->flash_start = ps8751_flash_window_start;
	me->flash_read = ps8751_flash_window_read;
	me->flash_write = ps8751_flash_window_write;
	me->flash_write_enable = ps8751_flash_window_write_enable;
	me->flash_write_disable = ps8751_flash_window_write_disable;
}
