// SPDX-License-Identifier: GPL-2.0

#include <stdint.h>
#include <vb2_api.h>

#include "vboot/load_kernel.h"

vb2_error_t vboot_load_kernel(struct vb2_context *ctx, blockdev_type_t type,
			      struct vb2_kernel_params *kparams)
{
	return VB2_SUCCESS;
}

vb2_error_t vboot_load_nbr_kernel(struct vb2_context *ctx,
				  uint32_t nbr_flags,
				  struct vb2_kernel_params *kparams)
{
	return VB2_SUCCESS;
}
