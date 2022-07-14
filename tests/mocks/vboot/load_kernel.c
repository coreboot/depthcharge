// SPDX-License-Identifier: GPL-2.0

#include <vb2_api.h>

#include "tests/test.h"
#include "tests/vboot/common.h"
#include "vboot/load_kernel.h"

vb2_error_t vboot_load_kernel(struct vb2_context *ctx, blockdev_type_t type,
			      struct vb2_kernel_params *kparams)
{
	/*
	 * Give removable disks priority. This shouldn't matter for now because
	 * only one disk flag is passed for each call.
	 */
	assert_non_null(kparams);
	if (type == BLOCKDEV_REMOVABLE)
		return _load_external_disk();
	else if (type == BLOCKDEV_FIXED)
		return _load_internal_disk();
	fail_msg("%s called with unsupported type %d", __func__, type);
	/* Never reach here */
	return VB2_SUCCESS;
}

vb2_error_t vboot_load_minios_kernel(struct vb2_context *ctx,
				     uint32_t minios_flags,
				     struct vb2_kernel_params *kparams)
{
	assert_non_null(kparams);
	check_expected(minios_flags);
	return mock_type(vb2_error_t);
}
