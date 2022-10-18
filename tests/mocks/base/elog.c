// SPDX-License-Identifier: GPL-2.0

#include "base/elog.h"
#include "tests/test.h"

elog_error_t elog_add_event_raw(uint8_t event_type, void *data,
				uint8_t data_size)
{
	check_expected(event_type);
	check_expected(data);
	check_expected(data_size);
	return ELOG_SUCCESS;
}
