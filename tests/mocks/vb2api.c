// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <vb2_api.h>

vb2_gbb_flags_t vb2api_gbb_get_flags(struct vb2_context *ctx)
{
	return mock_type(vb2_gbb_flags_t);
}

uint32_t vb2api_get_locale_id(struct vb2_context *ctx)
{
	return mock_type(uint32_t);
}
