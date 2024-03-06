// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <tests/test.h>

char *cbmem_console_snapshot(void)
{
	static char cbmem_console_buf[64];
	int ret = snprintf(cbmem_console_buf, sizeof(cbmem_console_buf),
			   "mock cbmem console snapshot");
	if (ret >= sizeof(cbmem_console_buf))
		fail_msg("cbmem_console_buf too small");
	return cbmem_console_buf;
}
