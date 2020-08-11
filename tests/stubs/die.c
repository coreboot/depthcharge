// SPDX-License-Identifier: GPL-2.0

#include <libpayload.h>

/* Satisfy libpayload requirements */
void die_work(const char *file, const char *func, const int line,
		  const char *fmt, ...)
{
	va_list args;

	printf("%s:%d %s(): ", file, line, func);
	va_start(args, fmt);
	vprintf(fmt, args);
	va_end(args);
	abort();
}
