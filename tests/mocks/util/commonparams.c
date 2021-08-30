// SPDX-License-Identifier: GPL-2.0

#include <vb2_api.h>
#include <tests/test.h>
#include <tests/vboot/common.h>
#include <mocks/util/commonparams.h>

int reset_mock_workbuf;
char mock_vboot_workbuf[WORKBUF_SIZE];
struct vb2_context *mock_vboot_context;

struct vb2_context *vboot_get_context(void)
{
	if (!mock_vboot_context || reset_mock_workbuf) {
		reset_mock_workbuf = 0;

		memset(&mock_vboot_workbuf, 0, sizeof(mock_vboot_workbuf));

		ASSERT_VB2_SUCCESS(vb2api_init(mock_vboot_workbuf,
					       sizeof(mock_vboot_workbuf),
					       &mock_vboot_context));
		assert_non_null(mock_vboot_context);
	}

	return mock_vboot_context;
}
