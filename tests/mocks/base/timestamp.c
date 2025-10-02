// SPDX-License-Identifier: GPL-2.0

#include "base/timestamp.h"
#include <tests/test.h>

void timestamp_mix_in_randomness(u8 *buffer, size_t size)
{
	function_called();
	check_expected_ptr(buffer);
	check_expected(size);
}
