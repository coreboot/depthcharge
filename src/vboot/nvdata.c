/*
 * Copyright 2015 Google Inc.
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

#define NEED_VB20_INTERNALS  /* Poking around inside nvdata fields */

#include <libpayload.h>
#include <vb2_api.h>

#include "drivers/ec/cros/ec.h"
#include "vboot/nvdata.h"
#include "vboot/util/commonparams.h"

/*
 * TODO(chromium:1006689): DO NOT USE THIS FUNCTION: it is being deprecated.
 * depthcharge takes care of reading/writing nvdata, but the format and layout
 * of fields is internal to vboot, and should not be accessed outside of vboot.
 */
void nvdata_write_field_DO_NOT_USE(uint32_t field, uint32_t val)
{
	struct vb2_context *ctx = vboot_get_context();
	vb2_nv_set(ctx, field, val);
	nvdata_write(ctx->nvdata);
}

vb2_error_t nvdata_read(uint8_t *buf)
{
	if (CONFIG(NVDATA_CMOS))
		return nvdata_cmos_read(buf);
	else if (CONFIG(NVDATA_CROS_EC))
		return nvdata_cros_ec_read(buf);
	else if (CONFIG(NVDATA_DISK))
		return nvdata_disk_read(buf);
	else if (CONFIG(NVDATA_FLASH))
		return nvdata_flash_read(buf);
	else
		return nvdata_fake_read(buf);
}

vb2_error_t nvdata_write(const uint8_t *buf)
{
	if (CONFIG(NVDATA_CMOS))
		return nvdata_cmos_write(buf);
	else if (CONFIG(NVDATA_CROS_EC))
		return nvdata_cros_ec_write(buf);
	else if (CONFIG(NVDATA_DISK))
		return nvdata_disk_write(buf);
	else if (CONFIG(NVDATA_FLASH))
		return nvdata_flash_write(buf);
	else
		return nvdata_fake_write(buf);
}
