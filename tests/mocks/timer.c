// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <stddef.h>

#include "tests/test.h"

uint64_t timer_raw_value(void)
{
	return mock_type(uint64_t);
}
