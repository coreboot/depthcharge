/*
 * Copyright 2013 Google Inc.
 *
 * See file CREDITS for list of people who contributed to this
 * project.
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

/* Mediatek SPI-NOR Flash Controller (SNFC) */

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>
#include "base/container_of.h"
#include "drivers/flash/flash.h"
#include "mtk_snfc.h"

enum {
	SFLASH_POLLINGREG_COUNTER = 5000000,
	SFLASH_WRBUF_SIZE         = 128,
	SFLASH_WRITE_IN_PROGRESS  = 1,
	/* NOR flash controller commands */
	SFLASH_RD_TRIGGER	  = 1 << 0,
	SFLASH_READSTATUS         = 1 << 1,
	SFLASH_PRG_CMD            = 1 << 2,
	SFLASH_WR_TRIGGER         = 1 << 4,
	SFLASH_WRITESTATUS        = 1 << 5,
	SFLASH_AUTOINC		  = 1 << 7,
	/* NOR flash commands */
	SFLASH_OP_WREN		  = 0x6,
	/* Dual read commands */
	SFLASH_READ_DUAL_EN	  = 0x1,
	SFLASH_1_1_2_READ	  = 0x3b,
};

#define get_nth_byte(d, n)	((d >> (8 * n)) & 0xff)

/* set serial flash program address */
static void set_sfpaddr(MtkNorFlash *flash, u32 addr)
{
	mtk_snfc_regs *mtk_snfc = flash->reg;

	write8(&mtk_snfc->radr[2], get_nth_byte(addr, 2));
	write8(&mtk_snfc->radr[1], get_nth_byte(addr, 1));
	write8(&mtk_snfc->radr[0], get_nth_byte(addr, 0));
}

static int polling_cmd(MtkNorFlash *flash, uint32_t val)
{
	uint64_t start = timer_us(0);
	mtk_snfc_regs *mtk_snfc = flash->reg;

	do {
		if (!(read32(&mtk_snfc->cmd) & val))
			return 0;

	} while (timer_us(start) < SFLASH_POLLINGREG_COUNTER);

	return -1;
}

static int mtk_snfc_execute_cmd(MtkNorFlash *flash, u8 cmdval)
{
	u8 val = cmdval & ~(SFLASH_AUTOINC);
	mtk_snfc_regs *mtk_snfc = flash->reg;

	write8(&mtk_snfc->cmd, cmdval);
	return polling_cmd(flash, val);
}

static int sflashhw_read_flash_status(MtkNorFlash *flash, uint8_t *value)
{
	mtk_snfc_regs *mtk_snfc = flash->reg;

	if (mtk_snfc_execute_cmd(flash, SFLASH_READSTATUS))
		return -1;

	*value = read8(&mtk_snfc->rdsr);
	return 0;
}

static int wait_for_write_done(MtkNorFlash *flash)
{
	uint8_t reg;
	uint64_t start = timer_us(0);

	do {
		if (sflashhw_read_flash_status(flash, &reg))
			return -1;

		if (!(reg & SFLASH_WRITE_IN_PROGRESS))
			return 0;
	} while (timer_us(start) < SFLASH_POLLINGREG_COUNTER);

	return -1;
}

static int nor_read(MtkNorFlash *flash, uint32_t addr, uint8_t *buf,
		    uint32_t length)
{
	mtk_snfc_regs *mtk_snfc = flash->reg;

	setbits8(&mtk_snfc->read_dual, SFLASH_READ_DUAL_EN);
	write8(&mtk_snfc->prgdata[3], SFLASH_1_1_2_READ);

	set_sfpaddr(flash, addr);

	while (length) {
		if (mtk_snfc_execute_cmd(flash, SFLASH_RD_TRIGGER |
					 SFLASH_AUTOINC))
			return -1;

		*buf++ = read8(&mtk_snfc->rdata);
		length--;
	}
	return 0;
}

static int nor_write(MtkNorFlash *flash, uint32_t addr, const u8 *buf,
		     uint32_t len)
{
	mtk_snfc_regs *mtk_snfc = flash->reg;

	set_sfpaddr(flash, addr);
	while (len) {
		write8(&mtk_snfc->wdata, *buf);

		if (mtk_snfc_execute_cmd(flash, SFLASH_WR_TRIGGER |
					 SFLASH_AUTOINC))
			return -1;

		if (wait_for_write_done(flash))
			return -1;
		buf++;
		len--;
	}
	return 0;
}

static void *mtk_nor_flash_read(FlashOps *me, uint32_t offset, uint32_t size)
{
	MtkNorFlash *flash = container_of(me, MtkNorFlash, ops);
	uint8_t *data;
	int ret;

	assert((offset + size) <= flash->rom_size);
	data = flash->buffer + offset;

	ret = nor_read(flash, offset, data, size);
	if (ret) {
		printf("nor_read failed!\n");
		return NULL;
	}

	return data;
}

/* Return the number of successfully written bytes */
static int mtk_nor_flash_write(FlashOps *me, const void *buffer,
			       uint32_t offset, uint32_t size)
{
	MtkNorFlash *flash = container_of(me, MtkNorFlash, ops);
	int ret;

	assert((offset + size) <= flash->rom_size);
	ret = nor_write(flash, offset, (const u8 *)buffer, size);
	if (ret) {
		printf("nor_write failed!\n");
		return -1;
	}

	return size;
}

MtkNorFlash *new_mtk_nor_flash(uintptr_t reg_addr)
{
	MtkNorFlash *flash = xmalloc(sizeof(*flash));
	uint32_t rom_size = lib_sysinfo.spi_flash.size;
	uint32_t sector_size = lib_sysinfo.spi_flash.sector_size;

	memset(flash, 0, sizeof(*flash));
	flash->ops.read = mtk_nor_flash_read;
	flash->ops.write = mtk_nor_flash_write;
	flash->ops.sector_size = sector_size;
	flash->rom_size = rom_size;
	flash->reg = (mtk_snfc_regs *)reg_addr;

	/* Provide sufficient alignment on the cache buffer so that the
	   underlying SPI controllers can perform optimal DMA transfers. */
	flash->buffer = xmemalign(1*KiB, rom_size);
	return flash;
}
