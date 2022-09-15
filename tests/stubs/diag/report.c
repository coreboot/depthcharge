// SPDX-License-Identifier: GPL-2.0

#include "diag/common.h"

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
