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

#include "drivers/flash/ich.h"

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

IchSpiController *ich_spi_get_controller(void)
{
	static IchSpiController cntlr;
	static int done = 0;

	if (done)
		return &cntlr;

	uint8_t *rcrb = ich_spi_get_rcrb();

	const uint16_t ich7_spibar_offset = 0x3020;
	ich7_spi_regs *ich7_spi = (Ich7SpiRegs *)(rcrb + ich7_spibar_offset);

	cntlr.opmenu = ich7_spi->opmenu;
	cntlr.menubytes = sizeof(ich7_spi->opmenu);
	cntlr.optype = &ich7_spi->optype;
	cntlr.addr = &ich7_spi->spia;
	cntlr.data = (uint8_t *)ich7_spi->spid;
	cntlr.databytes = sizeof(ich7_spi->spid);
	cntlr.status = (uint8_t *)&ich7_spi->spis;
	cntlr.control = &ich7_spi->spic;
	cntlr.bbar = &ich7_spi->bbar;
	cntlr.preop = &ich7_spi->preop;

	cntlr.bios_cntl_clear = 0x0;
	// Disable the BIOS write protect so write commands are allowed.
	cntlr.bios_cntl_set = 0x1;

	done = 1;
	return &cntlr;
}

int ich_spi_get_lock(void)
{
	const uint16_t ich7_spibar_offset = 0x3020;
	ich7_spi_regs *ich7_spi = (Ich7SpiRegs *)(rcrb + ich7_spibar_offset);

	return readw(&ich7_spi->spis) & SPIS_LOCK;
}
