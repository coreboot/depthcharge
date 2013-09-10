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

#include "base/container_of.h"
#include "drivers/flash/flash.h"
#include "drivers/flash/ich.h"
#include "drivers/flash/ich_shared.h"

static void read_reg(const void *src, void *value, uint32_t size)
{
	const uint8_t *bsrc = src;
	uint8_t *bvalue = value;

	while (size >= 4) {
		*(uint32_t *)bvalue = readl(bsrc);
		bsrc += 4; bvalue += 4; size -= 4;
	}
	while (size) {
		*bvalue = readb(bsrc);
		bsrc++; bvalue++; size--;
	}
}

uint8_t *ich_spi_get_rcrb(void)
{
	static uint8_t *rcrb = NULL;

	if (rcrb)
		return rcrb;

	pcidev_t dev = PCI_DEV(0, 31, 0);

	uint32_t rcba; // Root Complex Base Address
	rcba = pci_read_config32(dev, 0xf0);
	// Bits 31-14 are the base address, 13-1 are reserved, 0 is enable.
	rcrb = (uint8_t *)(rcba & 0xffffc000);
	return rcrb;
}

static int spi_setup_opcode(IchFlash *flash, uint8_t type, uint8_t opcode)
{
	if (!flash->locked) {
		// The lock is off, so just use index 0.
		writeb(opcode, flash->opmenu);
		uint16_t optypes = readw(flash->optype);
		optypes = (optypes & 0xfffc) | (type & 0x3);
		writew(optypes, flash->optype);
		return 0;
	} else {
		// The lock is on. See if what we need is on the menu.
		uint16_t opcode_index;

		uint8_t opmenu[flash->menubytes];
		read_reg(flash->opmenu, opmenu, sizeof(opmenu));
		int menubytes = flash->menubytes;
		for (opcode_index = 0; opcode_index < menubytes;
				opcode_index++) {
			if (opmenu[opcode_index] == opcode)
				break;
		}

		if (opcode_index == flash->menubytes) {
			printf("ICH SPI: Opcode %x not found\n", opcode);
			return -1;
		}

		uint16_t optypes = readw(flash->optype);
		uint8_t optype = (optypes >> (opcode_index * 2)) & 0x3;
		if (optype != type) {
			printf("ICH SPI: Transaction doesn't fit type %d\n",
				optype);
			return -1;
		}
		return opcode_index;
	}
}

/*
 * Wait for up to 60ms til status register bit(s) turn 1 (in case wait_til_set
 * below is True) or 0. In case the wait was for the bit(s) to set - write
 * those bits back, which would cause resetting them.
 *
 * Return the last read status value on success or -1 on failure.
 */
static int32_t ich_status_poll(IchFlash *flash, uint16_t bitmask,
			       int wait_til_set)
{
	int timeout = 60000; // in us
	uint16_t status = 0;

	while (timeout--) {
		status = readw(flash->status);
		if (wait_til_set ^ ((status & bitmask) == 0)) {
			if (wait_til_set)
				writew((status & bitmask), flash->status);
			return status;
		}
		udelay(1);
	}

	printf("ICH SPI: SCIP timeout, read %#x, expected %#x.\n",
		status, bitmask);
	return -1;
}

static void *flash_read_op(IchFlash *flash, uint8_t opcode,
			   uint32_t offset, uint32_t size)
{
	uint8_t * const data = flash->cache + offset;

	if (ich_status_poll(flash, SPIS_SCIP, 0) == -1)
		return NULL;

	writew(SPIS_CDS | SPIS_FCERR, flash->status);

	uint8_t type = SPI_OPCODE_TYPE_READ_WITH_ADDRESS;
	int16_t opcode_index;
	if ((opcode_index = spi_setup_opcode(flash, type, opcode)) < 0)
		return NULL;

	// Prepare control fields.
	uint16_t control = SPIC_SCGO | ((opcode_index & 0x07) << 4);

	while (size) {
		uint32_t data_length;

		// SPI addresses are 24 bits long.
		writel(offset & 0x00FFFFFF, flash->addr);

		data_length = MIN(size, flash->databytes);

		// Set proper control field values.
		control &= ~((flash->databytes - 1) << 8);
		control |= SPIC_DS;
		control |= (data_length - 1) << 8;

		/* write it */
		writew(control, flash->control);

		/* Wait for Cycle Done Status or Flash Cycle Error. */
		int32_t status =
			ich_status_poll(flash, SPIS_CDS | SPIS_FCERR, 1);
		if (status == -1)
			return NULL;

		if (status & SPIS_FCERR) {
			printf("ICH SPI: Data transaction error\n");
			return NULL;
		}

		read_reg(flash->data, flash->cache + offset, data_length);

		offset += data_length;
		size -= data_length;
	}

	return data;
}

static void ich_spi_init(IchFlash *flash)
{
	if (flash->initialized)
		return;

	flash->locked = flash->get_lock(flash);

	pcidev_t dev = PCI_DEV(0, 31, 0);

	const uint32_t bbar_mask = 0x00ffff00;
	uint32_t ichspi_bbar;

	uint32_t minaddr = 0;
	minaddr &= bbar_mask;
	ichspi_bbar = readl(flash->bbar) & ~bbar_mask;
	ichspi_bbar |= minaddr;
	writel(ichspi_bbar, flash->bbar);

	uint8_t bios_cntl = pci_read_config8(dev, 0xdc);
	bios_cntl &= ~flash->bios_cntl_clear;
	bios_cntl |= flash->bios_cntl_set;
	pci_write_config8(dev, 0xdc, bios_cntl);

	flash->initialized = 1;
}

void *ich_spi_flash_read(FlashOps *me, uint32_t offset, uint32_t size)
{
	IchFlash *flash = container_of(me, IchFlash, ops);

	if (!flash->initialized)
		ich_spi_init(flash);

	uint32_t chunk = flash->databytes;
	uint32_t mask = ~(chunk - 1);

	uint32_t start = offset & mask;
	uint32_t end = (offset + size + chunk - 1) & mask;

	uint8_t *data = flash_read_op(flash, 0xB, start, end - start);
	if (data)
		data += (offset - start);
	return data;
}
