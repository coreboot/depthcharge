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

#define PRINT_BYTES(title, value) do { \
		int i; \
		printf(title); \
		printf(":"); \
		for (i = 0; i < sizeof(*(value)); i++) \
			printf(" %02x", *((uint8_t *)(value) + i)); \
		printf("\n"); \
	} while (0)

/*
 * TODO(chromium:1006689): DO NOT USE THIS FUNCTION: it is being deprecated.
 * depthcharge takes care of reading/writing nvdata, but the format and layout
 * of fields is internal to vboot, and should not be accessed outside of vboot.
 */
void nvdata_write_field_DO_NOT_USE(uint32_t field, uint32_t val)
{
	struct vb2_context *ctx = vboot_get_context();
	vb2_nv_set(ctx, field, val);
	nvdata_write(ctx);
}

vb2_error_t nvdata_read(struct vb2_context *ctx)
{
	if (CONFIG(NVDATA_CMOS))
		return nvdata_cmos_read(ctx->nvdata);
	else if (CONFIG(NVDATA_CROS_EC))
		return nvdata_cros_ec_read(ctx->nvdata);
	else if (CONFIG(NVDATA_FLASH))
		return nvdata_flash_read(ctx->nvdata);
	else
		die("no nvdata backend selected\n");

	PRINT_BYTES("read nvdata", &ctx->nvdata);
}

vb2_error_t nvdata_write(struct vb2_context *ctx)
{
	vb2_error_t ret;

	if (!(ctx->flags & VB2_CONTEXT_NVDATA_CHANGED)) {
		printf("nvdata unchanged\n");
		return VB2_SUCCESS;
	}

	PRINT_BYTES("write nvdata", &ctx->nvdata);

	if (CONFIG(NVDATA_CMOS))
		ret = nvdata_cmos_write(ctx->nvdata);
	else if (CONFIG(NVDATA_CROS_EC))
		ret = nvdata_cros_ec_write(ctx->nvdata);
	else if (CONFIG(NVDATA_FLASH))
		ret = nvdata_flash_write(ctx->nvdata);
	else
		die("no nvdata backend selected\n");

	if (!ret)
		ctx->flags &= ~VB2_CONTEXT_NVDATA_CHANGED;

	return ret;
}
