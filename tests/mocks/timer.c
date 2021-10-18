// SPDX-License-Identifier: GPL-2.0

#include <tests/test.h>
#include <libpayload.h>
#include <stddef.h>

__attribute__((weak))
uint64_t timer_raw_value(void)
{
	return 0;
}

__attribute__((weak))
uint64_t timer_hz(void)
{
	/* Libpayload requires it to be at least 1MHz.
	   Keep it that way not to break anything */
	return 1 * MHz;
}
