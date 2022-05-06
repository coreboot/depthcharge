// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <mocks/cbmem_console.h>

char cbmem_console_buf[CBMEM_CONSOLE_BUFFER_LEN];
int cbmem_console_snapshots_count;

/*
 * Each time this function is called, `cbmem_console_snapshots_count` will be
 * incremented by one. Therefore the returned string will be different for each
 * call.
 */
char *cbmem_console_snapshot(void)
{
	cbmem_console_snapshots_count++;
	snprintf(cbmem_console_buf, sizeof(cbmem_console_buf), "%d",
		 cbmem_console_snapshots_count);
	return cbmem_console_buf;
}
