/* SPDX-License-Identifier: GPL-2.0 */

#define CBMEM_CONSOLE_BUFFER_LEN 16

/*
 * Variables for cbmem_console_snapshot. See comments in mocks/cbmem_console.c
 * for more information.
 */
extern char cbmem_console_buf[CBMEM_CONSOLE_BUFFER_LEN];
extern int cbmem_console_snapshots_count;
