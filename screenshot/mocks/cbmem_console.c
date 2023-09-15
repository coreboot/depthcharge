// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

char *cbmem_console_snapshot(void)
{
	return "\n\nline 0\nline 1";
}
