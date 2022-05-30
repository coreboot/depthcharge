// SPDX-License-Identifier: GPL-2.0

#include <drivers/storage/info.h>
#include <libpayload.h>

char *dump_all_health_info(char *buf, const char *end)
{
	snprintf(buf, end - buf, "mock");
	return buf;
}
