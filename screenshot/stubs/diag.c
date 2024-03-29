// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

#include "diag/common.h"
#include "diag/health_info.h"
#include "diag/memory.h"
#include "diag/storage_test.h"

char *dump_all_health_info(char *buf, const char *end)
{
	snprintf(buf, end - buf, "mock health info");
	return buf;
}

uint32_t diag_storage_test_supported(void)
{
	return 1u;
}

DiagTestResult diag_dump_storage_test_log(char *buf, const char *end)
{
	snprintf(buf, end - buf, "mock storage test log");
	return DIAG_TEST_PASSED;
}

DiagTestResult diag_storage_test_control(enum BlockDevTestOpsType ops)
{
	return DIAG_TEST_PASSED;
}

DiagTestResult memory_test_init(MemoryTestMode mode)
{
	return DIAG_TEST_PASSED;
}

DiagTestResult memory_test_run(const char **buf)
{
	static const char *log = "mock memory test log";
	*buf = log;
	return DIAG_TEST_PASSED;
}

int diag_report_start_test(uint8_t type)
{
	return 0;
}

int diag_report_end_test(uint8_t result)
{
	return 0;
}

size_t diag_report_dump(void *buf, size_t size)
{
	return 1;
}
