// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>
#include <tests/test.h>

#ifndef __TEST_PRINT__
#define __TEST_PRINT__ 0
#endif

void console_write(const void *buffer, size_t count)
{
	if (!__TEST_PRINT__)
		return;

	/*
	 * CMocka 1.1.5 has buffer of 1024 chars inside print_message().
	 * Print the characters one by one to avoid truncation.
	 */
	const char *ptr;
	for (ptr = buffer; (void *)ptr < buffer + count; ptr++)
		print_message("%c", *ptr);
}
