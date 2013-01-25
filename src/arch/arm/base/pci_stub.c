/*
 * Copyright (c) 2013 The Chromium OS Authors.
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

#include <libpayload.h>
#include <stdint.h>

uint8_t pci_read_config8(uint32_t device, uint16_t reg)
{
	return 0xFF;
}

uint16_t pci_read_config16(uint32_t device, uint16_t reg)
{
	return 0xFFFF;
}

uint32_t pci_read_config32(uint32_t device, uint16_t reg)
{
	return 0xFFFFFFFF;
}

void pci_write_config8(uint32_t device, uint16_t reg, uint8_t val)
{
}

void pci_write_config16(uint32_t device, uint16_t reg, uint16_t val)
{
}

void pci_write_config32(uint32_t device, uint16_t reg, uint32_t val)
{
}
