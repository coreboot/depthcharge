// SPDX-License-Identifier: GPL-2.0

#include <stdint.h>
#include <vb2_api.h>

#include "vboot/load_kernel.h"

vb2_error_t vboot_load_kernel(struct vb2_context *ctx, uint32_t disk_flags,
			      struct vb2_kernel_params *kparams)
{
	return VB2_SUCCESS;
}

vb2_error_t vboot_load_minios_kernel(struct vb2_context *ctx,
				     uint32_t minios_flags,
				     struct vb2_kernel_params *kparams)
{
	return VB2_SUCCESS;
}
