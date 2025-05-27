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

void ndelay(uint64_t ns)
{
	mock_time_ms += (ns / NSECS_PER_MSEC);
}
