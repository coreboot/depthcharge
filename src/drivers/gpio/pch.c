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

#define NUM_GPIO_BANKS 3
// IO ports for controlling the banks of GPIOS.
//                                    bank:   0     1     2
static uint8_t gpio_use[NUM_GPIO_BANKS] = { 0x00, 0x30, 0x40 };
static uint8_t gpio_io[NUM_GPIO_BANKS]  = { 0x04, 0x34, 0x44 };
static uint8_t gpio_lvl[NUM_GPIO_BANKS] = { 0x0c, 0x38, 0x48 };

#define LP_GPIO_CONF0(num)         (0x100 + ((num) * 8))
#define  LP_GPIO_CONF0_MODE_BIT    0
#define  LP_GPIO_CONF0_DIR_BIT     2
#define  LP_GPIO_CONF0_GPI_BIT     30
#define  LP_GPIO_CONF0_GPO_BIT     31

static int pch_is_lp(void)
{
	switch (pci_read_config16(PCI_DEV(0, 0x1f, 0), REG_DEVICE_ID)) {
	case 0x9c41: /* Haswell ULT Sample */
		return 1;
	}
	return 0;
}

static uint32_t pch_gpiobase(void)
{
	static const uint32_t dev = PCI_DEV(0, 0x1f, 0);
	static const uint8_t pci_cfg_gpiobase = 0x48;

	static uint32_t base = ~(uint32_t)0;
	if (base != ~(uint32_t)0)
		return base;

	base = pci_read_config32(dev, pci_cfg_gpiobase);
	// Drop the IO space bit.
	base &= ~0x1;
	return base;
}

static int pch_lp_gpio_set(unsigned num, int bit, int val)
{
	uint16_t addr = pch_gpiobase() + LP_GPIO_CONF0(num);
	uint32_t conf0 = inl(addr);
	if (val)
		conf0 |= (1 << bit);
	else
		conf0 &= ~(1 << bit);
	outl(addr, conf0);
	return 0;
}

static int pch_gpio_set(unsigned num, uint8_t *bases, int val)
{
	int bank = num / 32;
	int bit = num % 32;
	if (bank >= NUM_GPIO_BANKS) {
		printf("GPIO bank %d out of bounds.\n", bank);
		return -1;
	}
	uint16_t addr = pch_gpiobase() + bases[bank];
	uint32_t reg = inl(addr);
	reg = (reg & ~(1 << bit)) | ((val & 1) << bit);
	outl(addr, reg);
	return 0;
}

static int pch_lp_gpio_get(unsigned num, int bit)
{
	uint16_t addr = pch_gpiobase() + LP_GPIO_CONF0(num);
	uint32_t conf0 = inl(addr);
	return !!(conf0 & (1 << bit));
}

static int pch_gpio_get(unsigned num, uint8_t *bases)
{
	int bank = num / 32;
	int bit = num % 32;
	if (bank >= NUM_GPIO_BANKS) {
		printf("GPIO bank %d out of bounds.\n", bank);
		return -1;
	}
	uint32_t reg = inl(pch_gpiobase() + bases[bank]);
	return (reg >> bit) & 1;
}

int pch_gpio_use(unsigned num, unsigned use)
{
	if (pch_is_lp())
		return pch_lp_gpio_set(num, LP_GPIO_CONF0_MODE_BIT, use);
	else
		return pch_gpio_set(num, gpio_use, use);
}

int pch_gpio_direction(unsigned num, unsigned input)
{
	if (pch_is_lp())
		return pch_lp_gpio_set(num, LP_GPIO_CONF0_DIR_BIT, input);
	else
		return pch_gpio_set(num, gpio_io, input);
}

int pch_gpio_get_value(unsigned num)
{
	if (pch_is_lp())
		return pch_lp_gpio_get(num, LP_GPIO_CONF0_GPI_BIT);
	else
		return pch_gpio_get(num, gpio_lvl);
}

int pch_gpio_set_value(unsigned num, unsigned value)
{
	if (pch_is_lp())
		return pch_lp_gpio_set(num, LP_GPIO_CONF0_GPO_BIT, value);
	else
		return pch_gpio_set(num, gpio_lvl, value);
}
