// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2021 Google LLC.
 */

#include "drivers/ec/ps8751/ps8751_priv.h"

static void ps8751_flash_fifo_start(Ps8751 *me)
{
}

/**
 * Read data from flash using the SPI FIFO. The actual number of bytes
 * read may be less than requested if the FIFO size is exceeded.
 */

static ssize_t __must_check ps8751_flash_fifo_read(Ps8751 *me,
				uint8_t * const data, const size_t count,
				const uint32_t a24)
{
	ssize_t chunk;

	/* FIFO only has room for 16 data bytes for read */
	chunk = MIN(count, PS_FW_RD_CHUNK);

	/* clip at flash page boundary */
	int page_offset = a24 & SPI_FLASH_PAGE_MASK;
	chunk = MIN(chunk, SPI_FLASH_PAGE_SIZE - page_offset);

	if (ps8751_spi_setup_cmd24(me, SPI_CMD_READ_DATA, a24) != 0)
		return -1;

	const I2cWriteVec rd[] = {
		{ P2_SPI_LEN,
		  ((chunk - 1) << 4) | (4 - 1) },
		{ P2_SPI_CTRL, P2_SPI_CTRL_TRIGGER },
	};
	if (ps8751_write_regs(me, PAGE_2, rd, ARRAY_SIZE(rd)) != 0)
		return -1;

	if (ps8751_spi_fifo_wait_busy(me) != 0)
		return -1;

	for (int i = 0; i < chunk; ++i) {
		if (ps8751_read_reg(me, PAGE_2, P2_RD_FIFO, &data[i]) != 0)
			return -1;
	}

	return chunk;
}

/**
 * Write data to flash using the SPI FIFO. The actual number of bytes
 * written may be less than requested if the FIFO size is exceeded.
 */

static ssize_t __must_check ps8751_flash_fifo_write(Ps8751 *me,
				const uint8_t * const data, const size_t count,
				const uint32_t a24)
{
	int chunk;

	if (ps8751_spi_cmd_enable_writes(me) != 0)
		return -1;

	/* FIFO only has room for 12 data bytes for write */
	chunk = MIN(PS_FW_WR_CHUNK, count);

	/* clip at flash page boundary */
	int page_offset = a24 & SPI_FLASH_PAGE_MASK;
	chunk = MIN(chunk, SPI_FLASH_PAGE_SIZE - page_offset);

	if (ps8751_spi_setup_cmd24(me, SPI_CMD_PROG_PAGE, a24) != 0)
		return -1;

	for (int i = 0; i < chunk; ++i) {
		if (ps8751_write_reg(me, PAGE_2, P2_WR_FIFO, data[i]) != 0)
			return -1;
	}

	const I2cWriteVec wr[] = {
		{ P2_SPI_LEN, (4 + chunk - 1) },
		{ P2_SPI_CTRL,
		  P2_SPI_CTRL_NOREAD|P2_SPI_CTRL_TRIGGER },
	};
	if (ps8751_write_regs(me, PAGE_2, wr, ARRAY_SIZE(wr)) != 0)
		return -1;
	if (ps8751_spi_fifo_wait_busy(me) != 0)
		return -1;
	if (ps8751_spi_wait_rom_ready(me) != 0)
		return -1;

	return chunk;
}

static int ps8751_flash_fifo_write_enable(Ps8751 *me)
{
	return 0;
}

static int ps8751_flash_fifo_write_disable(Ps8751 *me)
{
	return 0;
}

void ps8751_init_flash_fifo_ops(Ps8751 *me)
{
	me->flash_start = ps8751_flash_fifo_start;
	me->flash_read = ps8751_flash_fifo_read;
	me->flash_write = ps8751_flash_fifo_write;
	me->flash_write_enable = ps8751_flash_fifo_write_enable;
	me->flash_write_disable = ps8751_flash_fifo_write_disable;
}
