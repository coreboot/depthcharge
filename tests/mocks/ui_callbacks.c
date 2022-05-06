// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <tests/vboot/ui/common.h>
#include <mocks/callbacks.h>
#include <vb2_api.h>

uint32_t mock_time_ms;

uint32_t vb2ex_mtime(void)
{
	return mock_time_ms;
}

void vb2ex_msleep(uint32_t msec)
{
	mock_time_ms += msec;
}

vb2_error_t vb2ex_diag_get_storage_test_log(const char **log)
{
	*log = "mock";
	return mock_type(vb2_error_t);
}
