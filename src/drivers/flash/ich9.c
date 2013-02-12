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

typedef struct Ich9SpiRegs {
	uint32_t bfpr;
	uint16_t hsfs;
	uint16_t hsfc;
	uint32_t faddr;
	uint32_t _reserved0;
	uint32_t fdata[16];
	uint32_t frap;
	uint32_t freg[5];
	uint32_t _reserved1[3];
	uint32_t pr[5];
	uint32_t _reserved2[2];
	uint8_t ssfs;
	uint8_t ssfc[3];
	uint16_t preop;
	uint16_t optype;
	uint8_t opmenu[8];
	uint32_t bbar;
	uint8_t _reserved3[12];
	uint32_t fdoc;
	uint32_t fdod;
	uint8_t _reserved4[8];
	uint32_t afc;
	uint32_t lvscc;
	uint32_t uvscc;
	uint8_t _reserved5[4];
	uint32_t fpb;
	uint8_t _reserved6[28];
	uint32_t srdl;
	uint32_t srdc;
	uint32_t srd;
} __attribute__((packed)) Ich9SpiRegs;

IchSpiController *ich_spi_get_controller(void)
{
	static IchSpiController cntlr;
	static int done = 0;

	if (done)
		return &cntlr;

	uint8_t *rcrb = ich_spi_get_rcrb();

	const uint16_t ich9_spibar_offset = 0x3800;
	Ich9SpiRegs *ich9_spi = (Ich9SpiRegs *)(rcrb + ich9_spibar_offset);

	cntlr.opmenu = ich9_spi->opmenu;
	cntlr.menubytes = sizeof(ich9_spi->opmenu);
	cntlr.optype = &ich9_spi->optype;
	cntlr.addr = &ich9_spi->faddr;
	cntlr.data = (uint8_t *)ich9_spi->fdata;
	cntlr.databytes = sizeof(ich9_spi->fdata);
	cntlr.status = &ich9_spi->ssfs;
	cntlr.control = (uint16_t *)ich9_spi->ssfc;
	cntlr.bbar = &ich9_spi->bbar;
	cntlr.preop = &ich9_spi->preop;

	// Deassert SMM BIOS Write Protect Disable.
	cntlr.bios_cntl_clear = 0x20;
	// Disable the BIOS write protect so write commands are allowed.
	cntlr.bios_cntl_set = 0x1;

	done = 1;
	return &cntlr;
}

int ich_spi_get_lock(void)
{
	uint8_t *rcrb = ich_spi_get_rcrb();

	const uint16_t ich9_spibar_offset = 0x3800;
	Ich9SpiRegs *ich9_spi = (Ich9SpiRegs *)(rcrb + ich9_spibar_offset);

	return readw(&ich9_spi->hsfs) & HSFS_FLOCKDN;
}
