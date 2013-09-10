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

#include <assert.h>
#include <libpayload.h>
#include <stdint.h>

#include "netboot/params.h"

static NetbootParam netboot_params[NetbootParamIdMax];

const char netboot_sig[] = "netboot";

NetbootParam *netboot_params_val(NetbootParamId param)
{
	assert(param < NetbootParamIdMax);
	return &netboot_params[param];
}

static uintptr_t size32(uintptr_t size)
{
	return (size + sizeof(uint32_t) - 1) / sizeof(uint32_t);
}

int netboot_params_init(void *data, uintptr_t size)
{
	assert(data);

	memset(netboot_params, 0, sizeof(netboot_params));

	if (size < sizeof(netboot_sig))
		return 1;

	if (memcmp(data, netboot_sig, sizeof(netboot_sig)))
		return 1;

	uintptr_t max_pos = size / sizeof(uint32_t);
	uint32_t *data32 = (uint32_t *)data;
	uintptr_t pos = size32(sizeof(netboot_sig));

	if (pos >= max_pos)
		return 1;

	uint32_t count = data32[pos++];

	while (count--) {
		uint32_t val_type = data32[pos++];
		if (pos >= max_pos)
			return 1;
		uint32_t val_size = data32[pos++];
		if (pos >= max_pos)
			return 1;
		void *val_data = &data32[pos];
		pos += size32(val_size);
		if (pos >= max_pos)
			return 1;

		NetbootParam *param = &netboot_params[val_type];
		param->data = val_data;
		param->size = val_size;
	}
	return 0;
}
