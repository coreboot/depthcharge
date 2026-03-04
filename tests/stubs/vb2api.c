// SPDX-License-Identifier: GPL-2.0

#include <string.h>
#include <vb2_api.h>

vb2_error_t vb2api_gbb_read_hwid(struct vb2_context *ctx, char *hwid,
				 uint32_t *size)
{
	static const char mocked_hwid[] = "MOCK HWID";

	if (*size < sizeof(mocked_hwid))
		return VB2_ERROR_INVALID_PARAMETER;

	strcpy(hwid, mocked_hwid);
	*size = sizeof(mocked_hwid);
	return VB2_SUCCESS;
}
