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

#define NEED_VB20_INTERNALS  /* Poking around inside NV storage fields */

#include <libpayload.h>
#include <vb2_api.h>
#include <vboot_api.h>

#include "vboot/vbnv.h"

uint32_t vbnv_read(uint32_t flag)
{
	struct vb2_context ctx;

	memset(&ctx, 0, sizeof(ctx));
	VbExNvStorageRead(ctx.nvdata);
	vb2_nv_init(&ctx);
	return vb2_nv_get(&ctx, flag);
}

void vbnv_write(uint32_t flag, uint32_t val)
{
	struct vb2_context ctx;

	memset(&ctx, 0, sizeof(ctx));
	VbExNvStorageRead(ctx.nvdata);
	vb2_nv_init(&ctx);
	vb2_nv_set(&ctx, flag, val);
	if (ctx.flags & VB2_CONTEXT_NVDATA_CHANGED)
		VbExNvStorageWrite(ctx.nvdata);
}


