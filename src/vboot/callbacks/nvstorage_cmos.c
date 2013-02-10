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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA 02111-1307 USA
 */

#include <libpayload.h>
#include <vboot_api.h>

VbError_t VbExNvStorageRead(uint8_t* buf)
{
	if (lib_sysinfo.vbnv_start == (uint32_t)(-1)) {
		printf("%s:%d - vbnv address undefined\n",
		       __FUNCTION__, __LINE__);
		return VBERROR_INVALID_PARAMETER;
	}

	for (int i = 0; i < lib_sysinfo.vbnv_size; i++)
		buf[i] = nvram_read(lib_sysinfo.vbnv_start + i);

	return VBERROR_SUCCESS;
}

VbError_t VbExNvStorageWrite(const uint8_t* buf)
{
	if (lib_sysinfo.vbnv_start == (uint32_t)(-1)) {
		printf("%s:%d - vbnv address undefined\n",
		       __FUNCTION__, __LINE__);
		return VBERROR_INVALID_PARAMETER;
	}

	for (int i = 0; i < lib_sysinfo.vbnv_size; i++)
		nvram_write(buf[i], lib_sysinfo.vbnv_start + i);

	return VBERROR_SUCCESS;
}
