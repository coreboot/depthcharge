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

#include <assert.h>
#include <libpayload.h>
#include <vb2_api.h>

#include "drivers/flash/flash.h"
#include "image/fmap.h"
#include "image/symbols.h"
#include "vboot/stages.h"
#include "vboot/util/commonparams.h"

struct vb2_context *vboot_get_context(void)
{
	static struct vb2_context *ctx;
	vb2_error_t rv;

	if (ctx)
		return ctx;

	die_if(lib_sysinfo.vboot_workbuf == 0,
	       "vboot workbuf pointer is not set\n");

	/* Use the firmware verification workbuf from coreboot. */
	rv = vb2api_reinit(phys_to_virt(lib_sysinfo.vboot_workbuf), &ctx);

	die_if(rv, "vboot workbuf could not be initialized, error: %#x\n", rv);

	return ctx;
}
