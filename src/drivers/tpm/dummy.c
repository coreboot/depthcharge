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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>

#include "drivers/tpm/tpm.h"

uint32_t tis_wait_reg(uint8_t reg, uint8_t locality, uint8_t mask,
		      uint8_t expected)
{
	printf("%s not implemented.\n", __func__);
	halt();
}

int tis_command_ready(uint8_t locality)
{
	printf("%s not implemented.\n", __func__);
	halt();
}

uint32_t tis_probe(void)
{
	printf("%s not implemented.\n", __func__);
	halt();
}

uint32_t tis_senddata(const uint8_t *const data, uint32_t len)
{
	printf("%s not implemented.\n", __func__);
	halt();
}

uint32_t tis_readresponse(uint8_t *buffer, uint32_t *len)
{
	printf("%s not implemented.\n", __func__);
	halt();
}

uint32_t tpm_read(int locality, uint32_t reg)
{
	printf("%s not implemented.\n", __func__);
	halt();
}

void tpm_write(uint32_t value, int locality,  uint32_t reg)
{
	printf("%s not implemented.\n", __func__);
	halt();
}
