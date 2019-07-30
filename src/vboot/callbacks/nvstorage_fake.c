/*
 * Copyright 2012 Google Inc.
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

#include <libpayload.h>
#include <vboot_api.h>
#include <vb2_api.h>

static u8 fake_nvram[VBNV_BLOCK_SIZE];

vb2_error_t VbExNvStorageRead(uint8_t* buf)
{
	memcpy(buf, fake_nvram, sizeof(VBNV_BLOCK_SIZE));
	return VB2_SUCCESS;
}

vb2_error_t VbExNvStorageWrite(const uint8_t* buf)
{
	memcpy(fake_nvram, buf, sizeof(VBNV_BLOCK_SIZE));
	return VB2_SUCCESS;
}
