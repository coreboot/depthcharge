/*
 * Copyright 2011 Google Inc.
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

/* This file is derived from the flashrom project. */
#include <libpayload.h>
#include <stdint.h>

#include "drivers/flash/ich7.h"
#include "drivers/flash/ich_shared.h"

typedef struct Ich7SpiRegs {
	uint16_t spis;
	uint16_t spic;
	uint32_t spia;
	uint64_t spid[8];
	uint64_t _pad;
	uint32_t bbar;
	uint16_t preop;
	uint16_t optype;
	uint8_t opmenu[8];
} __attribute__((packed)) Ich7SpiRegs;

static int ich7_spi_get_lock(IchFlash *me)
{
	uint8_t *rcrb = ich_spi_get_rcrb();

	const uint16_t ich7_spibar_offset = 0x3020;
	Ich7SpiRegs *ich7_spi = (Ich7SpiRegs *)(rcrb + ich7_spibar_offset);

	return readw(&ich7_spi->spis) & SPIS_LOCK;
}

IchFlash *new_ich7_spi_flash(uint32_t rom_size)
{
	IchFlash *flash = xmalloc(sizeof(*flash) + rom_size);
	memset(flash, 0, sizeof(*flash));

	uint8_t *rcrb = ich_spi_get_rcrb();
	const uint16_t ich7_spibar_offset = 0x3020;
	Ich7SpiRegs *ich7_spi = (Ich7SpiRegs *)(rcrb + ich7_spibar_offset);

	flash->ops.read = &ich_spi_flash_read;

	flash->opmenu = ich7_spi->opmenu;
	flash->menubytes = sizeof(ich7_spi->opmenu);
	flash->optype = &ich7_spi->optype;
	flash->addr = &ich7_spi->spia;
	flash->data = (uint8_t *)ich7_spi->spid;
	flash->databytes = sizeof(ich7_spi->spid);
	flash->status = (uint8_t *)&ich7_spi->spis;
	flash->control = &ich7_spi->spic;
	flash->bbar = &ich7_spi->bbar;
	flash->preop = &ich7_spi->preop;
	flash->bios_cntl_clear = 0x0;
	// Disable the BIOS write protect so write commands are allowed.
	flash->bios_cntl_set = 0x1;

	flash->get_lock = &ich7_spi_get_lock;

	flash->rom_size = rom_size;

	return flash;
}
