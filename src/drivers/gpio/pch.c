/*
 * Copyright (c) 2012 The Chromium OS Authors.
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

/*
 * Danger Will Robinson! Bank 0 (GPIOs 0-31) seems to be fairly stable. Most
 * ICH versions have more, but decoding the matrix that describes them is
 * absurdly complex and constantly changing. We'll provide Bank 1 and Bank 2,
 * but they will ONLY work for certain unspecified chipsets because the offset
 * from GPIOBASE changes randomly. Even then, many GPIOs are unimplemented or
 * reserved or subject to arcane restrictions.
 */

#include <libpayload.h>
#include <pci.h>
#include <stdint.h>

#include "drivers/gpio/pch.h"

// IO ports for controlling the banks of GPIOs.
static struct {
	uint8_t use_sel;
	uint8_t io_sel;
	uint8_t lvl;
} gpio_bank[] = {
	{ 0x00, 0x04, 0x0c },		/* Bank 0 */
	{ 0x30, 0x34, 0x38 },		/* Bank 1 */
	{ 0x40, 0x44, 0x48 }		/* Bank 2 */
};

static uint32_t pch_gpiobase(void)
{
	static const uint32_t dev = PCI_DEV(0, 0x1f, 0);
	static const uint8_t pci_cfg_gpiobase = 0x48;

	static uint32_t base = ~(uint32_t)0;
	if (base)
		return base;

	base = pci_read_config32(dev, pci_cfg_gpiobase);
	// Drop the IO space bit.
	base &= ~0x1;
	return base;
}

static void pch_gpio_set(uint16_t addr, int pos, int val)
{
	uint32_t reg = inl(addr);
	reg = (reg & ~(1 << pos)) | ((val & 1) << pos);
	outl(addr, reg);
}

static int pch_gpio_get(uint16_t addr, int pos)
{
	uint32_t reg = inl(addr);
	return (reg >> pos) & 1;
}

int pch_gpio_direction(unsigned num, unsigned input)
{
	if (num > ARRAY_SIZE(gpio_bank))
		return -1;

	pch_gpio_set(pch_gpiobase() + gpio_bank[num / 32].io_sel,
		      num % 32, input);
	return 0;
}

int pch_gpio_get_value(unsigned num)
{
	if (num > ARRAY_SIZE(gpio_bank))
		return -1;

	return pch_gpio_get(pch_gpiobase() + gpio_bank[num / 32].lvl,
			     num % 32);
}

int pch_gpio_set_value(unsigned num, unsigned value)
{
	if (num > ARRAY_SIZE(gpio_bank))
		return -1;

	pch_gpio_set(pch_gpiobase() + gpio_bank[num / 32].lvl,
		      num % 32, value);
	return 0;
}
