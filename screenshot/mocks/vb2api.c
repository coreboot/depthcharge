// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <vb2_api.h>

#include "vboot/ui.h"

vb2_error_t vb2api_gbb_read_hwid(struct vb2_context *ctx, char *hwid,
				 uint32_t *size)
{
	const char *id = "EXAMPLE";
	strncpy(hwid, id, *size - 1);
	hwid[*size - 1] = '\0';
	*size = strlen(id);
	return VB2_SUCCESS;
}

int vb2api_phone_recovery_enabled(struct vb2_context *ctx)
{
	return 0;
}

int vb2api_phone_recovery_ui_enabled(struct vb2_context *ctx)
{
	return 0;
}

int vb2api_diagnostic_ui_enabled(struct vb2_context *ctx)
{
	return 0;
}
