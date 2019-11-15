// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright 2019 Google LLC.
 *
 * Stub functions for when auxfw.c is not compiled in.
 */

#include <libpayload.h>
#include <vboot_api.h>
#include <vb2_api.h>

vb2_error_t vb2ex_auxfw_check(enum vb2_auxfw_update_severity *severity)
{
	return VB2_SUCCESS;
}

vb2_error_t vb2ex_auxfw_update(void)
{
	return VB2_SUCCESS;
}

vb2_error_t vb2ex_auxfw_finalize(struct vb2_context *ctx)
{
	return VB2_SUCCESS;
}
