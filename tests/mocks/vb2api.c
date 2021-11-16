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

void vb2api_set_locale_id(struct vb2_context *ctx, uint32_t locale_id)
{
	mock_locale_id = locale_id;
}

int vb2api_phone_recovery_ui_enabled(struct vb2_context *ctx)
{
	return mock();
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

void vb2api_request_diagnostics(struct vb2_context *ctx)
{
	function_called();
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
