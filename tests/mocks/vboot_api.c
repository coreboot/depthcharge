// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <vboot_api.h>

vb2_error_t VbTryLoadKernel(struct vb2_context *ctx, uint32_t disk_flags)
{
	check_expected(disk_flags);
	return mock_type(vb2_error_t);
}

vb2_error_t VbTryLoadMiniOsKernel(struct vb2_context *ctx,
				  uint32_t minios_flags)
{
	return mock_type(vb2_error_t);
}
