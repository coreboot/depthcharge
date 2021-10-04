// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/common.h>
#include <vboot_api.h>

vb2_error_t VbTryLoadKernel(struct vb2_context *ctx, uint32_t disk_flags)
{
	/*
	 * Give removable disks priority. This shouldn't matter for now because
	 * only one disk flag is passed for each call.
	 */
	if (disk_flags & VB_DISK_FLAG_REMOVABLE)
		return _try_load_external_disk();
	else if (disk_flags & VB_DISK_FLAG_FIXED)
		return _try_load_internal_disk();
	fail_msg("%s called with unsupported disk_flags %#x",
		 __func__, disk_flags);
	/* Never reach here */
	return VB2_SUCCESS;
}

vb2_error_t VbTryLoadMiniOsKernel(struct vb2_context *ctx,
				  uint32_t minios_flags)
{
	return mock_type(vb2_error_t);
}
