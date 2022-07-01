// SPDX-License-Identifier: GPL-2.0

#include <diag/common.h>
#include <diag/storage_test.h>
#include <libpayload.h>
#include <tests/test.h>

int diag_storage_test_supported(void)
{
	return mock();
}

DiagTestResult diag_dump_storage_test_log(char *buf, const char *end)
{
	snprintf(buf, end - buf, "mock");
	return mock();
}

DiagTestResult diag_storage_test_control(enum BlockDevTestOpsType ops)
{
	return mock();
}
