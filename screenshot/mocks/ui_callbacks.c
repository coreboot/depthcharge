// SPDX-License-Identifier: GPL-2.0

#include <vb2_api.h>

const char *vb2ex_get_debug_info(struct vb2_context *ctx)
{
	return "mock";
}

const char *vb2ex_get_firmware_log(int reset)
{
	return "mock";
}
