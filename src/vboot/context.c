// SPDX-License-Identifier: GPL-2.0

#include <assert.h>
#include <libpayload.h>
#include <vb2_api.h>

#include "vboot/context.h"

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
