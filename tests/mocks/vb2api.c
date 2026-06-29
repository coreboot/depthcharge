// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <mocks/vb2api.h>
#include <vb2_api.h>

uint32_t mock_locale_id;

vb2_gbb_flags_t vb2api_gbb_get_flags(struct vb2_context *ctx)
{
	return mock_type(vb2_gbb_flags_t);
}

uint32_t vb2api_get_locale_id(struct vb2_context *ctx)
{
	return mock_locale_id;
}

int vb2api_diagnostic_ui_enabled(struct vb2_context *ctx)
{
	return mock();
}

vb2_error_t vb2api_enable_developer_mode(struct vb2_context *ctx)
{
	function_called();
	return VB2_SUCCESS;
}

vb2_error_t vb2api_nv_set(struct vb2_context *ctx, enum vb2_nv_param param, uint32_t value)
{
	check_expected(param);
	check_expected(value);

	if (param == VB2_NV_LOCALIZATION_INDEX)
		mock_locale_id = value;

	return VB2_SUCCESS;
}

enum vb2_dev_default_boot_target
vb2api_get_dev_default_boot_target(struct vb2_context *ctx)
{
	return mock_type(enum vb2_dev_default_boot_target);
}

vb2_error_t vb2api_disable_developer_mode(struct vb2_context *ctx)
{
	function_called();
	return VB2_SUCCESS;
}
